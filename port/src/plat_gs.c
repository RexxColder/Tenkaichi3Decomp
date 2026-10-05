/*
 * DMA transfers on PC. On the PS2 the game starts a transfer by setting bit 0x100 of a channel's control register
 * (Dn_CHCR) and the hardware clears the bit when the data has gone out. Here the game's register macros call
 * Port_DmaChcr on every access: a transfer that was started is carried out first and the bit cleared, so the
 * game's "wait until the channel is idle" loops end at once.
 *
 * Channels: 1 = VIF1 (models through the VU1 microprograms), 2 = GIF (GS packets), 4 = IPU (movies).
 * Headless build: the data is dropped. A renderer takes over in Port_DmaTransfer.
 */
#include <stdint.h>

#define REG(a) ((volatile uint32_t *)(uintptr_t)(a))

static void Port_DmaTransfer(int channel) {
    (void)channel; /* headless: nothing is drawn */
}

volatile uint32_t *Port_DmaChcr(int channel) {
    volatile uint32_t *chcr = REG(0x10008000u + 0x1000u * (uint32_t)channel - (channel == 4 ? 0xC00u : 0u));

    if (*chcr & 0x100u) {
        Port_DmaTransfer(channel);
        *chcr &= ~0x100u;
    }
    return chcr;
}
