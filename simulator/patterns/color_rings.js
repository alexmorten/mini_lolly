export class ColorRings {
	tick = 0
	ticksPerCycle = 240

	renderColors(config) {
		this.tick++

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
