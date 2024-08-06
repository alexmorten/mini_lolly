import math

offset = 10
SCALE = 1e6
LED_SPACING = 5.5
VIA_SPACING = 2.0


center_x = 150 * SCALE     # Center x coordinate in nm (150 mm)
center_y = 100 * SCALE     # Center y coordinate in nm (100 mm)
LED_MAP = []
LED_ROTATE = []
LEDS = 63
for stripidx, j in enumerate([1, 9, 17, 4, 12, 20, 7, 15, 2, 10, 18, 5, 13, 21, 8, 16, 3, 11, 19, 6, 14]):
    strip = []
    for i in range(j, LEDS + 1, 21):
        strip.append(i)
        LED_ROTATE.append(180 if stripidx % 2 == 0 else 0)
    if stripidx % 2 == 1:
        strip.reverse()
    LED_MAP += strip


positions = []

for led_idx in range(1, LEDS + 1):
    n = LED_MAP[led_idx - 1]
    r = LED_SPACING * math.sqrt(n)
    deg = n * 137.508
    angle = (deg / 180) * math.pi
    pos = (
        float(r * math.cos(angle) * SCALE),
        float(r * math.sin(angle) * SCALE),
    )

    positions.append(pos)

max_x = max([abs(x) for x, y in positions])
max_y = max([abs(y) for x, y in positions])

print(max_x, max_y)

divider = float(max(max_x, max_y))

print(divider)

mapped_positions = [(x/divider, y/divider) for x, y in positions]
print(mapped_positions)
for x, y in mapped_positions:
    print("{{{},{}}},".format(x, y))
