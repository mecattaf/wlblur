/*
 * Vibrancy Shader (HSL Color Boost)
 *
 * Extracted from Hyprland (BSD-3-Clause License)
 * Copyright (c) 2022-2025, vaxerski
 * https://github.com/hyprwm/Hyprland
 *
 * Original file: src/render/shaders/glsl/blur1.frag
 * Source commit: b77cbad50251f0506b61d834b025247dcf74dddf (2025-11-12)
 *
 * Modifications for wlblur:
 * - Extracted as standalone shader (separated from blur pass)
 * - Simplified vibrancy calculation for general-purpose use
 * - Added comprehensive documentation
 * - GLSL 3.0 ES compatibility ensured
 * - Made vibrancy explicitly opt-in (vibrancy=0.0 is passthrough)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#version 300 es

precision highp float;

// Input texture to apply vibrancy to
// Can be a blurred texture or any color image
uniform sampler2D tex;

// Vibrancy strength: saturation boost multiplier
// 0.0: passthrough (no effect)
// 0.0-1.0: subtle boost
// 1.0-2.0: strong boost
// Range: 0.0 - 2.0
// Default: 0.0 (disabled)
uniform float vibrancy;

// Vibrancy darkness: controls how much to darken boosted colors
// Prevents oversaturation from causing washed-out appearance
// 0.0: no darkening
// 1.0: maximum darkening
// Range: 0.0 - 1.0
// Default: 0.0 (no darkening)
uniform float vibrancy_darkness;

// Number of blur passes (used for vibrancy scaling)
// When integrated with blur, this normalizes vibrancy per pass
// For standalone use, set to 1
// Range: 1 - 8
// Default: 1
uniform int passes;

// Texture coordinates from vertex shader
in vec2 v_texcoord;

// Output color
out vec4 fragColor;

/*
 * Perceptual Brightness Constants
 *
 * Based on HSP color model: http://alienryderflex.com/hsp.html
 * These weights reflect how humans perceive brightness of RGB channels.
 * Red and blue appear darker than green at the same numeric value.
 */
const float Pr = 0.299;  // Red weight
const float Pg = 0.587;  // Green weight (dominant)
const float Pb = 0.114;  // Blue weight

/*
 * Vibrancy Algorithm Parameters
 *
 * These control the "boost curve" that determines which colors get
 * more saturation boost based on their existing saturation and brightness.
 *
 * Visualization: https://www.desmos.com/3d/a88652b9a4
 *
 * - a: Balance between saturation and brightness importance (0.93)
 *      Higher = prioritize brightness, Lower = prioritize saturation
 * - b: Base threshold for boost activation (0.11)
 *      Colors must exceed this saturation/brightness to be boosted
 * - c: Smoothness of boost transition (0.66)
 *      Higher = smoother gradient, Lower = sharper cutoff
 */
const float a = 0.93;
const float b = 0.11;
const float c = 0.66;

/*
 * Double Circle Sigmoid Function
 *
 * A smooth S-curve easing function composed of two circular arcs.
 * Used to perceptually scale brightness for the vibrancy algorithm.
 *
 * Source: http://www.flong.com/archive/texts/code/shapers_circ/
 *
 * Input: x [0,1], a [0,1] (inflection point)
 * Output: smoothly eased value [0,1]
 */
float doubleCircleSigmoid(float x, float a) {
    a = clamp(a, 0.0, 1.0);

    float y = 0.0;
    if (x <= a) {
        // First circle arc (ease in)
        y = a - sqrt(a * a - x * x);
    } else {
        // Second circle arc (ease out)
        y = a + sqrt(pow(1.0 - a, 2.0) - pow(x - 1.0, 2.0));
    }
    return y;
}

/*
 * RGB to HSL Conversion
 *
 * Converts RGB color space [0,1] to HSL color space:
 * - H (Hue): [0,1] representing 0-360 degrees
 * - S (Saturation): [0,1]
 * - L (Lightness): [0,1]
 *
 * Algorithm: Standard HSL conversion with optimized GLSL implementation.
 */
vec3 rgb2hsl(vec3 col) {
    float red   = col.r;
    float green = col.g;
    float blue  = col.b;

    float minc  = min(col.r, min(col.g, col.b));
    float maxc  = max(col.r, max(col.g, col.b));
    float delta = maxc - minc;

    // Lightness: average of min and max RGB components
    float lum = (minc + maxc) * 0.5;
    float sat = 0.0;
    float hue = 0.0;

    // Calculate saturation
    // Saturation is 0 at pure black/white, maximum at 50% lightness
    if (lum > 0.0 && lum < 1.0) {
        float mul = (lum < 0.5) ? (lum) : (1.0 - lum);
        sat       = delta / (mul * 2.0);
    }

    // Calculate hue (which RGB component is dominant)
    if (delta > 0.0) {
        vec3  maxcVec = vec3(maxc);
        // Determine which channel (R, G, or B) is the maximum
        vec3  masks = vec3(equal(maxcVec, col)) * vec3(notEqual(maxcVec, vec3(green, blue, red)));
        // Calculate hue based on dominant channel
        vec3  adds = vec3(0.0, 2.0, 4.0) + vec3(green - blue, blue - red, red - green) / delta;

        hue += dot(adds, masks);
        hue /= 6.0;

        // Wrap negative hue to [0,1] range
        if (hue < 0.0)
            hue += 1.0;
    }

    return vec3(hue, sat, lum);
}

