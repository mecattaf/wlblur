/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur_params.h - Unified blur parameter schema
 *
 * This schema consolidates parameters from:
 * - SceneFX (MIT License)
 * - Hyprland (BSD-3-Clause License)
 * - Wayfire (MIT License)
 */

#ifndef WLBLUR_BLUR_PARAMS_H
#define WLBLUR_BLUR_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Core blur parameters
 *
 * User-configurable settings that control blur quality and appearance.
 */
struct wlblur_blur_params {
    /* === Core Algorithm Parameters === */

    /**
     * Number of blur passes (downsampling + upsampling)
     *
     * Range: 1-8
     * Default: 3
     *
     * Each pass doubles the effective blur radius:
     *   blur_size = 2^(num_passes+1) × radius
     *
     * Examples:
     *   1 pass: Light blur (fast, subtle)
     *   3 passes: Balanced (default, Apple-level quality)
     *   5+ passes: Heavy blur (artistic, slower)
     *
     * Performance impact: Linear with pass count
     *   1 pass: ~0.5ms @ 1080p
     *   3 passes: ~1.2ms @ 1080p
     *   5 passes: ~2.0ms @ 1080p
     *
     * Source: SceneFX default=3, Hyprland default=1, Wayfire default=3
     */
    int num_passes;

    /**
     * Base blur radius in pixels
     *
     * Range: 1.0-20.0
     * Default: 5.0
     *
     * This is NOT the final blur size. Actual blur coverage is:
     *   blur_size = 2^(num_passes+1) × radius
     *
     * Examples (with 3 passes):
     *   radius=5  → blur_size=80px  (default)
     *   radius=10 → blur_size=160px (strong)
     *   radius=20 → blur_size=320px (extreme)
     *
     * Higher values = more GPU memory and bandwidth
     *
     * Source: Universal default=5.0 across all three implementations
     */
    float radius;

    /* === Post-Processing Effects === */

    /**
     * Brightness adjustment
     *
     * Range: 0.0-2.0
     * Default: 0.9 (slightly darkened)
     *
     * Applied after blur as: color.rgb *= brightness
     *
     * Values:
     *   < 1.0: Darken (recommended for readability over blur)
     *   = 1.0: No change
     *   > 1.0: Brighten
     *
     * Rationale: Blurred backgrounds often benefit from slight darkening
     * to improve contrast with foreground content.
     *
     * Source: SceneFX=0.9, Hyprland=0.9
     */
    float brightness;

    /**
     * Contrast adjustment
     *
     * Range: 0.0-2.0
     * Default: 0.9
     *
     * Applied as: color.rgb = (color.rgb - 0.5) * contrast + 0.5
     *
     * Values:
     *   < 1.0: Reduce contrast (softer look)
     *   = 1.0: No change
     *   > 1.0: Increase contrast (sharper look)
     *
     * Source: SceneFX=0.9, Hyprland=0.9
     */
    float contrast;

    /**
     * Saturation adjustment
     *
     * Range: 0.0-2.0
     * Default: 1.1 (slightly boosted)
     *
     * Applied as: mix(grayscale, color, saturation)
     *
     * Values:
     *   = 0.0: Full grayscale
     *   < 1.0: Desaturated
     *   = 1.0: Original saturation
     *   > 1.0: Boosted saturation (vibrant)
     *
     * Rationale: Slight saturation boost makes blur more visually appealing.
     *
     * Source: SceneFX=1.1, Hyprland=1.0
     */
    float saturation;

    /**
     * Noise amount (film grain)
     *
     * Range: 0.0-0.1
     * Default: 0.02
     *
     * Adds random per-pixel noise to reduce color banding.
     *
     * Values:
     *   = 0.0: No noise (may show banding)
     *   = 0.02: Subtle grain (recommended)
     *   > 0.05: Visible grain (artistic)
     *
     * Rationale: Blur can create smooth gradients that show banding
     * on 8-bit displays. Noise dithers this effect.
     *
     * Source: SceneFX=0.02, Hyprland=0.02
     */
    float noise;

    /* === Advanced Features (Phase 7+) === */

