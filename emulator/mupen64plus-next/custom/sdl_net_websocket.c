#define _GNU_SOURCE
#include "sdl_net_websocket.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/val.h>
#else
#error "sdl_net_websocket.c is intended to be compiled only for Emscripten builds."
#endif

/* JS glue that manages simple WebSocket objects and per-socket inbox queues.
 * The JS helpers below:
 *  - js_ws_open(url_ptr) -> socket id >= 0, or -1 on error
 *  - js_ws_send(id, ptr, len) -> bytes_sent or -1
 *  - js_ws_receive(id, buf_ptr, buf_len) -> bytes_received, 0 none, -2 closed, -1 error
 *  - js_ws_close(id)
 *  - js_ws_last_error(id) -> ptr to malloc'd UTF8 string on WASM heap (caller must not free)
 */

EM_JS(int, js_ws_open, (const char* url_ptr), {
  try {
    var url = UTF8ToString(url_ptr);
    if (!Module._wsSockets) {
      Module._wsSockets = {};
      Module._wsInbox = {};
      Module._wsNextId = 1;
      Module._wsErrors = {};
    }
    var ws = new WebSocket(url);
    ws.binaryType = 'arraybuffer';
    var id = Module._wsNextId++;
    Module._wsSockets[id] = ws;
    Module._wsInbox[id] = [];
    Module._wsErrors[id] = null;
    ws.onmessage = function(ev) {
      try {
        if (ev && ev.data) {
          var arr = new Uint8Array(ev.data);
          Module._wsInbox[id].push(arr);
        }
      } catch(e) {
        Module._wsErrors[id] = String(e);
      }
    };
    ws.onclose = function(ev) {
      Module._wsInbox[id].push(null); // closed sentinel
    };
    ws.onerror = function(ev) {
      Module._wsErrors[id] = "WebSocket error";
    };
    return id;
  } catch(e) {
    if (!Module._wsErrors) Module._wsErrors = {};
    Module._wsErrors[0] = String(e);
    return -1;
  }
});

EM_JS(int, js_ws_send, (int id, const unsigned char* ptr, int len), {
  if (!Module._wsSockets || !Module._wsSockets[id]) return -1;
  try {
    var arr = HEAPU8.subarray(ptr, ptr + len);
    Module._wsSockets[id].send(arr);
    return len;
  } catch(e) {
    if (!Module._wsErrors) Module._wsErrors = {};
    Module._wsErrors[id] = String(e);
    return -1;
  }
});

EM_JS(int, js_ws_receive, (int id, unsigned char* buf_ptr, int buf_len), {
  if (!Module._wsInbox || !Module._wsInbox[id]) return 0;
  var queue = Module._wsInbox[id];
  if (!queue.length) return 0;
  var totalCopied = 0;
  while (queue.length && totalCopied < buf_len) {
    var chunk = queue[0];
    if (chunk === null) {
      queue.shift();
      if (totalCopied === 0) return -2;
      return totalCopied;
    }
    var toCopy = Math.min(buf_len - totalCopied, chunk.length);
    for (var i = 0; i < toCopy; i++) {
      HEAPU8[buf_ptr + totalCopied + i] = chunk[i];
    }
    if (toCopy < chunk.length) {
      Module._wsInbox[id][0] = chunk.subarray(toCopy);
    } else {
      queue.shift();
    }
    totalCopied += toCopy;
  }
  return totalCopied;
});

EM_JS(void, js_ws_close, (int id), {
  if (Module._wsSockets && Module._wsSockets[id]) {
    try { Module._wsSockets[id].close(); } catch(e) {}
    delete Module._wsSockets[id];
    delete Module._wsInbox[id];
    delete Module._wsErrors[id];
  }
});

EM_JS(const char*, js_ws_last_error, (int id), {
  if (!Module._wsErrors) return 0;
  var s = Module._wsErrors[id] || Module._wsErrors[0] || null;
  if (!s) return 0;
  var len = lengthBytesUTF8(s) + 1;
  var p = _malloc(len);
  stringToUTF8(s, p, len);
  return p;
});

/* C state */
static const char* last_err_ptr = NULL;

/* Public API implementation */

int SDLNet_Init(void)
{
    return 0;
}

void SDLNet_Quit(void)
{
}

int SDLNet_ResolveHost(void *ip, const char *host, unsigned short port)
{
    (void)ip; (void)host; (void)port;
    /* netplay typically passes host strings; for browser we don't do DNS here. */
    return 0;
}

TCPsocket SDLNet_TCP_Open(const char *host_or_url)
{
    if (!host_or_url) return -1;

    /* If value already looks like ws:// or wss://, use as-is; otherwise assume host:port -> ws://host:port/ */
    if (strstr(host_or_url, "ws://") == host_or_url || strstr(host_or_url, "wss://") == host_or_url) {
        int id = js_ws_open(host_or_url);
        if (id < 0) last_err_ptr = js_ws_last_error(0);
        return id;
    } else {
        char urlbuf[1024];
        if (strlen(host_or_url) >= sizeof(urlbuf) - 10) return -1;
        snprintf(urlbuf, sizeof(urlbuf), "ws://%s/", host_or_url);
        int id = js_ws_open(urlbuf);
        if (id < 0) last_err_ptr = js_ws_last_error(0);
        return id;
    }
}

int SDLNet_TCP_Send(TCPsocket s, const void *data, int len)
{
    if (s < 0) return -1;
    int sent = js_ws_send(s, (const unsigned char*)data, len);
    if (sent < 0) last_err_ptr = js_ws_last_error(s);
    return sent;
}

/* returns:
 *  >0 bytes read
 *   0 no data available
 *  -2 socket closed sentinel
 *  -1 error
 */
int SDLNet_TCP_Recv(TCPsocket s, void *data, int maxlen)
{
    if (s < 0) return -1;
    int r = js_ws_receive(s, (unsigned char*)data, maxlen);
    if (r == -2) return -2;
    if (r < 0) {
        last_err_ptr = js_ws_last_error(s);
        return -1;
    }
    return r;
}

void SDLNet_TCP_Close(TCPsocket s)
{
    if (s < 0) return;
    js_ws_close(s);
}

const char* SDLNet_CheckError(void)
{
    return last_err_ptr;
}
