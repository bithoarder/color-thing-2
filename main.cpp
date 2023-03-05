#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "tusb_config.h"
#include "bsp/board.h"
#include "tusb.h"

#include "light-vm.h"

#define WS2812B_PIN 28

uint16_t bootProg[] = {
        OP_FLAGS(1),
        OP_LOAD_COLOR(0,0,10),
        OP_SET_LEDS(0b111'111'111'111), // right - front - left - back
        OP_FADE(50),
        OP_LOOP_END(0),
};

uint16_t mountProg[] = {
        OP_FLAGS(1),
        OP_LOAD_COLOR(0,10,0),
        OP_SET_LEDS(0b111'111'111'111), // right - front - left - back
        OP_FADE(50),
        OP_LOOP_END(0),
};

uint16_t suspendProg[] = {
        OP_FLAGS(1),
        OP_LOAD_COLOR(0,0,0),
        OP_SET_LEDS(0b111'111'111'111), // right - front - left - back
        OP_FADE(0),
        OP_LOOP_END(0),
};

uint16_t errorProg[] = {
        OP_FLAGS(0),

        OP_LOAD_COLOR(31, 0, 0),
        OP_SET_LEDS(0b000'000'000'111),
        OP_LOAD_COLOR(0, 0, 0),
        OP_SET_LEDS(0b111'111'111'000),
        OP_FADE(100),

        OP_LOAD_COLOR(31, 0, 0),
        OP_SET_LEDS(0b000'000'111'000),
        OP_LOAD_COLOR(0, 0, 0),
        OP_SET_LEDS(0b111'111'000'111),
        OP_FADE(100),

        OP_LOAD_COLOR(31, 0, 0),
        OP_SET_LEDS(0b000'111'000'000),
        OP_LOAD_COLOR(0, 0, 0),
        OP_SET_LEDS(0b111'000'111'111),
        OP_FADE(100),

        OP_LOAD_COLOR(31, 0, 0),
        OP_SET_LEDS(0b111'000'000'000),
        OP_LOAD_COLOR(0, 0, 0),
        OP_SET_LEDS(0b000'111'111'111),
        OP_FADE(100),

        OP_RESTART(),
};

int main()
{
    board_init();
    tusb_init();

    gpio_init(WS2812B_PIN);
    gpio_set_dir(WS2812B_PIN, GPIO_OUT);

    loadProgram(bootProg, sizeof(bootProg));

    for(;;)
    {
        tud_task();
        updateLedProgram();
        setAllLeds();
        sleep_ms(1);
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    if(bufsize >= 1)
    {
        switch(buffer[0])
        {
            case 0: // stop vm
                vm.running = false;
                break;

            case 1: // start vm
                vm.ip = 0;
                vm.running = true;
                break;

            case 2: { // load ops
                if (bufsize < 4) {
                    loadProgram(errorProg, sizeof(errorProg));
                    break;
                }
                uint16_t addr = (uint16_t(buffer[2]) << 8) | (buffer[1]);
                memcpy(((uint8_t*)&vm.ledProgram[0])+addr, buffer+3, bufsize-3);
                break;
            }

            default:
                loadProgram(errorProg, sizeof(errorProg));
                break;
        }
    }

    tud_hid_report(0, NULL, 0);
}

// Invoked when device is mounted
void tud_mount_cb()
{
    loadProgram(mountProg, sizeof(mountProg));
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    loadProgram(errorProg, sizeof(errorProg));
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    // we should go to low power mode too.
    loadProgram(suspendProg, sizeof(suspendProg));
}

// Invoked when usb bus is resumed
//void tud_resume_cb(void)
//{
//    ledColors[0][0] = 0;
//    ledColors[0][1] = 0;
//    ledColors[0][2] = 255;
//}

/*
Instructions
0000 rrrrggggbbbb - load color
0001 llllllllllll - set leds with mask
0002 dddddddddddd - fade (delay in ms, limit of 4095 ms)
0003 loop


Commands:
00 run
01 stop
02 <16 bit addr> poke

 */
