/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * config.h - Daemon configuration structures and API
 */

#ifndef WLBLURD_CONFIG_H
#define WLBLURD_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <wlblur/blur_params.h>

/*
 * Configuration System
 *
 * The wlblurd daemon supports TOML-based configuration with:
 * - Daemon settings (socket path, log level, limits)
 * - Default blur parameters
 * - Named presets for different surface types
 * - Hot reload via SIGUSR1
 *
 * See: docs/architecture/04-configuration-system.md
 *      docs/decisions/006-daemon-configuration-with-presets.md
 */

/**
 * Preset structure
 *
 * Named collection of blur parameters that can be referenced
 * by compositors instead of providing full parameter sets.
 */
struct preset {
    char name[32];                      // Preset name (e.g., "window", "panel")
    struct wlblur_blur_params params;   // Blur parameters for this preset
    struct preset *next;                // Next preset in linked list
};

/**
 * Preset registry
 *
 * Hash table for O(1) preset lookup by name.
 * Uses chaining for collision resolution.
 */
struct preset_registry {
    struct preset *buckets[64];         // Hash table buckets
    size_t preset_count;                // Total number of presets
};

/**
 * Daemon configuration
 *
 * Complete configuration loaded from TOML file.
 */
struct daemon_config {
    /* Daemon settings */
    char socket_path[256];              // Unix socket path
    char log_level[16];                 // Log level: debug, info, warn, error
    uint32_t max_nodes_per_client;      // Resource limit

    /* Default blur parameters */
    bool has_defaults;                  // true if [defaults] section present
    struct wlblur_blur_params defaults; // Default parameters

    /* Presets */
    struct preset_registry presets;     // Preset registry
};

/* === Configuration Loading === */

/**
 * Load configuration from TOML file
 *
 * Loads and parses configuration from the specified path.
 * If path is NULL, uses default location:
 *   1. $XDG_CONFIG_HOME/wlblur/config.toml
 *   2. ~/.config/wlblur/config.toml
 *   3. /etc/wlblur/config.toml
 *
 * If no config file exists, returns hardcoded defaults.
 *
 * @param path Config file path (or NULL for default)
 * @return Loaded configuration, or NULL on error
 */
struct daemon_config* config_load(const char *path);

/**
 * Validate configuration
 *
 * Checks that all parameters are within valid ranges:
 * - Algorithm must be KAWASE (m-3 constraint)
 * - num_passes: 1-8
 * - radius: 1.0-20.0
 * - brightness, contrast, saturation: 0.0-2.0
 * - noise: 0.0-1.0
 * - vibrancy: 0.0-2.0
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool config_validate(const struct daemon_config *config);

/**
 * Free configuration
 *
 * Frees all memory associated with a configuration,
 * including presets.
 *
 * @param config Configuration to free
 */
void config_free(struct daemon_config *config);

/* === Preset Management === */

/**
 * Initialize preset registry with hardcoded defaults
 *
 * Adds standard presets:
 * - window (radius=8.0, passes=3)
 * - panel (radius=4.0, passes=2)
 * - hud (radius=12.0, passes=4, vibrancy=0.2)
 * - tooltip (radius=2.0, passes=1)
 *
 * @param registry Registry to initialize
 */
void preset_registry_init(struct preset_registry *registry);

/**
 * Add preset to registry
 *
 * @param registry Registry to add to
 * @param name Preset name
 * @param params Blur parameters
 * @return true on success, false on error
 */
bool preset_registry_add(struct preset_registry *registry,
                         const char *name,
                         const struct wlblur_blur_params *params);

/**
 * Lookup preset by name
 *
 * @param registry Registry to search
 * @param name Preset name to find
 * @return Preset pointer, or NULL if not found
 */
struct preset* preset_registry_lookup(const struct preset_registry *registry,
                                      const char *name);

/**
 * Free all presets in registry
 *
 * @param registry Registry to free
 */
void preset_registry_free(struct preset_registry *registry);

/**
 * Resolve preset with fallback hierarchy
 *
 * Resolution order:
 * 1. Named preset (if provided and found)
 * 2. Direct parameters (if provided)
 * 3. Daemon defaults (from config)
 * 4. Hardcoded defaults
 *
 * @param config Daemon configuration
 * @param preset_name Preset name (or NULL)
 * @param override_params Direct parameters (or NULL)
 * @return Resolved blur parameters (never NULL)
 */
const struct wlblur_blur_params* resolve_preset(
    const struct daemon_config *config,
    const char *preset_name,
    const struct wlblur_blur_params *override_params
);

/* === Hot Reload === */

/**
 * Initialize hot reload signal handler
 *
 * Sets up SIGUSR1 handler for configuration reload.
 * Must be called after event loop setup.
 */
void reload_init(void);

/**
 * Check if reload is pending
 *
 * @return true if SIGUSR1 was received
 */
bool reload_pending(void);

/**
 * Handle configuration reload
 *
 * Reloads configuration from disk and validates it.
 * If reload fails, keeps old configuration.
 *
 * This function should be called from the main event loop
 * when reload_pending() returns true.
 *
 * @param config_path Path to config file (or NULL for default)
 * @return New config on success, NULL on failure
 */
struct daemon_config* handle_config_reload(const char *config_path);

#endif /* WLBLURD_CONFIG_H */
