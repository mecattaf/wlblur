/*
 * Kawase Upsample Shader
 *
 * Extracted from SceneFX (MIT License)
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 * https://github.com/wlrfx/scenefx
 *
 * Original file: render/fx_renderer/gles3/shaders/blur2.frag
 * Source commit: 7f9e7409f6169fa637f1265895c121a8f8b70272 (2025-11-06)
 *
 * Modifications for wlblur:
 * - Renamed to kawase_upsample.frag.glsl
 * - Added comprehensive uniform documentation
 * - Changed texture2D() to texture() for GLSL 3.0 ES compliance
 * - Added detailed sampling pattern documentation
 *
 * SPDX-License-Identifier: MIT
 */

#version 300 es

precision mediump float;

// Downsampled texture to upsample and blur
// This is the output from the downsample pass
uniform sampler2D tex;

// Sampling radius: controls blur strength
// Typically pass index (0, 1, 2, ...) for multi-pass blur
// Should match the corresponding downsample pass radius
// Range: 0.0 - 8.0 (practical maximum)
uniform float radius;

// Half-pixel offset: vec2(0.5/width, 0.5/height)
// Used to sample between pixels for smoother results
uniform vec2 halfpixel;

// Texture coordinates from vertex shader
// Range: [0.0, 1.0] for both x and y
in mediump vec2 v_texcoord;

// Output color
out vec4 fragColor;

/*
 * Kawase Upsample: 8-tap sampling pattern
 *
 * This shader implements the upsample pass of the Dual Kawase blur algorithm.
 * It samples 8 texels in a cross-plus-diagonal pattern with weighted blending.
 * This upsamples the image by 2x while maintaining smooth blur quality.
 *
 * Sampling pattern (numbers indicate weight):
 *
 *     . 1 2 1 .
 *     1 . . . 1
 *     2 . X . 2   X = sample point (v_texcoord / 2.0)
 *     1 . . . 1
 *     . 1 2 1 .
 *
 * The 2x upsampling is achieved by the "uv / 2.0" operation.
 *
 * Sample positions and weights:
 * - 4 cardinal directions (left, right, up, down): weight 1.0 each
 * - 4 diagonal directions (corners): weight 2.0 each
 * - Total weight: 12.0
 *
 * The heavier weighting of diagonal samples creates a smooth, rotationally
 * symmetric blur kernel.
 */
void main() {
    // Upsample: divide UV by 2.0 to read from downsampled texture
    vec2 uv = v_texcoord / 2.0;

    // Left cardinal (weight 1.0)
    vec4 sum = texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    // Top-left diagonal (weight 2.0)
    sum += texture(tex, uv + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;

    // Top cardinal (weight 1.0)
    sum += texture(tex, uv + vec2(0.0, halfpixel.y * 2.0) * radius);

    // Top-right diagonal (weight 2.0)
    sum += texture(tex, uv + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;

    // Right cardinal (weight 1.0)
    sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);

    // Bottom-right diagonal (weight 2.0)
    sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;

    // Bottom cardinal (weight 1.0)
    sum += texture(tex, uv + vec2(0.0, -halfpixel.y * 2.0) * radius);

    // Bottom-left diagonal (weight 2.0)
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    // Average with total weight of 12.0
    fragColor = sum / 12.0;
}
