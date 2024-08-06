import { drawLolly, setupCoordinates } from "./lib/draw.js"

import { ColorRings } from "./patterns/color_rings.js"
import { ColorArms } from "./patterns/color_arms.js"
import { RotatingImage } from "./patterns/rotating_image.js"

const activePattern = "rings"
const patterns = {
	rings: new ColorRings(),
	arms: new ColorArms(),
	rotatingImage: new RotatingImage(),
}

const fpsSelector = "#fps"
const simulateBigLolly = false

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
}

if (simulateBigLolly) {
	config.ledCount = 512
	config.spacingMm = 5.5 / 2.75
	config.ledSizeMm = 4 / 2.75
}

setupCoordinates(config)

let runAnimation = false

document.querySelector("#toggle-animation").addEventListener("click", () => {
	runAnimation = !runAnimation
	if (runAnimation) {
		requestAnimationFrame(drawFrame)
	} else {
		document.querySelector(fpsSelector).innerText = "FPS: paused"
	}
	document.querySelector("#toggle-animation").innerText = runAnimation ? "Pause Animation" : "Start Animation"
})

function drawFrame() {
	const ledColors = patterns[activePattern].renderColors(config)

	drawLolly(config, ledColors)

	if (runAnimation) {
		requestAnimationFrame(drawFrame)
	}
	updateFPSDisplay()
}

requestAnimationFrame(drawFrame)

let lastFPSUpdate = performance.now()
let frames = 0

function updateFPSDisplay() {
	frames++

	const now = performance.now()
	if (now - lastFPSUpdate < 500) {
		return
	}

	const timeDiff = now - lastFPSUpdate
	const fps = 1000 / (timeDiff / frames)

	lastFPSUpdate = now
	frames = 0

	document.querySelector(fpsSelector).innerText = "FPS: " + fps.toFixed(1)
}
