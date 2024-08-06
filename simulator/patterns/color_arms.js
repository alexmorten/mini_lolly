// Based on this p5.js sketch:
// https://editor.p5js.org/dynamix/sketches/gNS2GeqVQ

export class ColorArms {
	hueOffset = 0

	renderColors(config) {
		const ledColors = []
		for (let i = 0; i < config.ledCount; i++) {
			const armIndex = i % config.pattern.numArms
			const armHue = (armIndex / config.pattern.numArms) * 360
			const hueValue = (armHue + this.hueOffset) % 360
			ledColors.push(`hsl(${hueValue}, 100%, 50%)`)
		}
		this.hueOffset += 2
		return ledColors
	}
}
