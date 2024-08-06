// Based on this p5.js sketch:
// https://editor.p5js.org/dynamix/sketches/gNS2GeqVQ

export class ColorArms {
	hueOffset = 0

	hueStepSize = 2
	numArms = 7

	parameters() {
		return [
			{
				name: "Num Arms",
				type: "number",
				default: 7,
				update: val => {
					this.numArms = val
				},
			},
			{
				name: "Hue Step Size",
				type: "number",
				default: 2,
				update: val => {
					this.hueStepSize = val
				},
			},
		]
	}

	renderColors(config) {
		const ledColors = []
		for (let i = 0; i < config.ledCount; i++) {
			const armIndex = i % this.numArms
			const armHue = (armIndex / this.numArms) * 360
			const hueValue = (armHue + this.hueOffset) % 360
			ledColors.push(`hsl(${hueValue}, 100%, 50%)`)
		}
		this.hueOffset += this.hueStepSize
		return ledColors
	}
}
