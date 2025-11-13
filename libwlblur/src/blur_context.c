/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur_context.c - Context lifecycle management
 */

#include "wlblur/wlblur.h"
#include "wlblur/blur_context.h"
#include "wlblur/dmabuf.h"
#include "../private/internal.h"
#include <stdlib.h>
#include <stdio.h>

// Thread-local error state
static __thread enum wlblur_error last_error = WLBLUR_ERROR_NONE;

struct wlblur_context {
	struct wlblur_egl_context *egl_ctx;
	struct wlblur_kawase_renderer *kawase;
};

struct wlblur_context* wlblur_context_create(void) {
	struct wlblur_context *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		last_error = WLBLUR_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	// Initialize EGL
	ctx->egl_ctx = wlblur_egl_create();
	if (!ctx->egl_ctx) {
		last_error = WLBLUR_ERROR_EGL_INIT;
		free(ctx);
		return NULL;
	}

	// Check required extensions
	if (!ctx->egl_ctx->has_dmabuf_import ||
	    !ctx->egl_ctx->has_dmabuf_export) {
		last_error = WLBLUR_ERROR_MISSING_EXTENSION;
		wlblur_egl_destroy(ctx->egl_ctx);
		free(ctx);
		return NULL;
	}

	// Create Kawase renderer
	ctx->kawase = wlblur_kawase_create(ctx->egl_ctx);
	if (!ctx->kawase) {
		last_error = WLBLUR_ERROR_SHADER_COMPILE;
		wlblur_egl_destroy(ctx->egl_ctx);
		free(ctx);
		return NULL;
	}

	last_error = WLBLUR_ERROR_NONE;
	return ctx;
}

void wlblur_context_destroy(struct wlblur_context *ctx) {
	if (!ctx) return;

	wlblur_kawase_destroy(ctx->kawase);
	wlblur_egl_destroy(ctx->egl_ctx);
	free(ctx);
}

bool wlblur_apply_blur(
	struct wlblur_context *ctx,
	const struct wlblur_dmabuf_attribs *input_attribs,
	const struct wlblur_blur_params *params,
	struct wlblur_dmabuf_attribs *output_attribs
) {
	if (!ctx || !input_attribs || !params || !output_attribs) {
		last_error = WLBLUR_ERROR_INVALID_PARAMS;
		return false;
	}

	// Validate parameters
	if (!wlblur_params_validate(params)) {
		last_error = WLBLUR_ERROR_INVALID_PARAMS;
		return false;
	}

	// Make EGL context current
	if (!wlblur_egl_make_current(ctx->egl_ctx)) {
		last_error = WLBLUR_ERROR_EGL_INIT;
		return false;
	}

	// Import input DMA-BUF
	GLuint input_tex = wlblur_dmabuf_import(ctx->egl_ctx, input_attribs);
	if (input_tex == 0) {
		last_error = WLBLUR_ERROR_DMABUF_IMPORT;
		return false;
	}

	// Apply blur
	GLuint blurred_tex = wlblur_kawase_blur(
		ctx->kawase,
		input_tex,
		input_attribs->width,
		input_attribs->height,
		params
	);

	if (blurred_tex == 0) {
		last_error = WLBLUR_ERROR_GL_ERROR;
		glDeleteTextures(1, &input_tex);
		return false;
	}

	// Export result
	output_attribs->width = input_attribs->width;
	output_attribs->height = input_attribs->height;

	if (!wlblur_dmabuf_export(ctx->egl_ctx, blurred_tex,
	                          input_attribs->width, input_attribs->height,
	                          output_attribs)) {
		last_error = WLBLUR_ERROR_DMABUF_EXPORT;
		glDeleteTextures(1, &input_tex);
		return false;
	}

	// Cleanup imported texture (exported texture managed by caller)
	glDeleteTextures(1, &input_tex);

	last_error = WLBLUR_ERROR_NONE;
	return true;
}

enum wlblur_error wlblur_get_error(void) {
	return last_error;
}

const char* wlblur_error_string(enum wlblur_error error) {
	switch (error) {
	case WLBLUR_ERROR_NONE:
		return "No error";
	case WLBLUR_ERROR_EGL_INIT:
		return "EGL initialization failed";
	case WLBLUR_ERROR_MISSING_EXTENSION:
		return "Required EGL extension missing (DMA-BUF support)";
	case WLBLUR_ERROR_SHADER_COMPILE:
		return "Shader compilation failed";
	case WLBLUR_ERROR_DMABUF_IMPORT:
		return "DMA-BUF import failed";
	case WLBLUR_ERROR_DMABUF_EXPORT:
		return "DMA-BUF export failed";
	case WLBLUR_ERROR_INVALID_PARAMS:
		return "Invalid parameters";
	case WLBLUR_ERROR_GL_ERROR:
		return "OpenGL error occurred";
	case WLBLUR_ERROR_OUT_OF_MEMORY:
		return "Out of memory";
	default:
		return "Unknown error";
	}
}

struct wlblur_version wlblur_version(void) {
	static struct wlblur_version ver = {
		.major = 0,
		.minor = 1,
		.patch = 0,
		.string = "0.1.0",
	};
	return ver;
}

bool wlblur_check_version(int required_major, int required_minor) {
	struct wlblur_version ver = wlblur_version();
	if (ver.major > required_major) return true;
	if (ver.major == required_major && ver.minor >= required_minor) return true;
	return false;
}
