#ifndef SDL_NET_WEBSOCKET_H
#define SDL_NET_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Minimal replacement for the few SDL_net TCP functions netplay.c expects.
 * Designed for Emscripten / browser builds (uses WebSockets).
 *
 * Notes:
 *  - TCPsocket is an int id; -1 on error.
 *  - SDLNet_TCP_Recv returns:
 *      >0 bytes read,
 *       0 no data available,
 *      -1 error,
 *      -2 socket closed sentinel (treated as EOF).
 */

typedef int TCPsocket; /* socket id, -1 on error */

int SDLNet_Init(void);
void SDLNet_Quit(void);

int SDLNet_ResolveHost(void *ip, const char *host, unsigned short port);

TCPsocket SDLNet_TCP_Open(const char *host_or_url);

int SDLNet_TCP_Send(TCPsocket s, const void *data, int len);
int SDLNet_TCP_Recv(TCPsocket s, void *data, int maxlen);

void SDLNet_TCP_Close(TCPsocket s);

const char* SDLNet_CheckError(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_NET_WEBSOCKET_H */
