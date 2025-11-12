/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur_params.c - Parameter preset and validation implementation
 */

#include "wlblur/blur_params.h"
#include <math.h>

struct wlblur_blur_params wlblur_params_default(void) {
    // SceneFX-style defaults (validated in production)
    return (struct wlblur_blur_params){
        .num_passes = 3,
        .radius = 5.0f,
        .brightness = 0.9f,
        .contrast = 0.9f,
        .saturation = 1.1f,
        .noise = 0.02f,
        .vibrancy = 0.0f,
        .vibrancy_darkness = 0.0f,
        .tint_r = 0.0f,
        .tint_g = 0.0f,
        .tint_b = 0.0f,
        .tint_a = 0.0f,
    };
}

struct wlblur_blur_params wlblur_params_from_preset(enum wlblur_preset preset) {
    switch (preset) {
    case WLBLUR_PRESET_SCENEFX_DEFAULT:
        // SceneFX: Balanced quality/performance
        return (struct wlblur_blur_params){
            .num_passes = 3,
            .radius = 5.0f,
            .brightness = 0.9f,
            .contrast = 0.9f,
            .saturation = 1.1f,
            .noise = 0.02f,
            .vibrancy = 0.0f,
            .vibrancy_darkness = 0.0f,
            .tint_r = 0.0f, .tint_g = 0.0f, .tint_b = 0.0f, .tint_a = 0.0f,
        };

    case WLBLUR_PRESET_HYPRLAND_DEFAULT:
        // Hyprland: Subtle blur by default (1 pass)
        // Note: Hyprland users often increase passes via config
        return (struct wlblur_blur_params){
            .num_passes = 1,  // Hyprland's default
            .radius = 5.0f,
            .brightness = 0.9f,
            .contrast = 0.9f,
            .saturation = 1.0f,  // No saturation boost
            .noise = 0.02f,
            .vibrancy = 0.0f,  // Can be enabled separately
            .vibrancy_darkness = 0.0f,
            .tint_r = 0.0f, .tint_g = 0.0f, .tint_b = 0.0f, .tint_a = 0.0f,
        };

    case WLBLUR_PRESET_WAYFIRE_DEFAULT:
        // Wayfire: Minimal post-processing
        return (struct wlblur_blur_params){
            .num_passes = 3,
            .radius = 5.0f,
            .brightness = 1.0f,  // No brightness adjustment
            .contrast = 1.0f,    // No contrast adjustment
            .saturation = 1.0f,  // No saturation adjustment
            .noise = 0.0f,       // No noise
            .vibrancy = 0.0f,
            .vibrancy_darkness = 0.0f,
            .tint_r = 0.0f, .tint_g = 0.0f, .tint_b = 0.0f, .tint_a = 0.0f,
        };

    case WLBLUR_PRESET_CUSTOM:
    default:
        return wlblur_params_default();
    }
}

bool wlblur_params_validate(const struct wlblur_blur_params *params) {
    // Core algorithm
    if (params->num_passes < 1 || params->num_passes > 8) return false;
    if (params->radius < 1.0f || params->radius > 20.0f) return false;

    // Post-processing
    if (params->brightness < 0.0f || params->brightness > 2.0f) return false;
    if (params->contrast < 0.0f || params->contrast > 2.0f) return false;
    if (params->saturation < 0.0f || params->saturation > 2.0f) return false;
    if (params->noise < 0.0f || params->noise > 0.1f) return false;

    // Advanced
    if (params->vibrancy < 0.0f || params->vibrancy > 2.0f) return false;
    if (params->vibrancy_darkness < 0.0f || params->vibrancy_darkness > 1.0f) return false;

    // Tint (already constrained by float precision, but check for sanity)
    if (params->tint_r < 0.0f || params->tint_r > 1.0f) return false;
    if (params->tint_g < 0.0f || params->tint_g > 1.0f) return false;
    if (params->tint_b < 0.0f || params->tint_b > 1.0f) return false;
    if (params->tint_a < 0.0f || params->tint_a > 1.0f) return false;

    return true;
}

struct wlblur_blur_computed wlblur_params_compute(
    const struct wlblur_blur_params *params
) {
    // Formula from SceneFX: blur_size = 2^(passes+1) * radius
    int blur_size = (int)(powf(2.0f, params->num_passes + 1) * params->radius);

    return (struct wlblur_blur_computed){
        .blur_size = blur_size,
        .damage_expand = blur_size,  // Damage must expand by full blur radius
    };
}
