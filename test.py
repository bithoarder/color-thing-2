# Install python3 HID package https://pypi.org/project/hid/
import binascii

import hid
import struct
import time


class LightProg:
    def __init__(self):
        self.prog = []

    def compile(self):
        return b''.join(struct.pack('<H', v) for v in self.prog)

    def restart(self):
        self.prog.append(0)
        return self

    def load_color(self, r, g, b):
        assert 0 <= r < 32 and 0 <= g < 32 and 0 <= b < 32
        self.prog.append(0b1000000000000000 | (r << 10) | (g << 5) | (b << 0))
        return self

    def set_leds(self, mask):
        """When the wire is located in the back: the order is
        0:back-bottom,  1:back-middle,   2:back-top
        3:right-top,     4:right-middle,   5:right-bottom
        6:front-bottom, 7:front-middle,  8:front-top
        9:left-top,   10:left-middle, 11:left-bottom
        """
        assert 0 <= mask <= 0b111111111111
        self.prog.append((0b0001 << 12) | mask)
        return self

    def fade(self, duration):
        assert 0 <= duration <= 0b111111111111
        self.prog.append((0b0010 << 12) | duration)
        return self

    def set_flags(self, flags):
        assert 0 <= flags <= 0b111111111111
        self.prog.append((0b0011 << 12) | flags)
        return self

    def set_linear(self):
        self.set_flags(0)
        return self

    def set_gamma(self):
        self.set_flags(1)
        return self

    def loop_start(self, reg, count):
        assert 0 <= reg <= 15
        assert 1 <= count <= 0xff
        self.prog.append((0b0100 << 12) | (reg << 8) | count)
        return self

    def loop_end(self, reg):
        assert 0 <= reg <= 15
        self.prog.append((0b0101 << 12) | (reg << 8) | 0)
        return self


test1_prog = LightProg() \
    .set_gamma() \
    .load_color(31, 0, 0) \
    .set_leds(0b100001100001) \
    .load_color(0, 31, 0) \
    .set_leds(0b010010010010) \
    .load_color(0, 0, 31) \
    .set_leds(0b001100001100) \
    .fade(1000) \
    .load_color(31, 31, 31) \
    .set_leds(0b111111111111) \
    .fade(50) \
    .fade(100) \
    .load_color(0, 0, 0) \
    .set_leds(0b111111111111) \
    .fade(200) \
    .restart() \
    .compile()

red_rotor_prog = LightProg() \
    .set_linear() \
    .load_color(31, 0, 0) \
    .set_leds(0b000000000111) \
    .load_color(0, 0, 0) \
    .set_leds(0b111111111000) \
    .fade(100) \
    .load_color(31, 0, 0) \
    .set_leds(0b000000111000) \
    .load_color(0, 0, 0) \
    .set_leds(0b111111000111) \
    .fade(100) \
    .load_color(31, 0, 0) \
    .set_leds(0b000111000000) \
    .load_color(0, 0, 0) \
    .set_leds(0b111000111111) \
    .fade(100) \
    .load_color(31, 0, 0) \
    .set_leds(0b111000000000) \
    .load_color(0, 0, 0) \
    .set_leds(0b000111111111) \
    .fade(100) \
    .restart() \
    .compile()

loop_test_prog = LightProg() \
    .set_gamma() \
    .loop_start(0, 2) \
    .load_color(31, 0, 0) \
    .set_leds(0b111111111111) \
    .fade(250) \
    .load_color(0, 0, 0) \
    .set_leds(0b111111111111) \
    .fade(250) \
    .loop_end(0) \
    .loop_start(0, 3) \
    .load_color(0, 31, 0) \
    .set_leds(0b111111111111) \
    .fade(250) \
    .load_color(0, 0, 0) \
    .set_leds(0b111111111111) \
    .fade(250) \
    .loop_end(0) \
    .restart() \
    .compile()

h = hid.device()
h.open(0xcafe, 0x4004)
print(h.get_manufacturer_string(), h.get_product_string())
h.set_nonblocking(1)


def load_prog(prog):
    h.write(b'\x00')  # stop running prog
    # max 64 bytes per cmd:
    for i in range(0, len(prog), 60):
        j = min(i + 60, len(prog))
        cmd = struct.pack('<BH', 2, i) + prog[i:j]
        print(i, j, binascii.hexlify(cmd))  # , prog[i:j])
        h.write(cmd)
    h.write(b'\x01')  # start prog


#load_prog(test1_prog)
load_prog(red_rotor_prog)
#load_prog(loop_test_prog)
