// device.c - WebAssembly/Emscripten friendly version
// Stripped of libretro-private dependencies

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "memory/memory.h"
#include "dd/dd_controller.h"
#include "rcp/rcp.h"
#include "pi/pi_controller.h"
#include "si/si_controller.h"
#include "rsp/rsp_core.h"
#include "ai/ai_controller.h"
#include "vi/vi_controller.h"
#include "rdp/rdp_core.h"
#include "pif/pif.h"
#include "cart/cart.h"

// Initialize the main emulated device
void device_init(struct device* dev)
{
    if (!dev) return;

    memset(dev, 0, sizeof(struct device));

    memory_init(&dev->mem);
    rcp_init(&dev->rcp, dev);
    pi_init(&dev->pi, dev);
    si_init(&dev->si, dev);
    rsp_init(&dev->rsp, dev);
    ai_init(&dev->ai, dev);
    vi_init(&dev->vi, dev);
    rdp_init(&dev->rdp, dev);
    dd_init(&dev->dd, dev);
    pif_init(&dev->pif, dev);
    cart_init(&dev->cart, dev);

    // No retroarch/libretro memory map ? internal only
    printf("[device] Initialized successfully (no libretro hooks)\n");
}

// Power-on reset
void device_poweron(struct device* dev)
{
    if (!dev) return;

    memory_reset(&dev->mem);
    rcp_reset(&dev->rcp);
    pi_reset(&dev->pi);
    si_reset(&dev->si);
    rsp_reset(&dev->rsp);
    ai_reset(&dev->ai);
    vi_reset(&dev->vi);
    rdp_reset(&dev->rdp);
    dd_reset(&dev->dd);
    pif_reset(&dev->pif);
    cart_reset(&dev->cart);

    printf("[device] Power-on reset complete\n");
}

// Power-off cleanup
void device_destroy(struct device* dev)
{
    if (!dev) return;

    cart_destroy(&dev->cart);
    pif_destroy(&dev->pif);
    dd_destroy(&dev->dd);
    rdp_destroy(&dev->rdp);
    vi_destroy(&dev->vi);
    ai_destroy(&dev->ai);
    rsp_destroy(&dev->rsp);
    si_destroy(&dev->si);
    pi_destroy(&dev->pi);
    rcp_destroy(&dev->rcp);
    memory_destroy(&dev->mem);

    memset(dev, 0, sizeof(struct device));

    printf("[device] Destroyed successfully\n");
}
