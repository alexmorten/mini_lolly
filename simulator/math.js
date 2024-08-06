export function radians(degrees) {
	return (degrees * Math.PI) / 180
}

export const goldenAngle = 137.507764

export function gammaDecode(value) {
	value /= 255
	return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4)
}

export function gammaEncode(value) {
	return (value <= 0.0031308 ? 12.92 * value : 1.055 * Math.pow(value, 1 / 2.4) - 0.055) * 255
}
