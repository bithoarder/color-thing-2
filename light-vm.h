#include <stdint.h>

const int leds = 12;

struct vm {
    uint8_t prevLedColors[leds][3];
    uint8_t nextLedColors[leds][3];

    uint16_t ledProgram[32*1024];

    uint16_t ip = 0;
    bool running = true;
    uint8_t loadedColor[3] = {0, 0, 0};
    uint32_t fadeStart = 0, fadeEnd = 0;
    uint16_t flags = 1;
    uint8_t loopCounters[16];
    uint16_t loopAddrs[16];
};

extern vm vm;

#define OP_RESTART() (0<<12)                                                           // 0000 0000 0000 0000
#define OP_LOAD_COLOR(r,g,b) ((1<<15)|((r)<<10)|((g)<<5)|((b)<<0))                     // 1rrr rrgg gggb bbbb
#define OP_SET_LEDS(mask) ((0b0001<<12)|(mask))                                        // 0001 llll llll llll
#define OP_FADE(duration) ((0b0010<<12)|(duration))                                    // 0010 dddd dddd dddd
#define OP_FLAGS(flags) ((0b0011<<12)|(flags))                                         // 0011 ffff ffff ffff
#define OP_LOOP_START(index, iterations) ((0b0100<<12)|((index)<<8)|((iterations)<<0)) // 0100 rrrr nnnn nnnn
#define OP_LOOP_END(index) ((0b0101<<12)|((index)<<8)|0)                               // 0101 rrrr 0000 0000

extern void setAllLeds();
extern void updateLedProgram();
extern void loadProgram(const void *prog, int progSize);
