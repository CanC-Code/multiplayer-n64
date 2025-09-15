/*
 * netplay_ws.c
 *
 * Rewritten netplay interface that uses a small neutral network API.
 * When compiled native (no __EMSCRIPTEN__) the implementation delegates to SDL_net.
 * When compiled for Emscripten, it delegates to the WebSocket wrapper in custom/sdl_net_websocket.h.
 *
 * Place this file at:
 *   mupen64plus-next/mupen64plus-core/src/main/netplay_ws.c
 *
 * The WebSocket wrapper header (custom/sdl_net_websocket.h) must provide:
 *   - int WS_Init(void);
 *   - void WS_Quit(void);
 *   - void* WS_UDP_Open(int port);                     // returns UDPsocket-like handle
 *   - void  WS_UDP_Close(void* udpsock);
 *   - int   WS_UDP_Bind(void* udpsock, int channel, void* ipaddr); // returns channel index or -1
 *   - void* WS_TCP_Open(void* ipaddr);                // returns TCPsocket-like handle
 *   - void  WS_TCP_Close(void* tcpsock);
 *   - void* WS_AllocPacket(int size);                 // returns UDPpacket-like pointer
 *   - void  WS_FreePacket(void* packet);
 *   - int   WS_UDP_Send(void* udpsock, int channel, void* packet);
 *   - int   WS_UDP_Recv(void* udpsock, void* packet); // returns 1 on recv, 0 otherwise
 *   - int   WS_TCP_Send(void* tcpsock, const void* data, int len);
 *   - int   WS_TCP_Recv(void* tcpsock, void* data, int len);
 *   - void  WS_ResolveHost(void* out_ipaddr, const char* host, int port);
 *   - uint32_t WS_Read32(const uint8_t* buf);
 *   - void     WS_Write32(uint32_t v, uint8_t* buf);
 *
 * If your wrapper uses different names, either rename them or add thin forwarders.
 */

#define SETTINGS_SIZE 24

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "main.h"
#include "util.h"
#include "plugin/plugin.h"
#include "backends/plugins_compat/plugins_compat.h"
#include "netplay.h"
#include "custom/libretro_private.h"

#include <mupen64plus-next_common.h>

#if defined(__EMSCRIPTEN__)
/* Expect a custom WebSocket-based implementation */
#include "custom/sdl_net_websocket.h"
#else
/* Fallback to SDL_net on native builds */
#include <SDL2/SDL_net.h>
#endif

#include <mupen64plus-next_common.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if !defined(WIN32)
#include <sys/socket.h>
#endif

#ifndef HAVE_LIBNX
#include <netinet/ip.h>
#else
#include <switch.h>
#include <netinet/in.h>
#endif // HAVE_LIBNX

/* ----------------- small neutral wrapper layer -----------------
   These wrapper names are used below in the main netplay implementation.
   On native builds they map to SDLNet_* functions.
   On Emscripten builds they should be implemented by custom/sdl_net_websocket.h/.c
-----------------------------------------------------------------*/

#if defined(__EMSCRIPTEN__)
/* Types used by the code below must be defined in custom/sdl_net_websocket.h:
   - typedef void* UDPsocket;
   - typedef void* TCPsocket;
   - typedef struct { uint8_t *data; int len; } UDPpacket; (or something compatible)
   - typedef void* IPaddress;
   The functions listed at top-of-file must be available.
*/
#define NET_Init()            WS_Init()
#define NET_Quit()            WS_Quit()
#define UDP_Open(port)        WS_UDP_Open(port)
#define UDP_Close(sock)       WS_UDP_Close(sock)
#define UDP_AllocPacket(sz)   WS_AllocPacket(sz)
#define UDP_FreePacket(p)     WS_FreePacket(p)
#define UDP_Send(sock, chan, pkt) WS_UDP_Send(sock, chan, pkt)
#define UDP_Recv(sock, pkt)   WS_UDP_Recv(sock, pkt)
#define UDP_Unbind(sock, chan) WS_UDP_Unbind ? WS_UDP_Unbind(sock, chan) : 0
#define UDP_Bind(sock, chan, addr) WS_UDP_Bind(sock, chan, addr)

