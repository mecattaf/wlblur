/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * reload.c - Hot reload functionality via SIGUSR1
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Reload flag
 *
 * Set by signal handler, checked by main loop.
 * Using volatile sig_atomic_t for signal safety.
 */
static volatile sig_atomic_t reload_requested = 0;

/**
 * Signal handler for SIGUSR1
 *
 * Safe signal handler - only sets a flag.
 */
static void sigusr1_handler(int sig) {
    (void)sig;
    reload_requested = 1;
}

/**
 * Initialize hot reload signal handler
 */
void reload_init(void) {
    struct sigaction sa = {
        .sa_handler = sigusr1_handler,
        .sa_flags = SA_RESTART,  // Restart interrupted system calls
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("[reload] sigaction(SIGUSR1)");
        return;
    }

    printf("[reload] Hot reload initialized (send SIGUSR1 to reload)\n");
}

/**
 * Check if reload is pending
 */
bool reload_pending(void) {
    return reload_requested != 0;
}

/**
 * Handle configuration reload
 *
 * This function should be called from the main event loop
 * when reload_pending() returns true.
 *
 * Returns new config on success, NULL on failure.
 */
struct daemon_config* handle_config_reload(const char *config_path) {
    // Clear reload flag
    reload_requested = 0;

    printf("[reload] Reloading configuration...\n");

    // Load new config
    struct daemon_config *new_config = config_load(config_path);
    if (!new_config) {
        fprintf(stderr, "[reload] Failed to load config - keeping old configuration\n");
        return NULL;
    }

    // Validate new config
    if (!config_validate(new_config)) {
        fprintf(stderr, "[reload] Config validation failed - keeping old configuration\n");
        config_free(new_config);
        return NULL;
    }

    printf("[reload] Configuration reloaded successfully\n");
    printf("[reload]   Presets loaded: %zu\n", new_config->presets.preset_count);

    return new_config;
}
