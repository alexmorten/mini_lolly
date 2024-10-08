import { radians, goldenAngle } from "./math.js"

export function drawLolly(config, ledColors) {
	var canvas = document.getElementById("lollycanvas")
	var ctx = canvas.getContext("2d")

	const r = config.radiusMm * config.pixelPerMm
	// console.log("r", r);
	ctx.fillStyle = "black"
	ctx.beginPath()
	// x, y, radius, startAngle, endAngle
	ctx.arc(r, r, r, 0, 2 * Math.PI)
	ctx.fill()
	ctx.clip()

	drawLEDs(config, ledColors, ctx)
}

export function setupCoordinates(config) {
	const ledCoords = []
	for (let n = 1; n <= config.ledCount; n++) {
		let angle = radians(n * goldenAngle)
		let radius = Math.sqrt(n) * config.spacingMm * config.pixelPerMm
		let x = radius * Math.cos(angle)
		let y = radius * Math.sin(angle)
		// console.log("LED", n, "radius", radius, "x", x, "y", y);
		ledCoords.push({ x, y })
	}
	config.coordinates = ledCoords

	const minX = Math.min(...ledCoords.map(c => c.x))
	const maxX = Math.max(...ledCoords.map(c => c.x))
	const minY = Math.min(...ledCoords.map(c => c.y))
	const maxY = Math.max(...ledCoords.map(c => c.y))

	// map coordinates to -1 and 1 for 2D patterns
	const scale = Math.max(maxX, -minX, maxY, -minY)

	const coords2d = []
	for (let c of ledCoords) {
		const x = c.x / scale
		const y = c.y / scale
		coords2d.push({ x, y })
	}
	config.coordinates2D = coords2d
}

function drawLEDs(config, ledColors, ctx) {
	ctx.font = "20px Arial"
	ctx.textAlign = "center"
	ctx.textBaseline = "middle"

	for (let i = 0; i < config.coordinates.length; i++) {
		ctx.fillStyle = ledColors[i]
		const x = config.coordinates[i].x + config.radiusMm * config.pixelPerMm
		const y = config.coordinates[i].y + config.radiusMm * config.pixelPerMm
		const radius = (config.ledSizeMm / 2) * config.pixelPerMm

		// LED glow:
		// Draws multiple circles with different transparency levels.
		// The bigger the circle, the more transparent it is.
		// In the end they get blurred tugether via a CSS filter on the whole canvas.
		const alphaValues = [0.1, 0.2, 0.4, 1]
		const sizeMultiples = [4.4, 3.3, 2.2, 1]
		for (let j = 0; j < alphaValues.length; j++) {
			ctx.globalAlpha = alphaValues[j]
			ctx.beginPath()
			ctx.arc(x, y, radius * sizeMultiples[j], 0, 2 * Math.PI)
			ctx.fill()
		}

		if (!config.printIndex) {
			continue
		}

		// LED number
		ctx.fillStyle = "black"
		ctx.fillText(i + 1, x, y)
	}
}
