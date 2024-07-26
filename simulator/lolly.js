import { drawLolly } from "./draw.js";
import { updateLedColors } from "./pattern.js";

const config = {
  ledCount: 63,
  radiusMm: 47,
  spacingMm: 5.5,
  ledSizeMm: 4,
  blurRadius: 1,
  pixelPerMm: 8.2,
  printIndex: false,
  pattern: {
    numArms: 21,
  },
};

function draw() {
  const ledColors = updateLedColors(config);
  drawLolly(config, ledColors);
  requestAnimationFrame(draw);
}

requestAnimationFrame(draw);
