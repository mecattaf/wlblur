/*
 * Kawase Downsample Shader
 *
 * Extracted from SceneFX (MIT License)
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 * https://github.com/wlrfx/scenefx
 *
 * Original file: render/fx_renderer/gles3/shaders/blur1.frag
 * Source commit: 7f9e7409f6169fa637f1265895c121a8f8b70272 (2025-11-06)
 *
 * Modifications for wlblur:
 * - Renamed to kawase_downsample.frag.glsl
 * - Added comprehensive uniform documentation
 * - Changed texture2D() to texture() for GLSL 3.0 ES compliance
 * - Added detailed sampling pattern documentation
 *
 * SPDX-License-Identifier: MIT
 */

#version 300 es

precision mediump float;

// Input texture to blur
// This is the source texture that will be downsampled and blurred
uniform sampler2D tex;

// Sampling radius: controls blur strength
// Typically pass index (0, 1, 2, ...) for multi-pass blur
// Higher values = larger blur radius
// Range: 0.0 - 8.0 (practical maximum)
uniform float radius;

// Half-pixel offset: vec2(0.5/width, 0.5/height)
// Used to sample between pixels for smoother results
// This compensates for texture coordinate rounding
uniform vec2 halfpixel;

// Texture coordinates from vertex shader
// Range: [0.0, 1.0] for both x and y
in mediump vec2 v_texcoord;

// Output color
out vec4 fragColor;

/*
 * Kawase Downsample: 5-tap sampling pattern
 *
 * This shader implements the downsample pass of the Dual Kawase blur algorithm.
 * It samples 5 texels: 1 center + 4 diagonal corners, weighted to produce a
 * smooth blur while simultaneously downsampling the image by 2x.
 *
 * Sampling pattern (X = center, A/B/C/D = corners):
 *
 *     A . . B
 *     . . . .
 *     . . X .
 *     . . . .
 *     C . . D
 *
 * Weights:
 * - Center (X): 4.0
 * - Each corner (A,B,C,D): 1.0
 * - Total weight: 8.0
 *
 * The 2x downsampling is achieved by the "uv * 2.0" operation, which
 * effectively reads every other pixel from the source texture.
 */
void main() {
    // Downsample: multiply UV by 2.0 to read every other pixel
    vec2 uv = v_texcoord * 2.0;

    // Center sample (weight 4.0)
    vec4 sum = texture(tex, uv) * 4.0;

    // Four diagonal corner samples (weight 1.0 each)
    // These are offset by (radius * halfpixel) in diagonal directions
    sum += texture(tex, uv - halfpixel.xy * radius);           // Top-left
    sum += texture(tex, uv + halfpixel.xy * radius);           // Bottom-right
    sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius);  // Top-right
    sum += texture(tex, uv - vec2(halfpixel.x, -halfpixel.y) * radius);  // Bottom-left

    // Average with total weight of 8.0
    fragColor = sum / 8.0;
}
