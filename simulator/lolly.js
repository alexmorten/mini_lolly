import { drawLolly } from "./draw.js";
import { updateLedColors } from "./pattern.js";

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

let lastFrameTime = performance.now();

function draw() {
  const ledColors = updateLedColors(config);
  drawLolly(config, ledColors);

  requestAnimationFrame(draw);
  updateFPS(performance.now());
}

requestAnimationFrame(draw);

function updateFPS(currentTime) {
  const timeDifference = currentTime - lastFrameTime;
  const fps = 1000 / timeDifference;
  lastFrameTime = currentTime;

  // Display FPS (you can log it, or update a DOM element)
  //   console.log(fps.toFixed(2)); // for example, displaying to console
  document.getElementById("fps").innerText = "FPS: " + fps.toFixed(1); // for example, updating a DOM element
}
