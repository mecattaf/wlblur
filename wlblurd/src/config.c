/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * config.c - TOML configuration parsing and validation
 */

#include "config.h"
#include <toml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Get default config path
 *
 * Priority:
 * 1. $XDG_CONFIG_HOME/wlblur/config.toml
 * 2. ~/.config/wlblur/config.toml
 * 3. /etc/wlblur/config.toml
 */
static const char* get_default_config_path(void) {
    static char path[512];

    // Try XDG_CONFIG_HOME
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        snprintf(path, sizeof(path), "%s/wlblur/config.toml", xdg_config);
        if (access(path, R_OK) == 0) {
            return path;
        }
    }

    // Try ~/.config
    const char *home = getenv("HOME");
    if (home) {
        snprintf(path, sizeof(path), "%s/.config/wlblur/config.toml", home);
        if (access(path, R_OK) == 0) {
            return path;
        }
    }

    // Try /etc
    const char *system_path = "/etc/wlblur/config.toml";
    if (access(system_path, R_OK) == 0) {
        return system_path;
    }

    return NULL;  // No config file found
}

/**
 * Parse algorithm string to enum
 */
static bool parse_algorithm(const char *str, enum wlblur_algorithm *out) {
    if (strcmp(str, "kawase") == 0) {
        *out = WLBLUR_ALGO_KAWASE;
        return true;
    }
    // Future algorithms (not supported in m-3)
    if (strcmp(str, "gaussian") == 0 || strcmp(str, "box") == 0 ||
        strcmp(str, "bokeh") == 0) {
        fprintf(stderr, "[config] Algorithm '%s' not yet supported (coming in v2.0)\n", str);
        return false;
    }
    fprintf(stderr, "[config] Unknown algorithm: %s\n", str);
    return false;
}

/**
 * Parse blur parameters from TOML table
 */