#define TCP_Open(addr)        WS_TCP_Open(addr)
#define TCP_Close(tcpsock)    WS_TCP_Close(tcpsock)
#define TCP_Send(tcpsock, buf, len) WS_TCP_Send(tcpsock, buf, len)
#define TCP_Recv(tcpsock, buf, len) WS_TCP_Recv(tcpsock, buf, len)

#define ResolveHost(dest, host, port) WS_ResolveHost(dest, host, port)
#define Read32(buf)            WS_Read32(buf)
#define Write32(v, buf)        WS_Write32(v, buf)

#else /* native (SDL_net) */

#define NET_Init()            (SDLNet_Init() >= 0 ? 0 : -1)
#define NET_Quit()            SDLNet_Quit()
#define UDP_Open(port)        SDLNet_UDP_Open(port)
#define UDP_Close(sock)       SDLNet_UDP_Close(sock)
#define UDP_AllocPacket(sz)   SDLNet_AllocPacket(sz)
#define UDP_FreePacket(p)     SDLNet_FreePacket(p)
#define UDP_Send(sock, chan, pkt) SDLNet_UDP_Send(sock, chan, pkt)
#define UDP_Recv(sock, pkt)   SDLNet_UDP_Recv(sock, pkt)
#define UDP_Unbind(sock, chan) SDLNet_UDP_Unbind(sock, chan)
#define UDP_Bind(sock, chan, addr) SDLNet_UDP_Bind(sock, chan, addr)

#define TCP_Open(addr)        SDLNet_TCP_Open(addr)
#define TCP_Close(tcpsock)    SDLNet_TCP_Close(tcpsock)
#define TCP_Send(tcpsock, buf, len) SDLNet_TCP_Send(tcpsock, buf, len)
#define TCP_Recv(tcpsock, buf, len) SDLNet_TCP_Recv(tcpsock, buf, len)

#define ResolveHost(dest, host, port) SDLNet_ResolveHost(dest, host, port)
#define Read32(buf)            SDLNet_Read32(buf)
#define Write32(v, buf)        SDLNet_Write32((int32_t)(v), buf)

#endif

/* ----------------- netplay implementation (nearly identical to original) ----------------- */

static int l_canFF;
static int l_netplay_controller;
static int l_netplay_control[4];
static void* l_udpSocket; /* UDPsocket or wrapper representation */
static void* l_tcpSocket; /* TCPsocket or wrapper representation */
static int l_udpChannel;
static int l_spectator;
static int l_netplay_is_init = 0;
static uint32_t l_vi_counter;
static uint8_t l_status;
static uint32_t l_reg_id;
static struct controller_input_compat *l_cin_compats;
static uint8_t l_plugin[4];
static uint8_t l_buffer_target;
static uint8_t l_player_lag[4];

/* packet / protocol codes (same as upstream) */
#define UDP_SEND_KEY_INFO 0
#define UDP_RECEIVE_KEY_INFO 1
#define UDP_REQUEST_KEY_INFO 2
#define UDP_RECEIVE_KEY_INFO_GRATUITOUS 3
#define UDP_SYNC_DATA 4

#define TCP_SEND_SAVE 1
#define TCP_RECEIVE_SAVE 2
#define TCP_SEND_SETTINGS 3
#define TCP_RECEIVE_SETTINGS 4
#define TCP_REGISTER_PLAYER 5
#define TCP_GET_REGISTRATION 6
#define TCP_DISCONNECT_NOTICE 7

/* helper to compute local buffer size */
static uint8_t buffer_size(uint8_t control_id)
{
    uint8_t counter = 0;
    struct netplay_event* current = l_cin_compats[control_id].event_first;
    while (current != NULL) {
        current = current->next;
        ++counter;
    }
    return counter;
}

