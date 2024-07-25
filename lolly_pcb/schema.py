from skip import Symbol, Schematic
import os

# this assumes the following
# * a template file that contains one led that can be cloned - named "T"
# * we assume 4 pin leds

s = Schematic('mini_lolly_pcb/template.kicad_sch')

template = s.symbol.D1
gnd = s.symbol.PWR01
vcc = s.symbol.PWR02

# gen leds
leds = []
h = {}
led_count = 21*3


def connect(led1, led2):
    w = s.wire.new()
    w.start_at(led1.pin.DOUT)
    w.end_at(led2.pin.DIN)


led_indexes = []

for stripidx, j in enumerate([1, 9, 17, 4, 12, 20, 7, 15, 2, 10, 18, 5, 13, 21, 8, 16, 3, 11, 19, 6, 14]):
    strip = []
    for i in range(j, led_count + 1, 21):
        strip.append(i)
    if stripidx % 2 == 1:
        strip.reverse()
    led_indexes.append(strip)


for idx, strip in enumerate(led_indexes):
    column = []
    for j in strip:
        led = template.clone()
        id = j
        led.setAllReferences(f'D{id}')
        led.lid = id
        led.move(idx*5, j*5)
        column.append(led)
        h[id] = led
    leds.append(column)

for idx in range(2, led_count+1):
    connect(h[idx-1], h[idx])
#         # and connect them with some wires
# for col in leds:
#     for led1, led2 in zip(col, col[1:]):
#         connect(led1, led2)

for col in leds:
    for led in col:
        w = s.wire.new()
        w.start_at(led.pin.VDD)
        w.end_at(vcc)
        w = s.wire.new()
        w.start_at(led.pin.VSS)
        w.end_at(gnd)


template.delete()
s.write('mini_lolly_pcb/mini_lolly_pcb.kicad_sch')
