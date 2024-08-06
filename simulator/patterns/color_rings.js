export class ColorRings {
	tick = 0

	tickSpeed = 1
	ticksPerCycle = 240

	parameters() {
		return [
			{
				name: "Tick Speed",
				type: "number",
				default: 1,
				update: val => {
					this.tickSpeed = val
				},
			},
			{
				name: "Ticks Per Cycle",
				type: "number",
				default: 240,
				update: val => {
					this.ticksPerCycle = val
				},
			},
		]
	}

	renderColors(config) {
		this.tick += this.tickSpeed

		const border = this.tick / this.ticksPerCycle
		const ledColors = []

		for (let i = 0; i < config.ledCount; i++) {
			const { x, y } = config.coordinates2D[i]

			const distToBorder = Math.abs(Math.sqrt(x * x + y * y) - border)
			const hueValue = distToBorder * 360
			ledColors.push(`hsl(${hueValue}, 100%, 50%)`)
		}

		return ledColors
	}
}
