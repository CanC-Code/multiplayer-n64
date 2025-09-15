#ifndef NET_COMPAT_H__
#define NET_COMPAT_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_OK = 0,
    NET_ERR,
    NET_NOT_CONNECTED,
} net_status_t;

/* Opaque handle type (implementation-defined) */
typedef struct net_handle_t net_handle_t;

/* Initialize networking subsystem. Call once. */
net_status_t net_init(void);

/* Shutdown networking subsystem. */
void net_shutdown(void);

/*
 * Connect to the specified WebSocket/TCP URL.
 * For browser builds use "ws://" or "wss://".
 * For native builds the supported forms are "tcp://host:port" or "host:port" or "ws://host:port" (treated as tcp).
 * Returns NULL on error.
 */
net_handle_t *net_connect(const char *url);

/* Close connection and free handle. */
void net_close(net_handle_t *h);

/* Send data (binary). Returns NET_OK on success. */
net_status_t net_send(net_handle_t *h, const void *data, size_t len);

/*
 * Receive a single message/packet.
 * - buf: pointer to buffer
 * - bufsize: size of buffer
 * - out_len: pointer to size_t to receive message length
 *
 * Returns:
 *  - NET_OK and *out_len > 0 when a message was returned
 *  - NET_OK and *out_len == 0 when no message is currently available
 *  - NET_NOT_CONNECTED if not connected
 *  - NET_ERR on error
 */
net_status_t net_recv(net_handle_t *h, void *buf, size_t bufsize, size_t *out_len);

/* Poll/update internal state (Emscripten callbacks need no-op poll). */
void net_poll(net_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* NET_COMPAT_H__ */
