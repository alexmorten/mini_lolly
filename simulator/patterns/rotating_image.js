import { mapTeddieImageLedColors } from "../lib/image_mapper.js"

const img1 = document.querySelector("#teddy-image")
const img2 = document.querySelector("#emoji-image")

if (!img1.complete) {
	img1.addEventListener("load", () => {
		console.log("img1 loaded")
		requestAnimationFrame(drawFrame)
	})
}
if (!img2.complete) {
	img2.addEventListener("load", () => {
		console.log("img2 loaded")
		requestAnimationFrame(drawFrame)
	})
}

export class RotatingImage {
	renderColors(config) {
		if (!img1.complete) {
			return
		}
		if (!img2.complete) {
			return
		}
		return mapTeddieImageLedColors(config)
	}
}
