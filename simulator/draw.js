import { radians, goldenAngle } from "./math.js";

let ledCoords = [];

export function drawLolly(config, ledColors) {
  if (ledCoords.length === 0) {
    calculateLedCoordinates(config);
  }
  var canvas = document.getElementById("lollycanvas");
  var ctx = canvas.getContext("2d");

  const r = config.radiusMm * config.pixelPerMm;
  // console.log("r", r);
  ctx.fillStyle = "black";
  ctx.beginPath();
  // x, y, radius, startAngle, endAngle
  ctx.arc(r, r, r, 0, 2 * Math.PI);
  ctx.fill();
  ctx.clip();

  drawLEDs(config, ledColors, ctx);
}

function calculateLedCoordinates(config) {
  for (let n = 1; n <= config.ledCount; n++) {
    let angle = radians(n * goldenAngle);
    let radius = Math.sqrt(n) * config.spacingMm * config.pixelPerMm;
    let x = radius * Math.cos(angle);
    let y = radius * Math.sin(angle);
    // console.log("LED", n, "radius", radius, "x", x, "y", y);
    ledCoords.push({ x, y });
  }
}

function drawLEDs(config, ledColors, ctx) {
  ctx.font = "20px Arial";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";

  const blurRadius = config.blurRadius * config.pixelPerMm;

  for (let i = 0; i < ledCoords.length; i++) {
    ctx.fillStyle = ledColors[i];
    var x = ledCoords[i].x + config.radiusMm * config.pixelPerMm;
    var y = ledCoords[i].y + config.radiusMm * config.pixelPerMm;

    // more blurred background
    ctx.filter = "brightness(200%) blur(" + blurRadius * 2 + "px)";
    ctx.beginPath();
    ctx.arc(x, y, (config.ledSizeMm * config.pixelPerMm) / 1, 0, 2 * Math.PI);
    ctx.fill();

    // less blurred foreground
    ctx.filter = "brightness(500%) blur(" + blurRadius + "px)";
    ctx.beginPath();
    ctx.arc(x, y, (config.ledSizeMm / 2) * config.pixelPerMm, 0, 2 * Math.PI);
    ctx.fill();

    if (!config.printIndex) {
      continue;
    }
    // LED number
    ctx.filter = "none";
    ctx.fillStyle = "black";
    ctx.fillText(i + 1, x, y);
  }
}
