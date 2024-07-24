import pcbnew
import math


offset = 10
SCALE = 1e6
LED_SPACING = 9
VIA_SPACING = 2.0


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

        for i, led in enumerate(selected_footprints):
            led_idx = i + 1
            # angle = 0.1 * i  # Increase the angle to spread out the spiral
            # radius = start_radius + spacing * i  # Increase radius gradually
            # # Convert mm to nm for pcbnew
            # x = center_x + int(radius * math.cos(angle) * 1e6)
            # y = center_y + int(radius * math.sin(angle) * 1e6)

            r = LED_SPACING * math.sqrt(i)
            deg = i * 137.508
            angle = (deg / 180) * math.pi
            pos = pcbnew.VECTOR2I(
                int(r * math.cos(angle) * SCALE + center_x), int(r * math.sin(angle) * SCALE + center_y))
            led = board.FindFootprintByReference("D{}".format(led_idx))
            led.SetPosition(pos)

            pads = led.Pads()
            gnd_pad = [pad for pad in pads if pad.GetNet().GetNetname()
                       == "GND"][0]
            offset = pcbnew.VECTOR2I(int(VIA_SPACING * SCALE), int(0))
            # Attempt to load the LED footprint from the library
            # led.SetPosition(pcbnew.VECTOR2I(int(x), int(y)))
            via = draw_via(board, gnd_pad.GetCenter() + offset,
                           pcbnew.F_Cu, gnd_pad.GetNet())
            draw_track(board, gnd_pad.GetCenter(), via.GetCenter(),
                       pcbnew.F_Cu, gnd_pad.GetNet(), 0.8 * SCALE)

            if (i == 0):
                continue

            prev_dout_pad, din_pad = pads_for_idx(board, led, pads, led_idx)

            draw_track(board, prev_dout_pad.GetCenter(), din_pad.GetCenter(),
                       pcbnew.F_Cu, prev_dout_pad.GetNet(), 0.25 * SCALE)

        pcbnew.Refresh()


# Register the plugin so it shows up in PCBNew under Tools -> External Plugins
SpiralLEDPlacement().register()
