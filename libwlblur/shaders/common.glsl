/*
 * Common Shader Utilities
 *
 * Shared utility functions for wlblur shaders.
 * This file contains helper functions used across multiple shaders.
 *
 * Copyright (c) 2025 wlblur contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WLBLUR_COMMON_GLSL
#define WLBLUR_COMMON_GLSL

/*
 * Pseudo-Random Number Generator
 *
 * Generates a deterministic pseudo-random number based on 2D coordinates.
 * Useful for adding noise/grain to prevent banding in smooth gradients.
 *
 * Input: 2D seed (typically texture coordinates)
 * Output: Random-looking float in range [0, 1]
 *
 * Algorithm: Hash function using dot product with magic constants
 * Source: Common GLSL pattern, origin unknown
 */
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

/*
 * Luminance Calculation (ITU-R BT.709)
 *
 * Calculates perceptual luminance of an RGB color.
 * Uses HDTV color standard weights.
 *
 * Input: RGB color [0,1]
 * Output: Luminance [0,1]
 *
 * Weights reflect human eye sensitivity:
 * - Green: 71.52% (most sensitive)
 * - Red: 21.26%
 * - Blue: 7.22% (least sensitive)
 */
float luminance(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

/*
 * Grayscale Conversion
 *
 * Converts RGB color to grayscale while preserving perceived brightness.
 * Uses luminance calculation to maintain perceptual accuracy.
 *
 * Input: RGB color [0,1]
 * Output: Grayscale RGB (all channels equal) [0,1]
 */
vec3 toGrayscale(vec3 rgb) {
    float gray = luminance(rgb);
    return vec3(gray);
}

/*
 * Linear Interpolation (Lerp)
 *
 * Linearly interpolates between two values.
 * While GLSL provides mix() built-in, this is here for clarity/documentation.
 *
 * Input: a (start), b (end), t (interpolation factor [0,1])
 * Output: a + (b - a) * t
 *
 * t=0.0: returns a
 * t=0.5: returns midpoint
 * t=1.0: returns b
 */
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/*
 * Smoothstep (Hermite Interpolation)
 *
 * Smooth interpolation with ease-in and ease-out.
 * GLSL provides this as built-in, but documented here for completeness.
 *
 * Input: edge0, edge1 (range), x (value to smooth)
 * Output: Smoothly interpolated value [0,1]
 *
 * x <= edge0: returns 0
 * x >= edge1: returns 1
 * edge0 < x < edge1: returns smooth S-curve
 */
// Note: Use GLSL built-in smoothstep() instead of reimplementing

/*
 * Clamp Value to Range
 *
 * Constrains a value to stay within [min, max].
 * GLSL provides this as built-in, but documented here for completeness.
 *
 * Input: x (value), minVal, maxVal
 * Output: clamped value
 */
// Note: Use GLSL built-in clamp() instead of reimplementing

/*
 * Remap Value from One Range to Another
 *
 * Maps a value from input range [inMin, inMax] to output range [outMin, outMax].
 * Useful for scaling uniform values or texture coordinates.
 *
 * Input: value, inMin, inMax, outMin, outMax
 * Output: remapped value
 *
 * Example: remap(0.5, 0.0, 1.0, -1.0, 1.0) = 0.0
 */
float remap(float value, float inMin, float inMax, float outMin, float outMax) {
    float t = (value - inMin) / (inMax - inMin);
    return outMin + t * (outMax - outMin);
}

/*
 * Vector Remap (component-wise)
 *
 * Applies remap() to each component of a vec3.
 */
vec3 remap3(vec3 value, float inMin, float inMax, float outMin, float outMax) {
    return vec3(
        remap(value.x, inMin, inMax, outMin, outMax),
        remap(value.y, inMin, inMax, outMin, outMax),
        remap(value.z, inMin, inMax, outMin, outMax)
    );
}

#endif // WLBLUR_COMMON_GLSL
