/*
 * Blur Finish Shader (Post-Processing Effects)
 *
 * Extracted from SceneFX (MIT License)
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 * https://github.com/wlrfx/scenefx
 *
 * Original file: render/fx_renderer/gles3/shaders/blur_effects.frag
 * Source commit: 7f9e7409f6169fa637f1265895c121a8f8b70272 (2025-11-06)
 *
 * Modifications for wlblur:
 * - Renamed to blur_finish.frag.glsl
 * - Added comprehensive uniform documentation
 * - Changed texture2D() to texture() for GLSL 3.0 ES compliance
 * - Preserved mediump/highp precision as-is from original
 * - Added detailed algorithm documentation
 *
 * SPDX-License-Identifier: MIT
 */

#version 300 es

precision highp float;

// Blurred texture from previous passes
uniform sampler2D tex;

// Brightness adjustment: multiplier for RGB values
// < 1.0: darker, > 1.0: brighter
// Range: 0.0 - 2.0
// Default: 0.9 (slightly darker to prevent washout)
uniform float brightness;

// Contrast adjustment: expands or contracts value range
// < 1.0: reduced contrast, > 1.0: increased contrast
// Adjusted around 0.5 gray point
// Range: 0.0 - 2.0
// Default: 0.9 (slightly reduced)
uniform float contrast;

// Saturation adjustment: color intensity
// 0.0: grayscale, 1.0: original, > 1.0: oversaturated
// Uses perceptual luminance weights (ITU-R BT.601)
// Range: 0.0 - 2.0
// Default: 1.1 (slightly boosted)
uniform float saturation;

// Noise amount: pseudo-random grain per pixel
// Adds visual texture to prevent banding in smooth gradients
// Range: 0.0 - 0.1
// Default: 0.02 (subtle grain)
uniform float noise;

// Texture coordinates from vertex shader
in vec2 v_texcoord;

// Output color
out vec4 fragColor;

/*
 * Brightness Matrix
 *
 * Creates a 4x4 color matrix that adds a constant offset to RGB channels.
 * The offset is calculated as (brightness - 1.0), so:
 * - brightness = 1.0 → no change
 * - brightness < 1.0 → darkens (negative offset)
 * - brightness > 1.0 → brightens (positive offset)
 *
 * Matrix form:
 * [ 1 0 0 0 ]
 * [ 0 1 0 0 ]
 * [ 0 0 1 0 ]
 * [ b b b 1 ]  where b = (brightness - 1.0)
 */
mat4 brightnessMatrix() {
	float b = brightness - 1.0;
	return mat4(1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				b, b, b, 1);
}

/*
 * Contrast Matrix
 *
 * Creates a 4x4 color matrix that scales RGB values around the 0.5 gray point.
 * The translation offset is (1.0 - contrast) / 2.0, which ensures that
 * 0.5 gray remains unchanged while other values are scaled.
 *
 * Matrix form:
 * [ c 0 0 0 ]
 * [ 0 c 0 0 ]
 * [ 0 0 c 0 ]
 * [ t t t 1 ]  where c = contrast, t = (1.0 - contrast) / 2.0
 *
 * Effect: (value - 0.5) * contrast + 0.5
 */
mat4 contrastMatrix() {
	float t = (1.0 - contrast) / 2.0;
	return mat4(contrast, 0, 0, 0,
				0, contrast, 0, 0,
				0, 0, contrast, 0,
				t, t, t, 1);
}

/*
 * Saturation Matrix
 *
 * Creates a 4x4 color matrix that adjusts color saturation.
 * Uses perceptual luminance weights (ITU-R BT.601):
 * - Red: 0.3086
 * - Green: 0.6094
 * - Blue: 0.0820
 *
 * When saturation = 0.0: all colors become grayscale (desaturate)
 * When saturation = 1.0: original colors preserved
 * When saturation > 1.0: colors become more vivid (oversaturate)
 *
 * The matrix construction ensures that the luminance of each pixel
 * is preserved during saturation adjustment.
 */
mat4 saturationMatrix() {
	vec3 luminance = vec3(0.3086, 0.6094, 0.0820) * (1.0 - saturation);
	vec3 red = vec3(luminance.x);
	red.x += saturation;
	vec3 green = vec3(luminance.y);
	green.y += saturation;
	vec3 blue = vec3(luminance.z);
	blue.z += saturation;
	return mat4(red, 0,
				green, 0,
				blue, 0,
				0, 0, 0, 1);
}

/*
 * Noise Amount Function
 *
 * Generates pseudo-random noise per pixel using a hash function.
 * The hash is based on texture coordinates, producing deterministic
 * but visually random noise patterns.
 *
 * Algorithm:
 * 1. Fracture texture coordinates to create high-frequency variation
 * 2. Dot product with magic numbers for mixing
 * 3. Fractional part of result creates [-0.5 * noise, 0.5 * noise] range
 *
 * This adds subtle grain that helps prevent color banding in smooth gradients.
 */
float noiseAmount(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
	p3 += dot(p3, p3.yzx + 33.33);
	float hash = fract((p3.x + p3.y) * p3.z);
	return (mod(hash, 1.0) - 0.5) * noise;
}

/*
 * Main shader: Apply post-processing effects
 *
 * Order of operations:
 * 1. Sample blurred texture
 * 2. Apply brightness/contrast/saturation via matrix multiplication
 *    Note: Matrices are NOT transposed (see original comment)
 * 3. Add noise to RGB channels
 *
 * The matrix multiplication is done in the order:
 * brightness * contrast * saturation * color
 * This applies saturation first, then contrast, then brightness.
 */
void main() {
	vec4 color = texture(tex, v_texcoord);
	// Do *not* transpose the combined matrix when multiplying
	color = brightnessMatrix() * contrastMatrix() * saturationMatrix() * color;
	color.xyz += noiseAmount(v_texcoord);
	fragColor = color;
}
