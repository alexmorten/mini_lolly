import { drawLolly } from "./draw.js";

const config = {
  ledCount: 63,
  radiusMm: 47,
  spacingMm: 5.5,
  ledSizeMm: 4,
  blurRadius: 1,
  pixelPerMm: 8.2,
  printIndex: false,
};

drawLolly(config);
