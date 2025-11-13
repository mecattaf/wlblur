/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * wlblur.h - Public API for blur operations
 */

#ifndef WLBLUR_H
#define WLBLUR_H

#include <stdbool.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * EGL context for offscreen rendering
 */
struct wlblur_egl_context {
	EGLDisplay display;
	EGLContext context;
	EGLConfig config;
	bool has_dmabuf_import;
	bool has_dmabuf_export;
	bool has_surfaceless;

	/* Extension function pointers */
	PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
	PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA;
	PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
};

/**
 * Create EGL display and context for offscreen rendering
 *
 * Requirements:
 * - GLES 3.0 minimum
 * - Surfaceless context (EGL_KHR_surfaceless_context)
 * - DMA-BUF import/export extensions
 *
 * @return EGL context handle or NULL on failure
 */
struct wlblur_egl_context* wlblur_egl_create(void);

/**
 * Destroy EGL context and clean up resources
 *
 * @param ctx EGL context to destroy
 */
void wlblur_egl_destroy(struct wlblur_egl_context *ctx);

/**
 * Make EGL context current for the calling thread
 *
 * @param ctx EGL context to make current
 * @return true on success, false on failure
 */
bool wlblur_egl_make_current(struct wlblur_egl_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WLBLUR_H */
