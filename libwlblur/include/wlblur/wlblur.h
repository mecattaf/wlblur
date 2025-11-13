/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * wlblur.h - Main public API
 */

#ifndef WLBLUR_H
#define WLBLUR_H

#include <stdint.h>
#include <stdbool.h>
#include <wlblur/blur_params.h>
#include <wlblur/dmabuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque blur context handle
 *
 * Contains EGL context, shader programs, FBO pool, and all rendering state.
 * Thread-safety: One context per thread. Do not share across threads.
 */
struct wlblur_context;

/**
 * Library version information
 */
struct wlblur_version {
	int major;
	int minor;
	int patch;
	const char *string;  // e.g., "0.1.0"
};

/**
 * Error codes
 */
enum wlblur_error {
	WLBLUR_ERROR_NONE = 0,
	WLBLUR_ERROR_EGL_INIT,          // EGL initialization failed
	WLBLUR_ERROR_MISSING_EXTENSION,  // Required EGL extension missing
	WLBLUR_ERROR_SHADER_COMPILE,     // Shader compilation failed
	WLBLUR_ERROR_DMABUF_IMPORT,      // DMA-BUF import failed
	WLBLUR_ERROR_DMABUF_EXPORT,      // DMA-BUF export failed
	WLBLUR_ERROR_INVALID_PARAMS,     // Parameter validation failed
	WLBLUR_ERROR_GL_ERROR,           // OpenGL error occurred
	WLBLUR_ERROR_OUT_OF_MEMORY,      // Memory allocation failed
};

/* === Context Management === */

/**
 * Create blur context
 *
 * Initializes:
 * - EGL context with GLES 3.0
 * - Shader programs (Kawase, effects)
 * - FBO pool
 * - Extension detection
 *
 * @return Context handle or NULL on failure
 *
 * Example:
 *   struct wlblur_context *ctx = wlblur_context_create();
 *   if (!ctx) {
 *       fprintf(stderr, "Failed to create blur context: %s\n",
 *               wlblur_error_string(wlblur_get_error()));
 *       return -1;
 *   }
 */
struct wlblur_context* wlblur_context_create(void);

/**
 * Destroy blur context
 *
 * Frees all resources:
 * - Shader programs
 * - FBO pool
 * - EGL context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void wlblur_context_destroy(struct wlblur_context *ctx);

/* === Blur Operations === */

/**
 * Apply blur to DMA-BUF texture
 *
 * This is the main API function that performs:
 * 1. Import input DMA-BUF as GL texture
 * 2. Apply Dual Kawase blur (multi-pass)
 * 3. Apply post-processing effects
 * 4. Export result as DMA-BUF
 *
 * @param ctx Blur context
 * @param input_attribs Input DMA-BUF attributes (from compositor)
 * @param params Blur parameters (passes, radius, effects)
 * @param output_attribs Output DMA-BUF attributes (filled by function)
 *
 * @return true on success, false on failure (check wlblur_get_error())
 *
 * Example:
 *   struct wlblur_dmabuf_attribs input = get_backdrop_dmabuf();
 *   struct wlblur_blur_params params = wlblur_params_default();
 *   struct wlblur_dmabuf_attribs output;
 *
 *   if (!wlblur_apply_blur(ctx, &input, &params, &output)) {
 *       fprintf(stderr, "Blur failed: %s\n",
 *               wlblur_error_string(wlblur_get_error()));
 *       return;
 *   }
 *
 *   // Use output.planes[0].fd as blurred texture
 *   composite_blurred_texture(output.planes[0].fd);
 *
 *   // Cleanup
 *   wlblur_dmabuf_close(&output);
 *
 * Ownership:
 * - input_attribs: Caller retains ownership (wlblur does not close FDs)
 * - output_attribs: Caller owns FDs, must call wlblur_dmabuf_close()
 *
 * Performance: ~1.4ms @ 1080p (3 passes, radius=5)
 */
bool wlblur_apply_blur(
	struct wlblur_context *ctx,
	const struct wlblur_dmabuf_attribs *input_attribs,
	const struct wlblur_blur_params *params,
	struct wlblur_dmabuf_attribs *output_attribs
);

/* === Error Handling === */

/**
 * Get last error code
 *
 * Thread-local error state. Cleared on next successful operation.
 *
 * @return Error code or WLBLUR_ERROR_NONE
 */
enum wlblur_error wlblur_get_error(void);

/**
 * Get human-readable error string
 *
 * @param error Error code
 * @return Error description (static string, do not free)
 */
const char* wlblur_error_string(enum wlblur_error error);

/* === Version Information === */

/**
 * Get library version
 *
 * @return Version structure (static, do not free)
 */
struct wlblur_version wlblur_version(void);

/**
 * Check API compatibility
 *
 * @param required_major Minimum required major version
 * @param required_minor Minimum required minor version
 * @return true if library version >= required version
 *
 * Example:
 *   if (!wlblur_check_version(0, 1)) {
 *       fprintf(stderr, "libwlblur too old, need 0.1+\n");
 *       return -1;
 *   }
 */
bool wlblur_check_version(int required_major, int required_minor);

#ifdef __cplusplus
}
#endif

#endif /* WLBLUR_H */
