import pcbnew
import math


def place_leds_in_spiral(board, led_count, start_radius, spacing,
                         center_x, center_y):
    """ Place LEDs in a logarithmic spiral pattern. """
    for i in range(led_count):
        # Calculate the angle and radius for the spiral
        angle = 0.1 * i  # Increase the angle to spread out the spiral
        radius = start_radius + spacing * i  # Increase radius gradually

        # Convert polar coordinates (radius and angle) to Cartesian coordinates
        # (x and y)
        # Convert mm to nm for pcbnew
        x = center_x + int(radius * math.cos(angle) * 1e6)
        y = center_y + int(radius * math.sin(angle) * 1e6)

        # Create a new LED module
        component_lib_path = "/Users/alex/private/mini_lolly/lolly_pcb/mini_lolly_pcb/WS2812B.pretty"
        led = pcbnew.FootprintLoad(
            component_lib_path, "WS2812B")
        if not led:
            print("Error loading LED footprint")
            exit(1)

        # Set LED position and orientation (0 degrees)
        led.SetPosition(pcbnew.wxPoint(x, y))
        led.SetOrientation(0)

        # Add the LED to the board
        board.Add(led)


# Load current board
board = pcbnew.GetBoard()

# Parameters for the spiral
LED_COUNT = 20       # Total number of LEDs
START_RADIUS = 10    # Starting radius in mm
SPACING = 5          # Spacing between each turn in mm
CENTER_X = 150e6     # Center x coordinate in nm (150 mm)
CENTER_Y = 100e6     # Center y coordinate in nm (100 mm)

# Place the LEDs
place_leds_in_spiral(board, LED_COUNT, START_RADIUS,
                     SPACING, CENTER_X, CENTER_Y)

# Refresh and save the board
pcbnew.Refresh()
board.Save(
    "/Users/alex/private/mini_lolly/lolly_pcb/mini_lolly_pcb/generated.kicad_pcb")
