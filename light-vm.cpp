#include "light-vm.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#define WS2812B_PIN 28

static inline void delay3cycles(uint32_t count) {
    asm volatile (
            "1: sub %0, %0, #1 \n"
            "bne 1b\n"
            : "+l" (count) // 0=read/write, r=general register, l=low register
            );
}

// python3 -c "from math import *; print([int(pow(i/255, 1.8)*255+0.5) for i in range(256)])"
const uint8_t gamma8[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 84, 85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120, 121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143, 145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168, 170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195, 197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218,
                          220, 222, 224, 226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255};

uint8_t ledColors[leds][3]; // R,G,B

void setAllLeds() {
    for(int ledIndex = 0; ledIndex < leds; ledIndex++) {
        uint8_t (&color)[3] = ledColors[ledIndex];
        for(int channelIndex = 0; channelIndex < 3; channelIndex++) {
            // my strip have R,B swapped :/
            int remappedChannelIndex = channelIndex ^ (1 - (channelIndex >> 1));
            uint8_t channel = color[remappedChannelIndex];
            for(int bitIndex = 7; bitIndex >= 0; bitIndex--) {
                uint8_t b = (channel >> bitIndex) & 1;

                gpio_put(WS2812B_PIN, 1);
                delay3cycles(b ? 37 : 15); //  0.9us / 0.35us  ((125000000*0.35e-6)/3)

                gpio_put(WS2812B_PIN, 0);
                delay3cycles(b ? 15 : 37); // 0.35us / 0.9us
            }
        }
    }

    // reset, 60us is a log too low
    sleep_us(80);
}

struct vm vm;

void updateLedProgram() {
    if(!vm.running)
        return;

    auto now = to_ms_since_boot(get_absolute_time());

    if(now > vm.fadeEnd && vm.fadeStart != 0 && vm.fadeEnd != 0) {
        // prev fade done
        memcpy(&vm.prevLedColors[0][0], &vm.nextLedColors[0][0], sizeof(vm.prevLedColors));
        vm.fadeEnd = vm.fadeStart = 0;
    }

    if(vm.fadeStart == vm.fadeEnd) {
        uint16_t op = vm.ledProgram[(vm.ip++) % (sizeof(vm.ledProgram) / sizeof(vm.ledProgram[0]))];
        if((op & 0b1000'0000'0000'0000) == 0b1000'0000'0000'0000) {
            // load color
            uint16_t r = (op >> 10) & 0x1f;
            uint16_t g = (op >> 5) & 0x1f;
            uint16_t b = (op >> 0) & 0x1f;
            vm.loadedColor[0] = (r<<3) | (r>>2);
            vm.loadedColor[1] = (g<<3) | (g>>2);
            vm.loadedColor[2] = (b<<3) | (b>>2);
        } else {
            switch(op >> 12) {
                case 0b0001: // set leds
                    for(int i = 0; i < leds; i++) {
                        if(op & (1 << i)) {
                            vm.nextLedColors[i][0] = vm.loadedColor[0];
                            vm.nextLedColors[i][1] = vm.loadedColor[1];
                            vm.nextLedColors[i][2] = vm.loadedColor[2];
                        }
                    }
                    break;

                case 0b0010: // start fade
                    vm.fadeStart = now;
                    vm.fadeEnd = now + (op & 0xfff);
                    break;

                case 0b0011: // flags
                    vm.flags = op & 0xfff;
                    break;

                case 0b0100: { // loop start
                    int loopIndex = (op >> 8) & 0xf;
                    vm.loopCounters[loopIndex] = op & 0xff;
                    vm.loopAddrs[loopIndex] = vm.ip;
                    break;
                }

                case 0b0101: { // loop end
                    int loopIndex = (op >> 8) & 0xf;
                    if(--vm.loopCounters[loopIndex] != 0)
                        vm.ip = vm.loopAddrs[loopIndex];
                    break;
                }

                default:
                    vm.ip = 0;
            }
        }
    } else {
        int a = (vm.fadeEnd - now) * 256 / (vm.fadeEnd - vm.fadeStart);
        int b = 255 - a;

        for(int i = 0; i < leds; i++) {
            for(int c = 0; c < 3; c++) {
                uint8_t v = (int(vm.prevLedColors[i][c]) * a + int(vm.nextLedColors[i][c]) * b) / 255;
                if(vm.flags & 1) v = gamma8[v];
                ledColors[i][c] = v;
            }
        }
    }
}

void loadProgram(const void *prog, int progSize) {
    vm.running = false;
    memcpy(vm.ledProgram, prog, progSize);
    vm.ip = 0;
    vm.running = true;
}

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