static bool parse_blur_params(toml_table_t *table, struct wlblur_blur_params *params) {
    // Initialize with defaults first
    *params = (struct wlblur_blur_params){
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

    // Parse algorithm
    toml_datum_t algo = toml_string_in(table, "algorithm");
    if (algo.ok) {
        if (!parse_algorithm(algo.u.s, &params->algorithm)) {
            free(algo.u.s);
            return false;
        }
        free(algo.u.s);
    }

    // Parse num_passes
    toml_datum_t passes = toml_int_in(table, "num_passes");
    if (passes.ok) {
        params->num_passes = passes.u.i;
    }

    // Parse radius
    toml_datum_t radius = toml_double_in(table, "radius");
    if (radius.ok) {
        params->radius = radius.u.d;
    }

    // Parse brightness
    toml_datum_t brightness = toml_double_in(table, "brightness");
    if (brightness.ok) {
        params->brightness = brightness.u.d;
    }

    // Parse contrast
    toml_datum_t contrast = toml_double_in(table, "contrast");
    if (contrast.ok) {
        params->contrast = contrast.u.d;
    }

    // Parse saturation
    toml_datum_t saturation = toml_double_in(table, "saturation");
    if (saturation.ok) {
        params->saturation = saturation.u.d;
    }

    // Parse noise
    toml_datum_t noise = toml_double_in(table, "noise");
    if (noise.ok) {
        params->noise = noise.u.d;
    }

    // Parse vibrancy
    toml_datum_t vibrancy = toml_double_in(table, "vibrancy");
    if (vibrancy.ok) {
        params->vibrancy = vibrancy.u.d;
    }

    return true;
}

/**
 * Validate blur parameters
 */
static bool validate_blur_params(const struct wlblur_blur_params *params, const char *context) {
    // Algorithm (m-3: only kawase)
    if (params->algorithm != WLBLUR_ALGO_KAWASE) {
        fprintf(stderr, "[config] %s: only 'kawase' algorithm supported in this version\n", context);
        return false;
    }

    // num_passes
    if (params->num_passes < 1 || params->num_passes > 8) {
        fprintf(stderr, "[config] %s: num_passes must be 1-8, got %d\n", context, params->num_passes);
        return false;
    }

    // radius
    if (params->radius < 1.0 || params->radius > 20.0) {
        fprintf(stderr, "[config] %s: radius must be 1.0-20.0, got %.1f\n", context, params->radius);
        return false;
    }

    // brightness
    if (params->brightness < 0.0 || params->brightness > 2.0) {
        fprintf(stderr, "[config] %s: brightness must be 0.0-2.0, got %.2f\n", context, params->brightness);
        return false;
    }

    // contrast
    if (params->contrast < 0.0 || params->contrast > 2.0) {
        fprintf(stderr, "[config] %s: contrast must be 0.0-2.0, got %.2f\n", context, params->contrast);
        return false;
    }

    // saturation
    if (params->saturation < 0.0 || params->saturation > 2.0) {
        fprintf(stderr, "[config] %s: saturation must be 0.0-2.0, got %.2f\n", context, params->saturation);
        return false;
    }

    // noise
    if (params->noise < 0.0 || params->noise > 1.0) {
        fprintf(stderr, "[config] %s: noise must be 0.0-1.0, got %.2f\n", context, params->noise);
        return false;
    }

    // vibrancy
    if (params->vibrancy < 0.0 || params->vibrancy > 2.0) {
        fprintf(stderr, "[config] %s: vibrancy must be 0.0-2.0, got %.2f\n", context, params->vibrancy);
        return false;
    }

    return true;
}

/**
 * Create config with hardcoded defaults
 */
static struct daemon_config* config_default(void) {
    struct daemon_config *config = calloc(1, sizeof(*config));
    if (!config) {
        return NULL;
    }

    // Daemon settings
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
    }
    snprintf(config->socket_path, sizeof(config->socket_path),
             "%s/wlblur.sock", runtime_dir);
    strncpy(config->log_level, "info", sizeof(config->log_level) - 1);
    config->max_nodes_per_client = 100;

    // Default parameters
    config->has_defaults = true;
    config->defaults = (struct wlblur_blur_params){
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

    // Initialize preset registry with standard presets
    preset_registry_init(&config->presets);

    printf("[config] Using hardcoded defaults\n");
    return config;
}

/**
 * Load configuration from TOML file
 */
struct daemon_config* config_load(const char *path) {
    // Determine config path
    if (!path) {
        path = get_default_config_path();
        if (!path) {
            printf("[config] No config file found, using defaults\n");
            return config_default();
        }
    }

    printf("[config] Loading configuration from: %s\n", path);

    // Open and parse TOML file
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[config] Failed to open %s: %s\n", path, strerror(errno));
        fprintf(stderr, "[config] Using defaults\n");
        return config_default();
    }

    char errbuf[200];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        fprintf(stderr, "[config] TOML parse error: %s\n", errbuf);
        fprintf(stderr, "[config] Using defaults\n");
        return config_default();
    }

    // Allocate config
    struct daemon_config *config = calloc(1, sizeof(*config));
    if (!config) {
        toml_free(root);
        return NULL;
    }

    // Set defaults
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
    }
    snprintf(config->socket_path, sizeof(config->socket_path),
             "%s/wlblur.sock", runtime_dir);
    strncpy(config->log_level, "info", sizeof(config->log_level) - 1);
    config->max_nodes_per_client = 100;

    // Parse [daemon] section
    toml_table_t *daemon = toml_table_in(root, "daemon");
    if (daemon) {
        toml_datum_t socket = toml_string_in(daemon, "socket_path");
        if (socket.ok) {
            strncpy(config->socket_path, socket.u.s, sizeof(config->socket_path) - 1);
            free(socket.u.s);
        }

        toml_datum_t log = toml_string_in(daemon, "log_level");
        if (log.ok) {
            strncpy(config->log_level, log.u.s, sizeof(config->log_level) - 1);
            free(log.u.s);
        }

        toml_datum_t max_nodes = toml_int_in(daemon, "max_nodes_per_client");
        if (max_nodes.ok) {
            config->max_nodes_per_client = max_nodes.u.i;
        }
    }

    // Parse [defaults] section
    toml_table_t *defaults = toml_table_in(root, "defaults");
    if (defaults) {
        config->has_defaults = true;
        if (!parse_blur_params(defaults, &config->defaults)) {
            fprintf(stderr, "[config] Failed to parse [defaults] section\n");
            toml_free(root);
            config_free(config);
            return config_default();
        }
        if (!validate_blur_params(&config->defaults, "[defaults]")) {
            toml_free(root);
            config_free(config);
            return config_default();
        }
    } else {
        config->has_defaults = false;
    }

    // Initialize preset registry
    preset_registry_init(&config->presets);

    // Parse [presets.*] sections
    toml_table_t *presets = toml_table_in(root, "presets");
    if (presets) {
        for (int i = 0; ; i++) {
            const char *key = toml_key_in(presets, i);
            if (!key) break;

            toml_table_t *preset_table = toml_table_in(presets, key);
            if (!preset_table) continue;

            struct wlblur_blur_params params;
            if (!parse_blur_params(preset_table, &params)) {
                fprintf(stderr, "[config] Failed to parse preset '%s'\n", key);
                continue;
            }

            char context[64];
            snprintf(context, sizeof(context), "preset '%s'", key);
            if (!validate_blur_params(&params, context)) {
                continue;
            }

            if (!preset_registry_add(&config->presets, key, &params)) {
                fprintf(stderr, "[config] Failed to add preset '%s'\n", key);
            }
        }
    }

    toml_free(root);

    printf("[config] Loaded %zu presets\n", config->presets.preset_count);
    return config;
}

/**
 * Validate configuration
 */
bool config_validate(const struct daemon_config *config) {
    if (!config) {
        return false;
    }

    // Validate defaults if present
    if (config->has_defaults) {
        if (!validate_blur_params(&config->defaults, "defaults")) {
            return false;
        }
    }

    // Could add more validation here
    return true;
}

/**
 * Free configuration
 */
void config_free(struct daemon_config *config) {
    if (!config) {
        return;
    }

    preset_registry_free(&config->presets);
    free(config);
}
