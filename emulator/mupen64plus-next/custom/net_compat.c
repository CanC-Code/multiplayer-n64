/* net_compat.c
 *
 * Minimal cross-platform networking shim:
 *  - In Emscripten: WebSocket client via emscripten_websocket_* API
 *  - Native: Simple TCP socket (POSIX)
 *
 * Notes:
 *  - Browser/emscripten uses WebSocket binary frames.
 *  - This is intentionally minimal. Production-ready code should
 *    do better error handling, reconnection, fragmentation handling,
 *    and conform to specific netplay packet formats.
 */

#include "net_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
/* Emscripten WebSocket implementation */
#include <emscripten/websocket.h>
#include <emscripten/emscripten.h>

#define MAX_MSGS 256
#define MAX_MSG_SIZE 65536

struct net_handle_t {
    EMSCRIPTEN_WEBSOCKET_T ws;
    int connected;
    /* simple ring of messages */
    unsigned char *msgs[MAX_MSGS];
    size_t msg_len[MAX_MSGS];
    int msg_head; /* next to read */
    int msg_tail; /* next to write */
};

static int g_net_inited = 0;

net_status_t net_init(void) {
    if (g_net_inited) return NET_OK;
    if (!emscripten_websocket_is_supported()) {
        return NET_ERR;
    }
    g_net_inited = 1;
    return NET_OK;
}

void net_shutdown(void) {
    g_net_inited = 0;
}

static int queue_push(struct net_handle_t *h, const unsigned char *data, size_t len) {
    if (!h) return 0;
    int next = (h->msg_tail + 1) % MAX_MSGS;
    if (next == h->msg_head) {
        /* queue full, drop oldest */
        if (h->msgs[h->msg_head]) {
            free(h->msgs[h->msg_head]);
            h->msgs[h->msg_head] = NULL;
        }
        h->msg_head = (h->msg_head + 1) % MAX_MSGS;
    }
    unsigned char *buf = malloc(len);
    if (!buf) return 0;
    memcpy(buf, data, len);
    h->msgs[h->msg_tail] = buf;
    h->msg_len[h->msg_tail] = len;
    h->msg_tail = next;
    return 1;
}

static int queue_pop(struct net_handle_t *h, unsigned char *buf, size_t bufsize, size_t *out_len) {
    if (!h || h->msg_head == h->msg_tail) {
        *out_len = 0;
        return 0;
    }
    size_t msglen = h->msg_len[h->msg_head];
    if (msglen > bufsize) {
        /* message too large for provided buffer; drop message and report error */
        free(h->msgs[h->msg_head]);
        h->msgs[h->msg_head] = NULL;
        h->msg_head = (h->msg_head + 1) % MAX_MSGS;
        *out_len = 0;
        return 0;
    }
    memcpy(buf, h->msgs[h->msg_head], msglen);
    *out_len = msglen;
    free(h->msgs[h->msg_head]);
    h->msgs[h->msg_head] = NULL;
    h->msg_head = (h->msg_head + 1) % MAX_MSGS;
    return 1;
}

/* WebSocket callbacks */
static EM_BOOL ws_open(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData) {
    (void)eventType; (void)e;
    struct net_handle_t *h = (struct net_handle_t*)userData;
    if (h) h->connected = 1;
    return EM_TRUE;
}
static EM_BOOL ws_close(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData) {
    (void)eventType; (void)e;
    struct net_handle_t *h = (struct net_handle_t*)userData;
    if (h) h->connected = 0;
    return EM_TRUE;
}
static EM_BOOL ws_error(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData) {
    (void)eventType; (void)e; (void)userData;
    return EM_TRUE;
}
static EM_BOOL ws_message(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData) {
    (void)eventType;
    struct net_handle_t *h = (struct net_handle_t*)userData;
    if (!h) return EM_FALSE;
    if (!e->isText && e->numBytes > 0) {
        queue_push(h, (const unsigned char*)e->data, (size_t)e->numBytes);
    } else if (e->isText && e->numBytes > 0) {
        /* If server sends text frames, treat as UTF-8 binary */
        queue_push(h, (const unsigned char*)e->data, (size_t)e->numBytes);
    }
    return EM_TRUE;
}