/* allocate and fill a UDP packet object using the wrapper's type */
static void* alloc_packet(int size)
{
#if defined(__EMSCRIPTEN__)
    return UDP_AllocPacket(size);
#else
    return UDP_AllocPacket(size); /* SDLNet_AllocPacket */
#endif
}
static void free_packet(void* p) { if (p) UDP_FreePacket(p); }

/* network request helper wrappers for read/write 32 */
static inline uint32_t net_read32(const uint8_t* b) { return Read32(b); }
static inline void net_write32(uint32_t v, uint8_t* b) { Write32(v, b); }

/* forward declarations */
static void netplay_request_input(uint8_t control_id);
static int netplay_ensure_valid(uint8_t control_id);
static void netplay_process(void);

/* start netplay, connecting to host:port */
m64p_error netplay_start(const char* host, int port)
{
    if (NET_Init() != 0)
    {
        log_cb(RETRO_LOG_INFO, "Netplay: Could not initialize network library\n");
        return M64ERR_SYSTEM_FAIL;
    }

    l_udpSocket = UDP_Open(0);
    if (l_udpSocket == NULL)
    {
        log_cb(RETRO_LOG_INFO, "Netplay: UDP socket creation failed\n");
        return M64ERR_SYSTEM_FAIL;
    }

#if !defined(WIN32)
    /* set TOS locally if available on native stack; wrapper may ignore */
    const char tos_local = (32 << 2);
    /* for wrapper this may be a no-op */
    /* no portable way to set without exposing underlying FD; wrapper may provide it */
    (void)tos_local;
#endif

    /* resolve host */
    void* dest_addr = NULL;
#if defined(__EMSCRIPTEN__)
    /* WS wrapper expected to provide an IPaddress-type opaque pointer creation */
    dest_addr = malloc(sizeof(void*));
    ResolveHost(dest_addr, host, port);
#else
    IPaddress* dest = malloc(sizeof(IPaddress));
    if (ResolveHost(dest, host, port) < 0) {
        log_cb(RETRO_LOG_INFO, "Netplay: Could not resolve host %s:%d\n", host, port);
        free(dest);
        return M64ERR_SYSTEM_FAIL;
    }
    dest_addr = dest;
#endif

    l_udpChannel = UDP_Bind(l_udpSocket, -1, dest_addr);
    if (l_udpChannel < 0)
    {
        log_cb(RETRO_LOG_INFO, "Netplay: could not bind to UDP socket\n");
        UDP_Close(l_udpSocket);
        l_udpSocket = NULL;
        free(dest_addr);
        return M64ERR_SYSTEM_FAIL;
    }

    l_tcpSocket = TCP_Open(dest_addr);
    if (l_tcpSocket == NULL)
    {
        log_cb(RETRO_LOG_INFO, "Netplay: could not open TCP socket\n");
        UDP_Unbind(l_udpSocket, l_udpChannel);
        UDP_Close(l_udpSocket);
        l_udpSocket = NULL;
        free(dest_addr);
        return M64ERR_SYSTEM_FAIL;
    }

    free(dest_addr);

    for (int i = 0; i < 4; ++i)
    {
        l_netplay_control[i] = -1;
        l_plugin[i] = 0;
        l_player_lag[i] = 0;
    }

    l_canFF = 0;
    l_netplay_controller = 0;
    l_netplay_is_init = 1;
    l_spectator = 1;
    l_vi_counter = 0;
    l_status = 0;
    l_reg_id = 0;

    return M64ERR_SUCCESS;
}

/* stop netplay and clean up */
m64p_error netplay_stop()
{
    if (l_udpSocket == NULL)
        return M64ERR_INVALID_STATE;

    if (l_cin_compats != NULL)
    {
        for (int i = 0; i < 4; ++i)
        {
            struct netplay_event* current = l_cin_compats[i].event_first;
            struct netplay_event* next;
            while (current != NULL)
            {
                next = current->next;
                free(current);
                current = next;
            }
        }
    }

    /* notify server of disconnect if possible */
    char output_data[5];
    output_data[0] = TCP_DISCONNECT_NOTICE;
    net_write32(l_reg_id, (uint8_t*)&output_data[1]);
    if (l_tcpSocket)
        TCP_Send(l_tcpSocket, &output_data[0], 5);

    /* unbind/close sockets */
    if (l_udpSocket) {
        UDP_Unbind(l_udpSocket, l_udpChannel);
        UDP_Close(l_udpSocket);
        l_udpSocket = NULL;
    }
    if (l_tcpSocket) {
        TCP_Close(l_tcpSocket);
        l_tcpSocket = NULL;
    }

    l_udpChannel = -1;
    l_netplay_is_init = 0;
    NET_Quit();
    return M64ERR_SUCCESS;
}

