import { drawLolly, setupCoordinates } from "./draw.js";
import { updateLedColors } from "./pattern.js";
import { mapTeddieImageLedColors } from "./image_mapper.js";

const fpsSelector = "#fps";
const simulateBigLolly = false;
const mode = "teddie";

const config = {
  canvasSize: 800,
  ledCount: 63,
  radiusMm: 47,
  spacingMm: 5.5,
  ledSizeMm: 4,
  blurRadius: 1.5,
  pixelPerMm: 8.45,
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

setupCoordinates(config);

const img1 = document.querySelector("#teddy-image");
const img2 = document.querySelector("#emoji-image");

if (!img1.complete) {
  img1.addEventListener("load", () => {
    console.log("img1 loaded");
    requestAnimationFrame(drawFrame);
  });
}
if (!img2.complete) {
  img2.addEventListener("load", () => {
    console.log("img2 loaded");
    requestAnimationFrame(drawFrame);
  });
}

let runAnimation = true;

document.querySelector("#toggle-animation").addEventListener("click", () => {
  runAnimation = !runAnimation;
  if (runAnimation) {
    requestAnimationFrame(drawFrame);
  } else {
    document.querySelector(fpsSelector).innerText = "FPS: paused";
  }
  document.querySelector("#toggle-animation").innerText = runAnimation ? "Pause Animation" : "Start Animation";
});

function drawFrame() {
  let ledColors;
  switch (mode) {
    case "demo":
      ledColors = updateLedColors(config);
      break;
    case "teddie":
      if (!img1.complete) {
        return;
      }
      if (!img2.complete) {
        return;
      }
      ledColors = mapTeddieImageLedColors(config);
      break;
  }
  drawLolly(config, ledColors);

  if (runAnimation) {
    requestAnimationFrame(drawFrame);
  }
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
