import { gammaDecode, gammaEncode } from "./math.js"

let imageID = "teddy-image"
imageID = "emoji-image"

let angle = 0

export function mapTeddieImageLedColors(config) {
	const ledColors = []
	for (let i = 0; i < config.ledCount; i++) {
		const c = config.coordinates[i]

		const scale = 1
		const ledScale = 2
		const yOffset = 0
		const xOffset = 0

		const relativeX = (c.x / scale + config.canvasSize / 2) / config.canvasSize + xOffset
		const relativeY = (c.y / scale + config.canvasSize / 2) / config.canvasSize + yOffset
		const size = (ledScale * config.ledSizeMm * config.pixelPerMm) / config.canvasSize

		const rotated = rotatePoint(relativeX, relativeY, angle)

		const color = averageColor(imageID, rotated.x, rotated.y, size)
		// console.log(relativeX, relativeY, size, "==", color);
		ledColors.push(color)
	}

	angle += 0.02
	return ledColors
}

function rotatePoint(x, y, angle) {
	// Translate the point to the origin
	let translatedX = x - 0.5
	let translatedY = y - 0.5

	// Rotate the point
	let rotatedX = translatedX * Math.cos(angle) - translatedY * Math.sin(angle)
	let rotatedY = translatedX * Math.sin(angle) + translatedY * Math.cos(angle)

	// Translate the point back
	let finalX = rotatedX + 0.5
	let finalY = rotatedY + 0.5

	return { x: finalX, y: finalY }
}

function averageColor(imageID, x, y, size) {
	var img = document.getElementById(imageID)
	var img
	var canvas = document.getElementById("img-mapping-canvas")
	var ctx = canvas.getContext("2d")

	// Set canvas dimensions to match the image
	canvas.width = img.width
	canvas.height = img.height

	// Draw the image on the canvas
	ctx.drawImage(img, 0, 0)

	size = Math.round(size * canvas.width)
	x = Math.round(x * canvas.width - size / 2)
	y = Math.round(y * canvas.height - size / 2)

	// Get the image data for the defined area
	var imageData = ctx.getImageData(x, y, size, size)
	var data = imageData.data

	var r = 0,
		g = 0,
		b = 0,
		a = 0
	var count = 0

	// Iterate over each pixel
	for (var i = 0; i < data.length; i += 4) {
		r += gammaDecode(data[i]) // Red
		g += gammaDecode(data[i + 1]) // Green
		b += gammaDecode(data[i + 2]) // Blue
		a += data[i + 3] // Alpha
		count++
	}

	// Calculate the average in linear space
	r /= count
	g /= count
	b /= count
	a /= count

	// Convert back to sRGB
	r = gammaEncode(r)
	g = gammaEncode(g)
	b = gammaEncode(b)

	// Output the result
	const color = `rgba(${Math.round(r)}, ${Math.round(g)}, ${Math.round(b)}, ${Math.round(a / 255)})`
	return color
}