int netplay_is_init() { return l_netplay_is_init; }

/* request input for a given player via UDP request packet */
static void netplay_request_input(uint8_t control_id)
{
    void* packet = alloc_packet(12);
    if (!packet) return;

#if defined(__EMSCRIPTEN__)
    /* wrapper packaging assumed to expose packet->data and packet->len */
    ((uint8_t*)(((UDPpacket*)packet)->data))[0] = UDP_REQUEST_KEY_INFO;
    ((uint8_t*)(((UDPpacket*)packet)->data))[1] = control_id;
    net_write32(l_reg_id, &((uint8_t*)(((UDPpacket*)packet)->data))[2]);
    net_write32(l_cin_compats[control_id].netplay_count, &((uint8_t*)(((UDPpacket*)packet)->data))[6]);
    ((uint8_t*)(((UDPpacket*)packet)->data))[10] = l_spectator;
    ((uint8_t*)(((UDPpacket*)packet)->data))[11] = buffer_size(control_id);
    ((UDPpacket*)packet)->len = 12;
    UDP_Send(l_udpSocket, l_udpChannel, packet);
#else
    UDPpacket* p = (UDPpacket*)packet;
    p->data[0] = UDP_REQUEST_KEY_INFO;
    p->data[1] = control_id;
    Write32(l_reg_id, &p->data[2]);
    Write32(l_cin_compats[control_id].netplay_count, &p->data[6]);
    p->data[10] = l_spectator;
    p->data[11] = buffer_size(control_id);
    p->len = 12;
    UDP_Send(l_udpSocket, l_udpChannel, p);
#endif

    free_packet(packet);
}

/* check whether an event count is already present in our local buffer */
static int check_valid(uint8_t control_id, uint32_t count)
{
    struct netplay_event* current = l_cin_compats[control_id].event_first;
    while (current != NULL) {
        if (current->count == count) return 1;
        current = current->next;
    }
    return 0;
}

/* thread helper used to request input until it arrives (or timeout) */
static int netplay_require_response(void* opaque)
{
    uint8_t control_id = *(uint8_t*)opaque;
    uint32_t timeout = SDL_GetTicks() + 10000;
    while (!check_valid(control_id, l_cin_compats[control_id].netplay_count))
    {
        if (SDL_GetTicks() > timeout) {
            l_udpChannel = -1;
            return 0;
        }
        netplay_request_input(control_id);
        SDL_Delay(5);
    }
    return 1;
}

