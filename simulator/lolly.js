import { drawLolly } from "./draw.js";
import { updateLedColors } from "./pattern.js";

const fpsSelector = "#fps";
const simulateBigLolly = false;

const config = {
  ledCount: 63,
  radiusMm: 47,
  spacingMm: 5.5,
  ledSizeMm: 4,
  blurRadius: 1.5,
  pixelPerMm: 8.2,
  printIndex: false,
  pattern: {
    numArms: 7,
  },
};

if (simulateBigLolly) {
  config.ledCount = 512;
  config.spacingMm = 5.5 / 2.75;
  config.ledSizeMm = 4 / 2.75;
}

function drawFrame() {
  const ledColors = updateLedColors(config);
  drawLolly(config, ledColors);

  requestAnimationFrame(drawFrame);
  updateFPSDisplay();
}

requestAnimationFrame(drawFrame);

let lastFPSUpdate = performance.now();
let frames = 0;

function updateFPSDisplay() {
  frames++;

  const now = performance.now();
  if (now - lastFPSUpdate < 500) {
    return;
  }

  const timeDiff = now - lastFPSUpdate;
  const fps = 1000 / (timeDiff / frames);

  lastFPSUpdate = now;
  frames = 0;

  document.querySelector(fpsSelector).innerText = "FPS: " + fps.toFixed(1);
}