    /**
     * Vibrancy strength (Hyprland-style HSL boost)
     *
     * Range: 0.0-2.0
     * Default: 0.0 (disabled)
     *
     * Boosts color saturation in HSL color space.
     * Applied as: hsl.saturation *= (1.0 + vibrancy)
     *
     * Values:
     *   = 0.0: Disabled (passthrough)
     *   = 1.15: Hyprland default (Apple-like vibrant colors)
     *   > 1.5: Extreme (artistic, may clip)
     *
     * Note: This is different from 'saturation' parameter.
     * Vibrancy works in HSL space, saturation works in RGB space.
     *
     * Phase: 7 (post-MVP)
     * Source: Hyprland-specific feature
     */
    float vibrancy;

    /**
     * Vibrancy darkness (lightness reduction when vibrancy active)
     *
     * Range: 0.0-1.0
     * Default: 0.0
     *
     * Reduces lightness to prevent colors from becoming too bright
     * when vibrancy is high.
     *
     * Applied as: hsl.lightness *= (1.0 - vibrancy_darkness)
     *
     * Phase: 7 (post-MVP)
     * Source: Hyprland
     */
    float vibrancy_darkness;

    /**
     * Tint color overlay (RGBA)
     *
     * Range: 0.0-1.0 per channel
     * Default: (0, 0, 0, 0) - no tint
     *
     * Applied as additive blend after all other effects.
     * Alpha channel controls tint strength.
     *
     * Example: Warm tint: (1.0, 0.8, 0.6, 0.1)
     *
     * Phase: Future
     * Source: Common feature in blur systems
     */
    float tint_r;
    float tint_g;
    float tint_b;
    float tint_a;
};

/**
 * Computed blur parameters (read-only)
 *
 * These are calculated from wlblur_blur_params, not set by user.
 */
struct wlblur_blur_computed {
    /**
     * Effective blur size in pixels
     *
     * Formula: 2^(num_passes + 1) × radius
     *
     * Examples:
     *   passes=3, radius=5  → blur_size=80
     *   passes=4, radius=5  → blur_size=160
     *   passes=5, radius=10 → blur_size=640
     *
     * This determines how far the blur effect extends from the source.
     */
    int blur_size;

    /**
     * Damage region expansion needed (pixels)
     *
     * Compositors must expand damage regions by this amount to prevent
     * artifacts at edges of blurred regions.
     *
     * Typically: damage_expand = blur_size
     *
     * Rationale: Blur kernel needs access to pixels outside the damaged
     * region to avoid edge artifacts.
     */
    int damage_expand;
};

/**
 * Preset configurations matching different compositors
 */
enum wlblur_preset {
    WLBLUR_PRESET_CUSTOM = 0,           // User-defined values
    WLBLUR_PRESET_SCENEFX_DEFAULT,      // SceneFX defaults
    WLBLUR_PRESET_HYPRLAND_DEFAULT,     // Hyprland defaults
    WLBLUR_PRESET_WAYFIRE_DEFAULT,      // Wayfire defaults
};

/* === Public API Functions === */

/**
 * Initialize parameters with default values
 *
 * Uses SceneFX-style defaults (balanced quality/performance):
 *   num_passes = 3
 *   radius = 5.0
 *   brightness = 0.9
 *   contrast = 0.9
 *   saturation = 1.1
 *   noise = 0.02
 *   vibrancy = 0.0 (disabled)
 *
 * Returns: Initialized parameter struct
 */
struct wlblur_blur_params wlblur_params_default(void);

/**
 * Load preset parameters
 *
 * @param preset Preset to load
 * @return Parameter struct with preset values
 */
struct wlblur_blur_params wlblur_params_from_preset(enum wlblur_preset preset);

/**
 * Validate parameter ranges
 *
 * Checks that all parameters are within valid ranges:
 *   num_passes: 1-8
 *   radius: 1.0-20.0
 *   brightness, contrast, saturation: 0.0-2.0
 *   noise: 0.0-0.1
 *   vibrancy: 0.0-2.0
 *   vibrancy_darkness: 0.0-1.0
 *   tint RGBA: 0.0-1.0
 *
 * @param params Parameters to validate
 * @return true if all parameters valid, false otherwise
 */
bool wlblur_params_validate(const struct wlblur_blur_params *params);

/**
 * Compute derived parameters
 *
 * Calculates blur_size and damage_expand from core parameters.
 *
 * Formula: blur_size = 2^(num_passes+1) × radius
 *
 * @param params Input parameters
 * @return Computed values struct
 */
struct wlblur_blur_computed wlblur_params_compute(
    const struct wlblur_blur_params *params
);

#endif /* WLBLUR_BLUR_PARAMS_H */
