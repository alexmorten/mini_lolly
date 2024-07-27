// Based on this p5.js sketch:
// https://editor.p5js.org/dynamix/sketches/gNS2GeqVQ

let hueOffset = 0;

export function updateLedColors(config) {
  const ledColors = [];
  for (let i = 0; i < config.ledCount; i++) {
    const armIndex = i % config.pattern.numArms;
    const armHue = (armIndex / config.pattern.numArms) * 360;
    const hueValue = (armHue + hueOffset) % 360;
    ledColors.push(`hsl(${hueValue}, 100%, 50%)`);
  }
  hueOffset += 2;
  return ledColors;
}
