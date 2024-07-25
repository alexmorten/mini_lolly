import pcbnew
import math


offset = 10
SCALE = 1e6
LED_SPACING = 5.5
VIA_SPACING = 2.0

extra_rotate = [16, 33, 40, 57]


def draw_via(board, pos, layer, net, diameter=0.8 * SCALE, drill=0.4 * SCALE):
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(pos)
    via.SetLayer(layer)
    via.SetWidth(int(diameter))
    via.SetDrill(int(drill))
    board.Add(via)
    via.SetNet(net)
    return via


def draw_track(board, start, end, layer, net, width=None):
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(start)
    track.SetEnd(end)
    track.SetLayer(layer)
    board.Add(track)
    track.SetNet(net)
    if width:
        track.SetWidth(int(width))
    return track


def vector(length, angle, scale=SCALE):
    return pcbnew.VECTOR2I(int(length * math.sin(angle) * scale), int(length * math.cos(angle) * scale))


def pads_for_idx(board, led, pads, idx):
    prev_idx = idx - 1
    prev_led = board.FindFootprintByReference("D{}".format(prev_idx))
    prev_pads = prev_led.Pads()
    # raise BaseException(prev_net_name)
    # padnames = [pad.GetPadName() for pad in prev_pads]
    # raise BaseException(padnames)
    prev_dout_pad = [pad for pad in prev_pads if pad.GetPadName(
    ) == "2"][0]

    din_pad = [pad for pad in pads if pad.GetPadName(
    ) == "4"][0]

    return prev_dout_pad, din_pad


class SpiralLEDPlacement(pcbnew.ActionPlugin):
    def defaults(self):
        self.name = "Spiral LED Placement"
        self.category = "Modify PCB"
        self.description = "Place LEDs in a spiral pattern"

    def Run(self):
        board = pcbnew.GetBoard()
        # start_radius = 5    # Starting radius in mm
        # spacing = 5          # Spacing between each turn in mm
        center_x = 150 * SCALE     # Center x coordinate in nm (150 mm)
        center_y = 100 * SCALE     # Center y coordinate in nm (100 mm)
        selected_footprints = [
            module for module in board.Footprints() if module.IsSelected()]
        LED_MAP = []
        LED_ROTATE = []
        LEDS = len(selected_footprints)
        for stripidx, j in enumerate([1, 9, 17, 4, 12, 20, 7, 15, 2, 10, 18, 5, 13, 21, 8, 16, 3, 11, 19, 6, 14]):
            strip = []
            for i in range(j, LEDS + 1, 21):
                strip.append(i)
                LED_ROTATE.append(180 if stripidx % 2 == 0 else 0)
            if stripidx % 2 == 1:
                strip.reverse()
            LED_MAP += strip


#

        for led_idx in range(1, LEDS + 1):
            n = LED_MAP[led_idx - 1]
            r = LED_SPACING * math.sqrt(n)
            deg = n * 137.508
            angle = (deg / 180) * math.pi
            pos = pcbnew.VECTOR2I(
                int(r * math.cos(angle) * SCALE + center_x), int(r * math.sin(angle) * SCALE + center_y))
            led = board.FindFootprintByReference("D{}".format(led_idx))
            led.SetPosition(pos)

            rotate = LED_ROTATE[led_idx - 1] - deg - 10
            if led_idx in extra_rotate:
                rotate += 90
            # manually correct some leds on the boarder
            # if led_idx in reverse:
            #     rotate += 180
            rotate_rad = (rotate / 180.) * math.pi
            offset = vector(2, rotate_rad)
            led.SetOrientationDegrees(rotate)
            pads = led.Pads()
            gnd_pad = [pad for pad in pads if pad.GetNet().GetNetname()
                       == "GND"][0]
            via = draw_via(board, gnd_pad.GetCenter() + offset,
                           pcbnew.F_Cu, gnd_pad.GetNet())
            draw_track(board, gnd_pad.GetCenter(), via.GetCenter(),
                       pcbnew.F_Cu, gnd_pad.GetNet(), 0.8 * SCALE)

            if (led_idx == 1):
                continue

            prev_dout_pad, din_pad = pads_for_idx(board, led, pads, led_idx)

            draw_track(board, prev_dout_pad.GetCenter(), din_pad.GetCenter(),
                       pcbnew.F_Cu, prev_dout_pad.GetNet(), 0.25 * SCALE)

#

        # for i, led in enumerate(selected_footprints):
        #     led_idx = i + 1

        #     r = LED_SPACING * math.sqrt(i)
        #     deg = i * 137.508
        #     angle = (deg / 180) * math.pi
        #     pos = pcbnew.VECTOR2I(
        #         int(r * math.cos(angle) * SCALE + center_x), int(r * math.sin(angle) * SCALE + center_y))
        #     led = board.FindFootprintByReference("D{}".format(led_idx))
        #     led.SetPosition(pos)

        #     pads = led.Pads()

        #     offset = pcbnew.VECTOR2I(int(VIA_SPACING * SCALE), int(0))

        pcbnew.Refresh()


# Register the plugin so it shows up in PCBNew under Tools -> External Plugins
SpiralLEDPlacement().register()
