/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * presets.c - Preset management and resolution
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Hash function for preset names
 *
 * Uses djb2 algorithm for good distribution.
 */
static uint32_t hash_preset_name(const char *name) {
    uint32_t hash = 5381;
    for (const char *p = name; *p; p++) {
        hash = ((hash << 5) + hash) + *p;  // hash * 33 + c
    }
    return hash % 64;  // 64 buckets
}

/**
 * Initialize preset registry with hardcoded standard presets
 */
void preset_registry_init(struct preset_registry *registry) {
    if (!registry) {
        return;
    }

    // Initialize buckets
    memset(registry->buckets, 0, sizeof(registry->buckets));
    registry->preset_count = 0;

    // Standard preset: window
    struct wlblur_blur_params window_params = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 3,
        .radius = 8.0,
        .brightness = 1.0,
        .contrast = 1.0,
        .saturation = 1.15,
        .noise = 0.02,
        .vibrancy = 0.0,
        .vibrancy_darkness = 0.0,
        .tint_r = 0.0,
        .tint_g = 0.0,
        .tint_b = 0.0,
        .tint_a = 0.0,
    };
    preset_registry_add(registry, "window", &window_params);

    // Standard preset: panel
    struct wlblur_blur_params panel_params = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 2,
        .radius = 4.0,
        .brightness = 1.05,
        .contrast = 1.0,
        .saturation = 1.1,
        .noise = 0.01,
        .vibrancy = 0.0,
        .vibrancy_darkness = 0.0,
        .tint_r = 0.0,
        .tint_g = 0.0,
        .tint_b = 0.0,
        .tint_a = 0.0,
    };
    preset_registry_add(registry, "panel", &panel_params);

    // Standard preset: hud
    struct wlblur_blur_params hud_params = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 4,
        .radius = 12.0,
        .brightness = 1.0,
        .contrast = 1.0,
        .saturation = 1.2,
        .noise = 0.02,
        .vibrancy = 0.2,
        .vibrancy_darkness = 0.0,
        .tint_r = 0.0,
        .tint_g = 0.0,
        .tint_b = 0.0,
        .tint_a = 0.0,
    };
    preset_registry_add(registry, "hud", &hud_params);

    // Standard preset: tooltip
    struct wlblur_blur_params tooltip_params = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 1,
        .radius = 2.0,
        .brightness = 1.0,
        .contrast = 1.0,
        .saturation = 1.0,
        .noise = 0.0,
        .vibrancy = 0.0,
        .vibrancy_darkness = 0.0,
        .tint_r = 0.0,
        .tint_g = 0.0,
        .tint_b = 0.0,
        .tint_a = 0.0,
    };
    preset_registry_add(registry, "tooltip", &tooltip_params);
}

/**
 * Add preset to registry
 */
bool preset_registry_add(struct preset_registry *registry,
                         const char *name,
                         const struct wlblur_blur_params *params) {
    if (!registry || !name || !params) {
        return false;
    }

    // Check for duplicate
    struct preset *existing = preset_registry_lookup(registry, name);
    if (existing) {
        // Update existing preset
        existing->params = *params;
        return true;
    }

    // Allocate new preset
    struct preset *preset = calloc(1, sizeof(*preset));
    if (!preset) {
        return false;
    }

    // Fill preset
    strncpy(preset->name, name, sizeof(preset->name) - 1);
    preset->params = *params;

    // Add to hash table
    uint32_t bucket = hash_preset_name(name);
    preset->next = registry->buckets[bucket];
    registry->buckets[bucket] = preset;

    registry->preset_count++;
    return true;
}

/**
 * Lookup preset by name
 */
struct preset* preset_registry_lookup(const struct preset_registry *registry,
                                      const char *name) {
    if (!registry || !name) {
        return NULL;
    }

    uint32_t bucket = hash_preset_name(name);
    for (struct preset *p = registry->buckets[bucket]; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }

    return NULL;
}

/**
 * Free all presets in registry
 */
void preset_registry_free(struct preset_registry *registry) {
    if (!registry) {
        return;
    }

    for (size_t i = 0; i < 64; i++) {
        struct preset *p = registry->buckets[i];
        while (p) {
            struct preset *next = p->next;
            free(p);
            p = next;
        }
        registry->buckets[i] = NULL;
    }

    registry->preset_count = 0;
}

/**
 * Resolve preset with fallback hierarchy
 *
 * Resolution order:
 * 1. Named preset (if provided and found)
 * 2. Direct parameters (if provided)
 * 3. Daemon defaults (from config)
 * 4. Hardcoded defaults
 */
const struct wlblur_blur_params* resolve_preset(
    const struct daemon_config *config,
    const char *preset_name,
    const struct wlblur_blur_params *override_params
) {
    // 1. Try named preset
    if (preset_name && preset_name[0] != '\0') {
        if (config) {
            struct preset *preset = preset_registry_lookup(&config->presets, preset_name);
            if (preset) {
                return &preset->params;
            }
        }
        // Preset not found, log warning and fall through
        fprintf(stderr, "[presets] Warning: Preset '%s' not found, using fallback\n",
                preset_name);
    }

    // 2. Try direct parameter override
    if (override_params) {
        return override_params;
    }

    // 3. Try daemon defaults
    if (config && config->has_defaults) {
        return &config->defaults;
    }

    // 4. Hardcoded defaults
    static const struct wlblur_blur_params hardcoded = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 3,
        .radius = 5.0,
        .brightness = 1.0,
        .contrast = 1.0,
        .saturation = 1.1,
        .noise = 0.02,
        .vibrancy = 0.0,
        .vibrancy_darkness = 0.0,
        .tint_r = 0.0,
        .tint_g = 0.0,
        .tint_b = 0.0,
        .tint_a = 0.0,
    };
    return &hardcoded;
}
