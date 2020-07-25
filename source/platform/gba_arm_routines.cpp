////////////////////////////////////////////////////////////////////////////////
//
// All of the code in this file will be compiled as arm code, and placed in the
// IWRAM section of the executable. The system has limited memory for IWRAM
// calls, so limit this file to performace critical code, or code that must be
// defined in IWRAM.
//
////////////////////////////////////////////////////////////////////////////////


// #ifdef __GBA__
// #define IWRAM_CODE __attribute__((section(".iwram"), long_call))
// #else
// #define IWRAM_CODE
// #endif // __GBA__


#include "/opt/devkitpro/libgba/include/gba_interrupt.h"
#include "gba_color.hpp"
#include "number/numeric.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include "/opt/devkitpro/libgba/include/gba_systemcalls.h"
#pragma GCC diagnostic pop


// Because the cartridge interrupt handler runs when the cartridge is removed,
// it obviously cannot be defined in gamepak rom! So we have to put the code in
// IWRAM.
IWRAM_CODE
void cartridge_interrupt_handler()
{
#define REG_DISPCNT *(unsigned*)0x4000000
#define DCNT_MODE3 0x0003
#define DCNT_BG2 0x0400

    irqDisable(IRQ_VBLANK | IRQ_HBLANK | IRQ_VCOUNT | IRQ_TIMER0 | IRQ_TIMER1 |
               IRQ_TIMER2 | IRQ_TIMER3 | IRQ_SERIAL | IRQ_DMA0 | IRQ_DMA1 |
               IRQ_DMA2 | IRQ_DMA3 | IRQ_KEYPAD | IRQ_GAMEPAK);


    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;

    const auto blue_screen_color = Color(0, 0, 255).bgr_hex_555();

    int ii;
    u32* dst = (u32*)0x06000000;
    u32 wd = (blue_screen_color << 16) | blue_screen_color;

#define M3_SIZE 0x12C00
    for (ii = 0; ii < M3_SIZE / 4; ii++)
        *dst++ = wd;

    while (true)
        ;
}