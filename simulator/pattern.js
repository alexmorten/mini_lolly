let hueOffset = 0;

export function updateLedColors(config) {
  const ledColors = [];
  for (let i = 0; i < config.ledCount; i++) {
    let armIndex = i % config.pattern.numArms;
    // let hueValue = (map(armIndex, 0, numArms, 0, 255) + hueOffset) % 255;
    let hueValue =
      ((armIndex * 255) / config.pattern.numArms + hueOffset) % 255;
    ledColors.push(`hsl(${hueValue}, 100%, 50%)`);
  }
  hueOffset += 1;
  return ledColors;
}