/* process incoming UDP packets and insert into local event lists */
static void netplay_process()
{
    void* packet = alloc_packet(512);
    if (!packet) return;

    while (UDP_Recv(l_udpSocket, packet) == 1)
    {
#if defined(__EMSCRIPTEN__)
        uint8_t* data = (uint8_t*)(((UDPpacket*)packet)->data);
        int datalen = ((UDPpacket*)packet)->len;
#else
        uint8_t* data = ((UDPpacket*)packet)->data;
        int datalen = ((UDPpacket*)packet)->len;
#endif
        uint32_t curr = 0;
        uint32_t count, keys;
        uint8_t player, plugin, current_status;

        switch (data[0]) {
            case UDP_RECEIVE_KEY_INFO:
            case UDP_RECEIVE_KEY_INFO_GRATUITOUS:
                player = data[1];
                current_status = data[2];
                if (data[0] == UDP_RECEIVE_KEY_INFO)
                    l_player_lag[player] = data[3];

                if (current_status != l_status) {
                    if (((current_status & 0x1) ^ (l_status & 0x1)) != 0)
                        log_cb(RETRO_LOG_INFO, "Netplay: players have de-synced at VI %u\n", l_vi_counter);
                    for (int dis = 1; dis < 5; ++dis) {
                        if (((current_status & (0x1 << dis)) ^ (l_status & (0x1 << dis))) != 0)
                            log_cb(RETRO_LOG_INFO, "Netplay: player %u has disconnected\n", dis);
                    }
                    l_status = current_status;
                }

                curr = 5;
                for (uint8_t i = 0; i < data[4]; ++i)
                {
                    count = net_read32(&data[curr]); curr += 4;

                    if (((count - l_cin_compats[player].netplay_count) > (UINT32_MAX / 2)) || (check_valid(player, count))) {
                        curr += 5; /* skip */
                        continue;
                    }

                    keys = net_read32(&data[curr]); curr += 4;
                    plugin = data[curr]; curr += 1;

                    struct netplay_event* new_event = (struct netplay_event*)malloc(sizeof(struct netplay_event));
                    new_event->count = count;
                    new_event->buttons = keys;
                    new_event->plugin = plugin;
                    new_event->next = l_cin_compats[player].event_first;
                    l_cin_compats[player].event_first = new_event;
                }
                break;

            default:
                log_cb(RETRO_LOG_INFO, "Netplay: received unknown message from server\n");
                break;
        }
    }

    free_packet(packet);
}

/* ensure we have an event available for the control_id; spawn requester thread if needed */
static int netplay_ensure_valid(uint8_t control_id)
{
    if (check_valid(control_id, l_cin_compats[control_id].netplay_count)) return 1;
    if (l_udpChannel == -1) return 0;

    SDL_Thread* thread = SDL_CreateThread(netplay_require_response, "Netplay key request", &control_id);

    while (!check_valid(control_id, l_cin_compats[control_id].netplay_count) && l_udpChannel != -1)
        netplay_process();

    int success;
    SDL_WaitThread(thread, &success);
    return success;
}

/* delete a specific event node from the linked list */
static void netplay_delete_event(struct netplay_event* current, uint8_t control_id)
{
    struct netplay_event* find = l_cin_compats[control_id].event_first;
    while (find != NULL) {
        if (find->next == current) {
            find->next = current->next;
            break;
        }
        find = find->next;
    }
    if (current == l_cin_compats[control_id].event_first)
        l_cin_compats[control_id].event_first = l_cin_compats[control_id].event_first->next;
    free(current);
}

/* get the next input for the given control_id (consumes an event) */
static uint32_t netplay_get_input(uint8_t control_id)
{
    uint32_t keys;
    struct retro_fastforwarding_override ff_override;

    netplay_process();
    netplay_request_input(control_id);

    if (l_player_lag[control_id] > 0 && buffer_size(control_id) > l_buffer_target) {
        l_canFF = 1;
        main_core_state_set(M64CORE_SPEED_LIMITER, 0);

        ff_override.fastforward = true;
        ff_override.inhibit_toggle = true;
        environ_cb(RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE, &ff_override);
    } else {
        main_core_state_set(M64CORE_SPEED_LIMITER, 1);

        ff_override.fastforward = false;
        ff_override.inhibit_toggle = true;
        environ_cb(RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE, &ff_override);

        l_canFF = 0;
    }

    if (netplay_ensure_valid(control_id)) {
        struct netplay_event* current = l_cin_compats[control_id].event_first;
        while (current->count != l_cin_compats[control_id].netplay_count)
            current = current->next;
        keys = current->buttons;
        Controls[control_id].Plugin = current->plugin;
        netplay_delete_event(current, control_id);
        ++l_cin_compats[control_id].netplay_count;
    } else {
        log_cb(RETRO_LOG_INFO, "Netplay: lost connection to server\n");
        main_core_state_set(M64CORE_EMU_STATE, M64EMU_STOPPED);
        keys = 0;
    }

    return keys;
}

