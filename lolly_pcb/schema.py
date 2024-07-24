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
led_count = 64


def connect(led1, led2):
    w = s.wire.new()
    w.start_at(led1.pin.DOUT)
    w.end_at(led2.pin.DIN)


for i in range(led_count):
    id = i + 1
    column = []
    print(id)
    led = template.clone()
    led.setAllReferences(f'D{id}')
    led.lid = id
    led.move(id*20, 0)
    h[id] = led
    leds.append(led)

# and connect them with some wires
print(leds)
for i in range(led_count-1):
    print(i)
    led1 = leds[i]
    led2 = leds[i+1]

    connect(led1, led2)

for led in leds:
    w = s.wire.new()
    w.start_at(led.pin.VDD)
    w.end_at(vcc)
    w = s.wire.new()
    w.start_at(led.pin.VSS)
    w.end_at(gnd)


template.delete()
s.write('mini_lolly_pcb/mini_lolly_pcb.kicad_sch')
