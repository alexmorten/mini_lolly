import { drawLolly, setupCoordinates } from "./lib/draw.js"

import { patterns } from "./patterns.js"

let activePattern = "ColorRings"

const patternPicker = document.querySelector("#pattern-picker")
for (const pattern in patterns) {
	patternPicker.appendChild(createPatternButton(pattern))
}

function createPatternButton(pattern) {
	const button = document.createElement("button")
	button.className = "btn btn-primary"
	button.innerText = pattern
	button.addEventListener("click", () => switchPattern(pattern))
	return button
}

function switchPattern(pattern) {
	activePattern = pattern
	updatePatternPickerActive(pattern)
	renderPatternParameters(pattern)
}

function updatePatternPickerActive(activePattern) {
	const divElement = document.querySelector("#pattern-picker")
	const buttons = divElement.querySelectorAll("button")
	for (const button of buttons) {
		if (button.innerText == activePattern) {
			button.classList.add("active")
		} else {
			button.classList.remove("active")
		}
	}
}

function renderPatternParameters(pattern) {
	const paramsEl = document.querySelector("#pattern-parameters")
	paramsEl.innerHTML = ""

	if (!patterns[pattern].parameters) {
		paramsEl.innerText = "No parameters for this pattern"
		return
	}
	const params = patterns[pattern].parameters()

	for (const param of params) {
		const input = document.createElement("input")
		input.type = param.type
		input.value = param.default
		input.addEventListener("input", event => {
			console.log("update", param.name, event.target.value)
			param.update(Number(event.target.value))
		})

		const label = document.createElement("label")
		label.innerText = param.name
		label.appendChild(input)

		paramsEl.appendChild(label)
	}
}

// setup pattern switcher and parameters
switchPattern(activePattern)

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
