/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * dmabuf.c - DMA-BUF import/export implementation
 */

#include "wlblur/dmabuf.h"
#include "wlblur/wlblur.h"
#include "private/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <drm_fourcc.h>

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

GLuint wlblur_dmabuf_import(
	struct wlblur_egl_context *ctx,
	const struct wlblur_dmabuf_attribs *attribs
) {
	if (!ctx || !attribs) {
		fprintf(stderr, "[wlblur] Invalid arguments to dmabuf_import\n");
		return 0;
	}

	if (!ctx->has_dmabuf_import) {
		fprintf(stderr, "[wlblur] DMA-BUF import not supported\n");
		return 0;
	}

	if (attribs->num_planes < 1 || attribs->num_planes > 4) {
		fprintf(stderr, "[wlblur] Invalid number of planes: %d\n",
		        attribs->num_planes);
		return 0;
	}

	/* Build EGL attribute list */
	EGLint egl_attribs[50];
	int i = 0;

	egl_attribs[i++] = EGL_WIDTH;
	egl_attribs[i++] = attribs->width;
	egl_attribs[i++] = EGL_HEIGHT;
	egl_attribs[i++] = attribs->height;
	egl_attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
	egl_attribs[i++] = attribs->format;

	/* Add plane 0 attributes (always required) */
	egl_attribs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
	egl_attribs[i++] = attribs->planes[0].fd;
	egl_attribs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
	egl_attribs[i++] = attribs->planes[0].offset;
	egl_attribs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
	egl_attribs[i++] = attribs->planes[0].stride;

	if (attribs->modifier != DRM_FORMAT_MOD_INVALID) {
		egl_attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
		egl_attribs[i++] = attribs->modifier & 0xFFFFFFFF;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
		egl_attribs[i++] = attribs->modifier >> 32;
	}

	/* Add plane 1 if present */
	if (attribs->num_planes > 1) {
		egl_attribs[i++] = EGL_DMA_BUF_PLANE1_FD_EXT;
		egl_attribs[i++] = attribs->planes[1].fd;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
		egl_attribs[i++] = attribs->planes[1].offset;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
		egl_attribs[i++] = attribs->planes[1].stride;

		if (attribs->modifier != DRM_FORMAT_MOD_INVALID) {
			egl_attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
			egl_attribs[i++] = attribs->modifier & 0xFFFFFFFF;
			egl_attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
			egl_attribs[i++] = attribs->modifier >> 32;
		}
	}

	/* Add plane 2 if present */
	if (attribs->num_planes > 2) {
		egl_attribs[i++] = EGL_DMA_BUF_PLANE2_FD_EXT;
		egl_attribs[i++] = attribs->planes[2].fd;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
		egl_attribs[i++] = attribs->planes[2].offset;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
		egl_attribs[i++] = attribs->planes[2].stride;

		if (attribs->modifier != DRM_FORMAT_MOD_INVALID) {
			egl_attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
			egl_attribs[i++] = attribs->modifier & 0xFFFFFFFF;
			egl_attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
			egl_attribs[i++] = attribs->modifier >> 32;
		}
	}

	/* Add plane 3 if present */
	if (attribs->num_planes > 3) {
		egl_attribs[i++] = EGL_DMA_BUF_PLANE3_FD_EXT;
		egl_attribs[i++] = attribs->planes[3].fd;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
		egl_attribs[i++] = attribs->planes[3].offset;
		egl_attribs[i++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
		egl_attribs[i++] = attribs->planes[3].stride;

		if (attribs->modifier != DRM_FORMAT_MOD_INVALID) {
			egl_attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
			egl_attribs[i++] = attribs->modifier & 0xFFFFFFFF;
			egl_attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
			egl_attribs[i++] = attribs->modifier >> 32;
		}
	}

	egl_attribs[i++] = EGL_NONE;

	/* Create EGLImage from DMA-BUF */
	EGLImageKHR image = ctx->eglCreateImageKHR(
		ctx->display,
		EGL_NO_CONTEXT,
		EGL_LINUX_DMA_BUF_EXT,
		NULL,
		egl_attribs
	);

	if (image == EGL_NO_IMAGE_KHR) {
		fprintf(stderr, "[wlblur] Failed to create EGLImage from DMA-BUF: 0x%x\n",
		        eglGetError());
		return 0;
	}

	/* Create GL texture */
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	/* Import EGLImage as texture */
	ctx->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	/* Set texture parameters */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Check for GL errors */
	GLenum gl_error = glGetError();
	if (gl_error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error during texture import: 0x%x\n",
		        gl_error);
		glDeleteTextures(1, &texture);
		ctx->eglDestroyImageKHR(ctx->display, image);
		return 0;
	}

	/* Cleanup EGLImage (texture retains reference) */
	ctx->eglDestroyImageKHR(ctx->display, image);

	return texture;
}