net_handle_t *net_connect(const char *url) {
    if (!g_net_inited) {
        if (net_init() != NET_OK) return NULL;
    }
    if (!url) return NULL;

    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url;
    attr.protocols = NULL;
    attr.createOnMainThread = EM_TRUE;

    EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&attr);
    if (!ws) return NULL;

    struct net_handle_t *h = calloc(1, sizeof(*h));
    if (!h) {
        emscripten_websocket_close(ws, 1000, "oom");
        return NULL;
    }
    h->ws = ws;
    h->connected = 0;
    h->msg_head = h->msg_tail = 0;
    for (int i=0;i<MAX_MSGS;i++) { h->msgs[i] = NULL; h->msg_len[i] = 0; }

    emscripten_websocket_set_onopen(ws, ws_open, h);
    emscripten_websocket_set_onclose(ws, ws_close, h);
    emscripten_websocket_set_onerror(ws, ws_error, h);
    emscripten_websocket_set_onmessage(ws, ws_message, h);

    return h;
}

void net_close(net_handle_t *h) {
    if (!h) return;
    if (h->ws) {
        emscripten_websocket_close(h->ws, 1000, "bye");
    }
    for (int i=0;i<MAX_MSGS;i++) {
        if (h->msgs[i]) free(h->msgs[i]);
    }
    free(h);
}

net_status_t net_send(net_handle_t *h, const void *data, size_t len) {
    if (!h || !h->ws || !h->connected) return NET_NOT_CONNECTED;
    if (!data || len == 0) return NET_ERR;
    EM_BOOL ok = emscripten_websocket_send_binary(h->ws, data, (int)len);
    return ok ? NET_OK : NET_ERR;
}

net_status_t net_recv(net_handle_t *h, void *buf, size_t bufsize, size_t *out_len) {
    if (!h) return NET_ERR;
    if (!h->connected && h->msg_head == h->msg_tail) return NET_NOT_CONNECTED;
    if (!buf || !out_len) return NET_ERR;
    if (queue_pop(h, (unsigned char*)buf, bufsize, out_len)) {
        return NET_OK;
    } else {
        *out_len = 0;
        return NET_OK; /* no message yet */
    }
}

void net_poll(net_handle_t *h) {
    (void)h;
    /* Emscripten callbacks fire automatically; nothing to do here */
}

#else /* native POSIX TCP implementation */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

struct net_handle_t {
    int sockfd;
    int connected;
    /* simple single-message staging buffer for recv: native receives stream,
     * but we'll implement a simple framing: we expect the remote to send length-prefixed packets:
     * [4-byte BE length][payload]. If the remote (relay) doesn't follow that,
     * you may need to change recv usage in your netplay code or use a different framing layer.
     */
    unsigned char rd_buf[65536];
    size_t rd_buf_len;
};

static int parse_host_port(const char *url, char **host_out, char **port_out, char **path_out) {
    /* Accept forms:
     *  - tcp://host:port/path
     *  - ws://host:port/path
     *  - host:port
     */
    const char *p = url;
    const char *prefix_tcp = "tcp://";
    const char *prefix_ws = "ws://";
    if (strncmp(p, prefix_tcp, strlen(prefix_tcp)) == 0) p += strlen(prefix_tcp);
    else if (strncmp(p, prefix_ws, strlen(prefix_ws)) == 0) p += strlen(prefix_ws);

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (!colon) return -1;
    size_t hostlen = colon - p;
    size_t portlen;
    const char *portstart = colon + 1;
    const char *portend = slash ? slash : (p + strlen(p));
    portlen = portend - portstart;

    *host_out = calloc(1, hostlen + 1);
    *port_out = calloc(1, portlen + 1);
    if (!*host_out || !*port_out) {
        free(*host_out); free(*port_out);
        return -1;
    }
    memcpy(*host_out, p, hostlen);
    memcpy(*port_out, portstart, portlen);
    if (path_out) {
        if (slash) *path_out = strdup(slash);
        else *path_out = strdup("");
    }
    return 0;
}