/* send local input for a player */
static void netplay_send_input(uint8_t control_id, uint32_t keys)
{
    void* packet = alloc_packet(11);
    if (!packet) return;

#if defined(__EMSCRIPTEN__)
    uint8_t* data = (uint8_t*)(((UDPpacket*)packet)->data);
    data[0] = UDP_SEND_KEY_INFO;
    data[1] = control_id;
    net_write32(l_cin_compats[control_id].netplay_count, &data[2]);
    net_write32(keys, &data[6]);
    data[10] = l_plugin[control_id];
    ((UDPpacket*)packet)->len = 11;
    UDP_Send(l_udpSocket, l_udpChannel, packet);
#else
    UDPpacket* p = (UDPpacket*)packet;
    p->data[0] = UDP_SEND_KEY_INFO;
    p->data[1] = control_id;
    Write32(l_cin_compats[control_id].netplay_count, &p->data[2]);
    Write32(keys, &p->data[6]);
    p->data[10] = l_plugin[control_id];
    p->len = 11;
    UDP_Send(l_udpSocket, l_udpChannel, p);
#endif

    free_packet(packet);
}

/* register player via TCP; server returns buffer target */
uint8_t netplay_register_player(uint8_t player, uint8_t plugin, uint8_t rawdata, uint32_t reg_id)
{
    l_reg_id = reg_id;
    char output_data[8];
    output_data[0] = TCP_REGISTER_PLAYER;
    output_data[1] = player;
    output_data[2] = plugin;
    output_data[3] = rawdata;
    net_write32(l_reg_id, (uint8_t*)&output_data[4]);

    TCP_Send(l_tcpSocket, &output_data[0], 8);

    uint8_t response[2];
    size_t recv = 0;
    while (recv < 2)
        recv += TCP_Recv(l_tcpSocket, &response[recv], 2 - recv);
    l_buffer_target = response[1];
    return response[0];
}

int netplay_lag() { return l_canFF; }
int netplay_next_controller() { return l_netplay_controller; }
void netplay_set_controller(uint8_t player) { l_netplay_control[player] = l_netplay_controller++; l_spectator = 0; }
int netplay_get_controller(uint8_t player) { return l_netplay_control[player]; }

/* read / sync storage (savegames) via TCP */
file_status_t netplay_read_storage(const char *filename, void *data, size_t size)
{
    const char *file_extension = strrchr(filename, '.');
    if (!file_extension) file_extension = "";
    else file_extension += 1;

    uint32_t buffer_pos = 0;
    char *output_data = malloc(size + strlen(file_extension) + 6);
    if (!output_data) return file_open_error;

    file_status_t ret;
    uint8_t request;
    if (l_netplay_control[0] != -1)
    {
        request = TCP_SEND_SAVE;
        memcpy(&output_data[buffer_pos], &request, 1);
        ++buffer_pos;

        memcpy(&output_data[buffer_pos], file_extension, strlen(file_extension) + 1);
        buffer_pos += strlen(file_extension) + 1;

        net_write32((uint32_t)size, (uint8_t*)&output_data[buffer_pos]);
        buffer_pos += 4;
        memcpy(&output_data[buffer_pos], data, size);
        buffer_pos += size;

        TCP_Send(l_tcpSocket, &output_data[0], buffer_pos);
    }
    else
    {
        request = TCP_RECEIVE_SAVE;
        memcpy(&output_data[buffer_pos], &request, 1);
        ++buffer_pos;

        memcpy(&output_data[buffer_pos], file_extension, strlen(file_extension) + 1);
        buffer_pos += strlen(file_extension) + 1;

        TCP_Send(l_tcpSocket, &output_data[0], buffer_pos);
        size_t recv = 0;
        char *data_array = data;
        while (recv < size)
            recv += TCP_Recv(l_tcpSocket, data_array + recv, size - recv);

        int sum = 0;
        for (size_t i = 0; i < size; ++i)
            sum |= (unsigned char)data_array[i];

        if (sum == 0)
            ret = file_open_error;
        else
            ret = file_ok;
    }
    free(output_data);
    return ret;
}