/*
 * HSL to RGB Conversion
 *
 * Converts HSL color space back to RGB [0,1].
 * Inverse operation of rgb2hsl().
 *
 * Algorithm: Standard HSL to RGB conversion optimized for GLSL.
 */
vec3 hsl2rgb(vec3 col) {
    const float onethird = 1.0 / 3.0;
    const float twothird = 2.0 / 3.0;
    const float rcpsixth = 6.0;

    float       hue = col.x;
    float       sat = col.y;
    float       lum = col.z;

    vec3        xt = vec3(0.0);

    // Determine RGB contributions based on hue sector (0-120, 120-240, 240-360 degrees)
    if (hue < onethird) {
        // Red to yellow sector
        xt.r = rcpsixth * (onethird - hue);
        xt.g = rcpsixth * hue;
        xt.b = 0.0;
    } else if (hue < twothird) {
        // Yellow to cyan sector
        xt.r = 0.0;
        xt.g = rcpsixth * (twothird - hue);
        xt.b = rcpsixth * (hue - onethird);
    } else {
        // Cyan to red sector
        xt = vec3(rcpsixth * (hue - twothird), 0.0, rcpsixth * (1.0 - hue));
    }

    xt = min(xt, 1.0);

    // Apply saturation
    float sat2   = 2.0 * sat;
    float satinv = 1.0 - sat;
    float luminv = 1.0 - lum;
    float lum2m1 = (2.0 * lum) - 1.0;
    vec3  ct     = (sat2 * xt) + satinv;

    // Apply lightness
    vec3  rgb;
    if (lum >= 0.5)
        rgb = (luminv * ct) + lum2m1;
    else
        rgb = lum * ct;

    return rgb;
}

/*
 * Main vibrancy shader
 *
 * Applies selective saturation boost based on existing color properties.
 * Dark, desaturated colors receive less boost to prevent unnatural results.
 * Bright, already-saturated colors receive more boost for vivid appearance.
 *
 * Algorithm:
 * 1. Convert RGB to HSL
 * 2. Calculate perceptual brightness (darker colors need less boost)
 * 3. Calculate boost amount based on saturation, brightness, and vibrancy settings
 * 4. Increase saturation by boost amount
 * 5. Convert back to RGB
 *
 * Note: When vibrancy=0.0, this is a no-op passthrough for performance.
 */
void main() {
    vec4 color = texture(tex, v_texcoord);

    // Fast path: vibrancy disabled, no processing needed
    if (vibrancy == 0.0) {
        fragColor = color;
        return;
    }

    // Invert vibrancy_darkness so that it correctly maps to the config setting
    // (Higher darkness value = less darkening applied)
    float vibrancy_darkness1 = 1.0 - vibrancy_darkness;

    // Convert to HSL for saturation manipulation
    vec3 hsl = rgb2hsl(color.rgb);

    // Calculate perceived brightness using HSP color model
    // Apply sigmoid curve to prevent dark colors from being over-boosted
    float perceivedBrightness = doubleCircleSigmoid(
        sqrt(color.r * color.r * Pr + color.g * color.g * Pg + color.b * color.b * Pb),
        0.8 * vibrancy_darkness1
    );

    // Calculate boost factor using elliptical distance in saturation-brightness space
    // This creates a smooth transition zone where colors gradually receive more boost
    // as they move away from the origin (desaturated/dark) in S-L space
    float b1 = b * vibrancy_darkness1;
    float boostBase = hsl[1] > 0.0
        ? smoothstep(
            b1 - c * 0.5,
            b1 + c * 0.5,
            1.0 - (pow(1.0 - hsl[1] * cos(a), 2.0) + pow(1.0 - perceivedBrightness * sin(a), 2.0))
        )
        : 0.0;

    // Apply vibrancy boost to saturation, normalized by number of passes
    float saturation = clamp(hsl[1] + (boostBase * vibrancy) / float(passes), 0.0, 1.0);

    // Convert back to RGB with boosted saturation
    vec3 newColor = hsl2rgb(vec3(hsl[0], saturation, hsl[2]));

    fragColor = vec4(newColor, color[3]);
}