net_status_t net_init(void) {
    return NET_OK;
}
void net_shutdown(void) { }

net_handle_t *net_connect(const char *url) {
    if (!url) return NULL;
    char *host = NULL, *port = NULL, *path = NULL;
    if (parse_host_port(url, &host, &port, &path) != 0) {
        return NULL;
    }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rv = getaddrinfo(host, port, &hints, &res);
    if (rv != 0 || !res) {
        free(host); free(port); free(path);
        return NULL;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); free(host); free(port); free(path); return NULL; }

    /* set non-blocking temporarily for connect timeout? We'll do blocking connect for simplicity */
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock);
        freeaddrinfo(res); free(host); free(port); free(path);
        return NULL;
    }

    freeaddrinfo(res); free(host); free(port); free(path);

    net_handle_t *h = calloc(1, sizeof(*h));
    if (!h) { close(sock); return NULL; }
    h->sockfd = sock;
    h->connected = 1;
    h->rd_buf_len = 0;
    return h;
}

void net_close(net_handle_t *h) {
    if (!h) return;
    if (h->sockfd >= 0) close(h->sockfd);
    free(h);
}

/* Send raw bytes */
net_status_t net_send(net_handle_t *h, const void *data, size_t len) {
    if (!h) return NET_ERR;
    if (!h->connected) return NET_NOT_CONNECTED;
    ssize_t sent = send(h->sockfd, data, len, 0);
    if (sent < 0) return NET_ERR;
    return (size_t)sent == len ? NET_OK : NET_ERR;
}

/*
 * Receive a single framed message.
 * Native side expects the server to send 4-byte big-endian length followed by payload.
 * If your relay uses raw WebSocket frames, you'll need a native websocket client.
 */
net_status_t net_recv(net_handle_t *h, void *buf, size_t bufsize, size_t *out_len) {
    if (!h || !out_len) return NET_ERR;
    if (!h->connected) return NET_NOT_CONNECTED;
    *out_len = 0;

    /* First, ensure we have at least 4 bytes of length header */
    ssize_t r;
    while (h->rd_buf_len < 4) {
        r = recv(h->sockfd, h->rd_buf + h->rd_buf_len, sizeof(h->rd_buf) - h->rd_buf_len, 0);
        if (r == 0) { h->connected = 0; return NET_NOT_CONNECTED; }
        if (r < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return NET_OK; /* no data yet */
            return NET_ERR;
        }
        h->rd_buf_len += (size_t)r;
    }

    /* parse length (big-endian) */
    uint32_t len = ((uint32_t)h->rd_buf[0] << 24) | ((uint32_t)h->rd_buf[1] << 16) |
                   ((uint32_t)h->rd_buf[2] << 8) | ((uint32_t)h->rd_buf[3]);

    /* ensure we have full payload */
    while (h->rd_buf_len < (size_t)(4 + len)) {
        r = recv(h->sockfd, h->rd_buf + h->rd_buf_len, sizeof(h->rd_buf) - h->rd_buf_len, 0);
        if (r == 0) { h->connected = 0; return NET_NOT_CONNECTED; }
        if (r < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return NET_OK; /* no more data right now */
            return NET_ERR;
        }
        h->rd_buf_len += (size_t)r;
        if (h->rd_buf_len > sizeof(h->rd_buf)) return NET_ERR;
    }

    if (len > bufsize) {
        /* caller's buffer too small: drop packet */
        /* shift remaining bytes out */
        size_t total = 4 + len;
        memmove(h->rd_buf, h->rd_buf + total, h->rd_buf_len - total);
        h->rd_buf_len -= total;
        *out_len = 0;
        return NET_ERR;
    }

    memcpy(buf, h->rd_buf + 4, len);
    *out_len = len;

    /* remove consumed bytes */
    size_t total = 4 + len;
    memmove(h->rd_buf, h->rd_buf + total, h->rd_buf_len - total);
    h->rd_buf_len -= total;

    return NET_OK;
}

void net_poll(net_handle_t *h) {
    (void)h;
    /* Optionally set socket non-blocking and handle housekeeping here */
}

#endif /* __EMSCRIPTEN__ */