/* sync emulation settings via TCP */
void netplay_sync_settings(uint32_t *count_per_op, uint32_t *count_per_op_denom_pot, uint32_t *disable_extra_mem, int32_t *si_dma_duration, uint32_t *emumode, int32_t *no_compiled_jump)
{
    if (!netplay_is_init()) return;

    char output_data[SETTINGS_SIZE + 1];
    uint8_t request;
    if (l_netplay_control[0] != -1)
    {
        request = TCP_SEND_SETTINGS;
        memcpy(&output_data[0], &request, 1);
        net_write32(*count_per_op, (uint8_t*)&output_data[1]);
        net_write32(*count_per_op_denom_pot, (uint8_t*)&output_data[5]);
        net_write32(*disable_extra_mem, (uint8_t*)&output_data[9]);
        net_write32((uint32_t)*si_dma_duration, (uint8_t*)&output_data[13]);
        net_write32(*emumode, (uint8_t*)&output_data[17]);
        net_write32((uint32_t)*no_compiled_jump, (uint8_t*)&output_data[21]);
        TCP_Send(l_tcpSocket, &output_data[0], SETTINGS_SIZE + 1);
    }
    else
    {
        request = TCP_RECEIVE_SETTINGS;
        output_data[0] = request;
        TCP_Send(l_tcpSocket, &output_data[0], 1);
        int32_t recv = 0;
        while (recv < SETTINGS_SIZE)
            recv += TCP_Recv(l_tcpSocket, &output_data[recv], SETTINGS_SIZE - recv);
        *count_per_op = net_read32((uint8_t*)&output_data[0]);
        *count_per_op_denom_pot = net_read32((uint8_t*)&output_data[4]);
        *disable_extra_mem = net_read32((uint8_t*)&output_data[8]);
        *si_dma_duration = (int32_t)net_read32((uint8_t*)&output_data[12]);
        *emumode = net_read32((uint8_t*)&output_data[16]);
        *no_compiled_jump = (int32_t)net_read32((uint8_t*)&output_data[20]);
    }
}

/* periodic sync/check for desyncs (send CP0 registers) */
void netplay_check_sync(struct cp0* cp0)
{
    if (!netplay_is_init()) return;

    const uint32_t* cp0_regs = r4300_cp0_regs(cp0);

    if (l_vi_counter % 600 == 0)
    {
        uint32_t packet_len = (CP0_REGS_COUNT * 4) + 5;
        void* packet = alloc_packet(packet_len);
        if (!packet) return;

#if defined(__EMSCRIPTEN__)
        uint8_t* data = (uint8_t*)(((UDPpacket*)packet)->data);
        data[0] = UDP_SYNC_DATA;
        net_write32(l_vi_counter, &data[1]);
        for (int i = 0; i < CP0_REGS_COUNT; ++i)
            net_write32(cp0_regs[i], &data[(i * 4) + 5]);
        ((UDPpacket*)packet)->len = packet_len;
        UDP_Send(l_udpSocket, l_udpChannel, packet);
#else
        UDPpacket* p = (UDPpacket*)packet;
        p->data[0] = UDP_SYNC_DATA;
        Write32(l_vi_counter, &p->data[1]);
        for (int i = 0; i < CP0_REGS_COUNT; ++i)
            Write32(cp0_regs[i], &p->data[(i * 4) + 5]);
        p->len = packet_len;
        UDP_Send(l_udpSocket, l_udpChannel, p);
#endif
        free_packet(packet);
    }
    ++l_vi_counter;
}

