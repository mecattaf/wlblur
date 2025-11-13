/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * egl_helpers.c - EGL utility functions
 */

#include "wlblur/wlblur.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool check_egl_extension(const char *exts, const char *name) {
	if (!exts || !name) {
		return false;
	}

	size_t name_len = strlen(name);
	const char *pos = exts;

	while ((pos = strstr(pos, name)) != NULL) {
		/* Check that it's a complete match (not a substring) */
		if ((pos == exts || pos[-1] == ' ') &&
		    (pos[name_len] == ' ' || pos[name_len] == '\0')) {
			return true;
		}
		pos += name_len;
	}

	return false;
}

struct wlblur_egl_context* wlblur_egl_create(void) {
	struct wlblur_egl_context *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		fprintf(stderr, "[wlblur] Failed to allocate EGL context\n");
		return NULL;
	}

	/* Get default display */
	ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (ctx->display == EGL_NO_DISPLAY) {
		fprintf(stderr, "[wlblur] Failed to get EGL display: 0x%x\n",
		        eglGetError());
		goto error;
	}

	/* Initialize EGL */
	EGLint major, minor;
	if (!eglInitialize(ctx->display, &major, &minor)) {
		fprintf(stderr, "[wlblur] Failed to initialize EGL: 0x%x\n",
		        eglGetError());
		goto error;
	}

	fprintf(stderr, "[wlblur] EGL %d.%d initialized\n", major, minor);

	/* Check required extensions */
	const char *egl_exts = eglQueryString(ctx->display, EGL_EXTENSIONS);
	if (!egl_exts) {
		fprintf(stderr, "[wlblur] Failed to query EGL extensions\n");
		goto error_terminate;
	}

	/* Check for surfaceless context support */
	ctx->has_surfaceless = check_egl_extension(egl_exts,
	                                           "EGL_KHR_surfaceless_context") ||
	                       check_egl_extension(egl_exts,
	                                           "EGL_KHR_surfaceless_opengl");

	if (!ctx->has_surfaceless) {
		fprintf(stderr, "[wlblur] EGL_KHR_surfaceless_context not available\n");
		goto error_terminate;
	}

	/* Check for DMA-BUF import */
	ctx->has_dmabuf_import =
		check_egl_extension(egl_exts, "EGL_EXT_image_dma_buf_import") &&
		check_egl_extension(egl_exts, "EGL_KHR_image_base");

	if (!ctx->has_dmabuf_import) {
		fprintf(stderr, "[wlblur] DMA-BUF import extensions not available\n");
		goto error_terminate;
	}

	/* Check for DMA-BUF export */
	ctx->has_dmabuf_export =
		check_egl_extension(egl_exts, "EGL_MESA_image_dma_buf_export");

	if (!ctx->has_dmabuf_export) {
		fprintf(stderr, "[wlblur] DMA-BUF export extension not available\n");
		goto error_terminate;
	}

	/* Bind OpenGL ES API */
	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "[wlblur] Failed to bind OpenGL ES API: 0x%x\n",
		        eglGetError());
		goto error_terminate;
	}

	/* Choose EGL config */
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_DONT_CARE,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE,
	};

	EGLint num_configs;
	if (!eglChooseConfig(ctx->display, config_attribs, &ctx->config, 1,
	                     &num_configs) || num_configs == 0) {
		fprintf(stderr, "[wlblur] Failed to choose EGL config: 0x%x\n",
		        eglGetError());
		goto error_terminate;
	}

	/* Create EGL context */
	EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 0,
		EGL_NONE,
	};

	ctx->context = eglCreateContext(ctx->display, ctx->config,
	                                EGL_NO_CONTEXT, context_attribs);
	if (ctx->context == EGL_NO_CONTEXT) {
		fprintf(stderr, "[wlblur] Failed to create EGL context: 0x%x\n",
		        eglGetError());
		goto error_terminate;
	}

	/* Make context current */
	if (!eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
	                    ctx->context)) {
		fprintf(stderr, "[wlblur] Failed to make EGL context current: 0x%x\n",
		        eglGetError());
		goto error_context;
	}

	/* Load extension function pointers */
	ctx->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
		eglGetProcAddress("eglCreateImageKHR");
	ctx->eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
		eglGetProcAddress("eglDestroyImageKHR");
	ctx->eglExportDMABUFImageMESA = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)
		eglGetProcAddress("eglExportDMABUFImageMESA");
	ctx->eglExportDMABUFImageQueryMESA = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)
		eglGetProcAddress("eglExportDMABUFImageQueryMESA");
	ctx->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
		eglGetProcAddress("glEGLImageTargetTexture2DOES");

	if (!ctx->eglCreateImageKHR || !ctx->eglDestroyImageKHR ||
	    !ctx->glEGLImageTargetTexture2DOES) {
		fprintf(stderr, "[wlblur] Failed to load required extension functions\n");
		goto error_context;
	}

	if (!ctx->eglExportDMABUFImageMESA || !ctx->eglExportDMABUFImageQueryMESA) {
		fprintf(stderr, "[wlblur] Failed to load DMA-BUF export functions\n");
		goto error_context;
	}

	/* Verify GL version */
	const char *gl_version = (const char *)glGetString(GL_VERSION);
	fprintf(stderr, "[wlblur] OpenGL ES version: %s\n",
	        gl_version ? gl_version : "unknown");

	/* Check for GL errors */
	GLenum gl_error = glGetError();
	if (gl_error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error during initialization: 0x%x\n",
		        gl_error);
		goto error_context;
	}

	fprintf(stderr, "[wlblur] EGL context created successfully\n");
	return ctx;

error_context:
	eglDestroyContext(ctx->display, ctx->context);
error_terminate:
	eglTerminate(ctx->display);
error:
	free(ctx);
	return NULL;
}

void wlblur_egl_destroy(struct wlblur_egl_context *ctx) {
	if (!ctx) {
		return;
	}

	if (ctx->display != EGL_NO_DISPLAY) {
		eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
		               EGL_NO_CONTEXT);

		if (ctx->context != EGL_NO_CONTEXT) {
			eglDestroyContext(ctx->display, ctx->context);
		}

		eglTerminate(ctx->display);
	}

	free(ctx);
}

bool wlblur_egl_make_current(struct wlblur_egl_context *ctx) {
	if (!ctx || ctx->display == EGL_NO_DISPLAY ||
	    ctx->context == EGL_NO_CONTEXT) {
		return false;
	}

	if (!eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
	                    ctx->context)) {
		fprintf(stderr, "[wlblur] Failed to make context current: 0x%x\n",
		        eglGetError());
		return false;
	}

	return true;
}