bool wlblur_dmabuf_export(
	struct wlblur_egl_context *ctx,
	GLuint texture,
	int width,
	int height,
	struct wlblur_dmabuf_attribs *attribs
) {
	if (!ctx || !texture || !attribs) {
		fprintf(stderr, "[wlblur] Invalid arguments to dmabuf_export\n");
		return false;
	}

	if (!ctx->has_dmabuf_export) {
		fprintf(stderr, "[wlblur] DMA-BUF export not supported\n");
		return false;
	}

	/* Clear output structure */
	memset(attribs, 0, sizeof(*attribs));

	/* Set dimensions */
	attribs->width = width;
	attribs->height = height;

	/* Create EGLImage from GL texture */
	EGLImageKHR image = ctx->eglCreateImageKHR(
		ctx->display,
		ctx->context,
		EGL_GL_TEXTURE_2D,
		(EGLClientBuffer)(uintptr_t)texture,
		NULL
	);

	if (image == EGL_NO_IMAGE_KHR) {
		fprintf(stderr, "[wlblur] Failed to create EGLImage from texture: 0x%x\n",
		        eglGetError());
		return false;
	}

	/* Query DMA-BUF attributes */
	int fourcc;
	int num_planes;
	uint64_t modifier;

	if (!ctx->eglExportDMABUFImageQueryMESA(
			ctx->display, image,
			&fourcc, &num_planes, &modifier)) {
		fprintf(stderr, "[wlblur] Failed to query DMA-BUF attributes: 0x%x\n",
		        eglGetError());
		ctx->eglDestroyImageKHR(ctx->display, image);
		return false;
	}

	if (num_planes < 1 || num_planes > 4) {
		fprintf(stderr, "[wlblur] Invalid number of planes from export: %d\n",
		        num_planes);
		ctx->eglDestroyImageKHR(ctx->display, image);
		return false;
	}

	attribs->format = fourcc;
	attribs->modifier = modifier;
	attribs->num_planes = num_planes;

	/* Export DMA-BUF file descriptors and plane info */
	int fds[4];
	EGLint strides[4];
	EGLint offsets[4];

	if (!ctx->eglExportDMABUFImageMESA(
			ctx->display, image,
			fds, strides, offsets)) {
		fprintf(stderr, "[wlblur] Failed to export DMA-BUF: 0x%x\n",
		        eglGetError());
		ctx->eglDestroyImageKHR(ctx->display, image);
		return false;
	}

	/* Fill plane info */
	for (int i = 0; i < num_planes; i++) {
		attribs->planes[i].fd = fds[i];
		attribs->planes[i].stride = strides[i];
		attribs->planes[i].offset = offsets[i];
	}

	/* Check for GL errors */
	GLenum gl_error = glGetError();
	if (gl_error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error during texture export: 0x%x\n",
		        gl_error);
		wlblur_dmabuf_close(attribs);
		ctx->eglDestroyImageKHR(ctx->display, image);
		return false;
	}

	ctx->eglDestroyImageKHR(ctx->display, image);
	return true;
}

void wlblur_dmabuf_close(struct wlblur_dmabuf_attribs *attribs) {
	if (!attribs) {
		return;
	}

	for (int i = 0; i < attribs->num_planes && i < 4; i++) {
		if (attribs->planes[i].fd >= 0) {
			close(attribs->planes[i].fd);
			attribs->planes[i].fd = -1;
		}
	}

	attribs->num_planes = 0;
}