/* read server registration (called right before game starts) */
void netplay_read_registration(struct controller_input_compat* cin_compats)
{
    if (!netplay_is_init()) return;

    l_cin_compats = cin_compats;

    uint32_t reg_id;
    char output_data = TCP_GET_REGISTRATION;
    char input_data[24];
    TCP_Send(l_tcpSocket, &output_data, 1);
    size_t recv = 0;
    while (recv < 24)
        recv += TCP_Recv(l_tcpSocket, &input_data[recv], 24 - recv);

    uint32_t curr = 0;
    for (int i = 0; i < 4; ++i)
    {
        reg_id = net_read32((uint8_t*)&input_data[curr]);
        curr += 4;
        if (reg_id == 0)
        {
            Controls[i].Present = 0;
            Controls[i].Plugin = PLUGIN_NONE;
            Controls[i].RawData = 0;
            curr += 2;
        }
        else
        {
            Controls[i].Present = 1;
            if (i > 0 && input_data[curr] == PLUGIN_MEMPAK)
                Controls[i].Plugin = PLUGIN_NONE;
            else if (input_data[curr] == PLUGIN_TRANSFER_PAK)
                Controls[i].Plugin = PLUGIN_NONE;
            else
                Controls[i].Plugin = input_data[curr];
            l_plugin[i] = Controls[i].Plugin;
            ++curr;
            Controls[i].RawData = input_data[curr];
            ++curr;
        }
    }
}

/* send/receive raw PIF-level input (used by emulator core) */
static void netplay_send_raw_input(struct pif* pif)
{
    for (int i = 0; i < 4; ++i)
    {
        if (l_netplay_control[i] != -1)
        {
            if (pif->channels[i].tx && pif->channels[i].tx_buf[0] == JCMD_CONTROLLER_READ)
                netplay_send_input(i, *(uint32_t*)pif->channels[i].rx_buf);
        }
    }
}

static void netplay_get_raw_input(struct pif* pif)
{
    for (int i = 0; i < 4; ++i)
    {
        if (Controls[i].Present == 1)
        {
            if (pif->channels[i].tx)
            {
                *pif->channels[i].rx &= ~0xC0; //Always show the controller as connected

                if (pif->channels[i].tx_buf[0] == JCMD_CONTROLLER_READ)
                {
                    *(uint32_t*)pif->channels[i].rx_buf = netplay_get_input(i);
                }
                else if ((pif->channels[i].tx_buf[0] == JCMD_STATUS || pif->channels[i].tx_buf[0] == JCMD_RESET) && Controls[i].RawData)
                {
                    uint16_t type = JDT_JOY_ABS_COUNTERS | JDT_JOY_PORT;
                    pif->channels[i].rx_buf[0] = (uint8_t)(type >> 0);
                    pif->channels[i].rx_buf[1] = (uint8_t)(type >> 8);
                    pif->channels[i].rx_buf[2] = 0;
                }
                else if (pif->channels[i].tx_buf[0] == JCMD_PAK_READ && Controls[i].RawData)
                {
                    pif->channels[i].rx_buf[32] = 255;
                }
                else if (pif->channels[i].tx_buf[0] == JCMD_PAK_WRITE && Controls[i].RawData)
                {
                    pif->channels[i].rx_buf[0] = 255;
                }
            }
        }
    }
}

void netplay_update_input(struct pif* pif)
{
    if (netplay_is_init())
    {
        netplay_send_raw_input(pif);
        netplay_get_raw_input(pif);
    }
}

/* send arbitrary config via TCP */
m64p_error netplay_send_config(char* data, int size)
{
    if (!netplay_is_init()) return M64ERR_NOT_INIT;

    if (l_netplay_control[0] != -1 || size == 1)
    {
        int result = TCP_Send(l_tcpSocket, data, size);
        if (result < size) return M64ERR_SYSTEM_FAIL;
        return M64ERR_SUCCESS;
    }
    else return M64ERR_INVALID_STATE;
}

/* receive arbitrary config via TCP */
m64p_error netplay_receive_config(char* data, int size)
{
    if (!netplay_is_init()) return M64ERR_NOT_INIT;

    if (l_netplay_control[0] == -1)
    {
        int recv = 0;
        while (recv < size)
            recv += TCP_Recv(l_tcpSocket, &data[recv], size - recv);
        return M64ERR_SUCCESS;
    }
    else return M64ERR_INVALID_STATE;
}
