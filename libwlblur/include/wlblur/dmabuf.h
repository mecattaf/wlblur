/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * dmabuf.h - DMA-BUF import/export helpers
 */

#ifndef WLBLUR_DMABUF_H
#define WLBLUR_DMABUF_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct wlblur_egl_context;

/**
 * DMA-BUF plane attributes
 */
struct wlblur_dmabuf_plane {
	int fd;           /* File descriptor */
	uint32_t offset;  /* Byte offset in buffer */
	uint32_t stride;  /* Bytes per row */
};

/**
 * DMA-BUF texture attributes (from compositor)
 */
struct wlblur_dmabuf_attribs {
	int width;
	int height;
	uint32_t format;      /* DRM_FORMAT_* (drm_fourcc.h) */
	uint64_t modifier;    /* DRM_FORMAT_MOD_* (usually LINEAR) */
	int num_planes;       /* 1 for ARGB8888 */
	struct wlblur_dmabuf_plane planes[4];
};

/**
 * Import DMA-BUF as OpenGL texture
 *
 * Creates a GL texture from DMA-BUF file descriptor and attributes.
 * This enables zero-copy texture sharing from compositor to daemon.
 *
 * Steps:
 * 1. Create EGLImage from DMA-BUF FD + attributes
 * 2. Create GL texture
 * 3. Bind EGLImage to texture
 *
 * @param ctx EGL context (must be current)
 * @param attribs DMA-BUF attributes (caller retains FD ownership)
 * @return GL texture ID or 0 on failure
 */
GLuint wlblur_dmabuf_import(
	struct wlblur_egl_context *ctx,
	const struct wlblur_dmabuf_attribs *attribs
);

/**
 * Export GL texture as DMA-BUF
 *
 * Creates DMA-BUF file descriptor from GL texture.
 * This enables zero-copy texture sharing from daemon to compositor.
 *
 * Steps:
 * 1. Create EGLImage from GL texture
 * 2. Export EGLImage as DMA-BUF
 * 3. Return FD + attributes
 *
 * @param ctx EGL context (must be current)
 * @param texture GL texture to export
 * @param width Texture width
 * @param height Texture height
 * @param attribs Output DMA-BUF attributes (caller owns FDs)
 * @return true on success, false on failure
 */
bool wlblur_dmabuf_export(
	struct wlblur_egl_context *ctx,
	GLuint texture,
	int width,
	int height,
	struct wlblur_dmabuf_attribs *attribs
);

/**
 * Close DMA-BUF file descriptors
 *
 * Closes all file descriptors in the DMA-BUF attributes structure.
 * Should be called when the DMA-BUF is no longer needed.
 *
 * @param attribs DMA-BUF attributes with FDs to close
 */
void wlblur_dmabuf_close(struct wlblur_dmabuf_attribs *attribs);

#ifdef __cplusplus
}
#endif

#endif /* WLBLUR_DMABUF_H */
