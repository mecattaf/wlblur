/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * test-dmabuf.c - DMA-BUF import/export roundtrip test
 */

#include "wlblur/wlblur.h"
#include "wlblur/dmabuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <drm_fourcc.h>

#define TEST_WIDTH 256
#define TEST_HEIGHT 256

/**
 * Create a checkerboard test texture
 */
static GLuint create_checkerboard_texture(int width, int height) {
	/* Allocate pixel data (RGBA8888) */
	uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
	assert(pixels != NULL);

	/* Generate checkerboard pattern */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int checker = ((x / 32) + (y / 32)) % 2;
			pixels[y * width + x] = checker ? 0xFFFFFFFF : 0xFF000000;
		}
	}

	/* Create GL texture */
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	/* Upload pixel data */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	/* Set texture parameters */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(pixels);

	/* Check for errors */
	GLenum error = glGetError();
	assert(error == GL_NO_ERROR);

	return texture;
}

/**
 * Read texture pixels for comparison
 */
static uint32_t* read_texture_pixels(GLuint texture, int width, int height) {
	uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
	assert(pixels != NULL);

	/* Create framebuffer to read from texture */
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, texture, 0);

	/* Check framebuffer status */
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[test] Framebuffer incomplete: 0x%x\n", status);
		free(pixels);
		glDeleteFramebuffers(1, &fbo);
		return NULL;
	}

	/* Read pixels */
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	/* Check for errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "[test] GL error reading pixels: 0x%x\n", error);
		free(pixels);
		glDeleteFramebuffers(1, &fbo);
		return NULL;
	}

	glDeleteFramebuffers(1, &fbo);
	return pixels;
}

/**
 * Compare two texture pixel buffers
 */
static bool compare_pixels(const uint32_t *a, const uint32_t *b,
                          int width, int height) {
	int mismatches = 0;

	for (int i = 0; i < width * height; i++) {
		if (a[i] != b[i]) {
			mismatches++;
			if (mismatches <= 5) {
				fprintf(stderr, "[test] Pixel mismatch at %d: "
				        "0x%08x != 0x%08x\n", i, a[i], b[i]);
			}
		}
	}

	if (mismatches > 0) {
		fprintf(stderr, "[test] Total mismatches: %d / %d pixels\n",
		        mismatches, width * height);
		return false;
	}

	return true;
}

/**
 * Print DRM format name
 */
static const char* format_name(uint32_t format) {
	switch (format) {
	case DRM_FORMAT_ARGB8888:
		return "ARGB8888";
	case DRM_FORMAT_XRGB8888:
		return "XRGB8888";
	case DRM_FORMAT_ABGR8888:
		return "ABGR8888";
	case DRM_FORMAT_XBGR8888:
		return "XBGR8888";
	default:
		return "UNKNOWN";
	}
}

int main(void) {
	printf("=== wlblur DMA-BUF Test ===\n\n");

	/* Create EGL context */
	printf("Creating EGL context...\n");
	struct wlblur_egl_context *ctx = wlblur_egl_create();
	if (!ctx) {
		fprintf(stderr, "Failed to create EGL context\n");
		return 1;
	}

	/* Verify extensions */
	printf("Extensions:\n");
	printf("  DMA-BUF import: %s\n", ctx->has_dmabuf_import ? "YES" : "NO");
	printf("  DMA-BUF export: %s\n", ctx->has_dmabuf_export ? "YES" : "NO");
	printf("  Surfaceless: %s\n", ctx->has_surfaceless ? "YES" : "NO");
	printf("\n");

	/* Create test texture */
	printf("Creating %dx%d checkerboard texture...\n",
	       TEST_WIDTH, TEST_HEIGHT);
	GLuint source_tex = create_checkerboard_texture(TEST_WIDTH, TEST_HEIGHT);
	assert(source_tex != 0);

	/* Read source pixels for later comparison */
	uint32_t *source_pixels = read_texture_pixels(source_tex,
	                                              TEST_WIDTH, TEST_HEIGHT);
	assert(source_pixels != NULL);

	/* Export as DMA-BUF */
	printf("Exporting texture as DMA-BUF...\n");
	struct wlblur_dmabuf_attribs attribs;
	bool exported = wlblur_dmabuf_export(ctx, source_tex, &attribs);
	if (!exported) {
		fprintf(stderr, "Failed to export texture\n");
		free(source_pixels);
		glDeleteTextures(1, &source_tex);
		wlblur_egl_destroy(ctx);
		return 1;
	}

	printf("Exported DMA-BUF:\n");
	printf("  Size: %dx%d\n", attribs.width, attribs.height);
	printf("  Format: %s (0x%08x)\n", format_name(attribs.format),
	       attribs.format);
	printf("  Modifier: 0x%016lx\n", attribs.modifier);
	printf("  Planes: %d\n", attribs.num_planes);
	for (int i = 0; i < attribs.num_planes; i++) {
		printf("    Plane %d: fd=%d stride=%u offset=%u\n",
		       i, attribs.planes[i].fd, attribs.planes[i].stride,
		       attribs.planes[i].offset);
	}
	printf("\n");

	/* Import back from DMA-BUF */
	printf("Importing DMA-BUF back as texture...\n");
	GLuint imported_tex = wlblur_dmabuf_import(ctx, &attribs);
	if (imported_tex == 0) {
		fprintf(stderr, "Failed to import DMA-BUF\n");
		wlblur_dmabuf_close(&attribs);
		free(source_pixels);
		glDeleteTextures(1, &source_tex);
		wlblur_egl_destroy(ctx);
		return 1;
	}

	/* Read imported texture pixels */
	printf("Reading imported texture pixels...\n");
	uint32_t *imported_pixels = read_texture_pixels(imported_tex,
	                                               TEST_WIDTH, TEST_HEIGHT);
	if (!imported_pixels) {
		fprintf(stderr, "Failed to read imported texture\n");
		wlblur_dmabuf_close(&attribs);
		free(source_pixels);
		glDeleteTextures(1, &source_tex);
		glDeleteTextures(1, &imported_tex);
		wlblur_egl_destroy(ctx);
		return 1;
	}

	/* Compare pixels */
	printf("Comparing pixels...\n");
	bool match = compare_pixels(source_pixels, imported_pixels,
	                            TEST_WIDTH, TEST_HEIGHT);

	free(source_pixels);
	free(imported_pixels);

	if (!match) {
		fprintf(stderr, "FAILED: Pixel data mismatch\n");
		wlblur_dmabuf_close(&attribs);
		glDeleteTextures(1, &source_tex);
		glDeleteTextures(1, &imported_tex);
		wlblur_egl_destroy(ctx);
		return 1;
	}

	printf("SUCCESS: Pixels match!\n\n");

	/* Cleanup */
	printf("Cleaning up...\n");
	wlblur_dmabuf_close(&attribs);
	glDeleteTextures(1, &source_tex);
	glDeleteTextures(1, &imported_tex);
	wlblur_egl_destroy(ctx);

	printf("\n=== All tests passed! ===\n");
	return 0;
}
