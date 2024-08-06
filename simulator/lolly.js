import { drawLolly, setupCoordinates } from "./lib/draw.js"

import { ColorRings } from "./patterns/color_rings.js"
import { ColorArms } from "./patterns/color_arms.js"
import { RotatingImage } from "./patterns/rotating_image.js"

let activePattern = "ColorArms"
const patterns = {
	ColorRings: new ColorRings(),
	ColorArms: new ColorArms(),
	RotatingImage: new RotatingImage(),
}

const patternPicker = document.querySelector("#pattern-picker")
for (const pattern in patterns) {
	patternPicker.appendChild(createPatternButton(pattern))
}

function createPatternButton(pattern) {
	const button = document.createElement("button")
	button.className = "btn btn-primary"
	button.innerText = pattern
	button.addEventListener("click", () => {
		activePattern = pattern
		document.querySelector("#active-pattern").innerText = activePattern
	})
	return button
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

let runAnimation = true

if (runAnimation) {
	document.querySelector("#toggle-animation").innerText = "Pause Animation"
} else {
	document.querySelector("#toggle-animation").innerText = "Start Animation"
}

document.querySelector("#active-pattern").innerText = activePattern

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
