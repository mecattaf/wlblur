/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur-png.c - Test libwlblur with test pattern
 */

#define _POSIX_C_SOURCE 199309L

#include "wlblur/wlblur.h"
#include "wlblur/blur_params.h"
#include "../libwlblur/private/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/**
 * Create test texture with checkerboard pattern
 */
static GLuint create_test_pattern(int width, int height) {
	/* Allocate RGBA buffer */
	size_t buffer_size = width * height * 4;
	unsigned char *pixels = malloc(buffer_size);
	if (!pixels) {
		fprintf(stderr, "Failed to allocate pixel buffer\n");
		return 0;
	}

	/* Generate checkerboard pattern */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int idx = (y * width + x) * 4;

			/* 64x64 checkerboard */
			bool is_white = ((x / 64) + (y / 64)) % 2 == 0;

			if (is_white) {
				pixels[idx + 0] = 255;  /* R */
				pixels[idx + 1] = 255;  /* G */
				pixels[idx + 2] = 255;  /* B */
			} else {
				pixels[idx + 0] = 64;   /* R */
				pixels[idx + 1] = 64;   /* G */
				pixels[idx + 2] = 64;   /* B */
			}
			pixels[idx + 3] = 255;  /* A */
		}
	}

	/* Create GL texture */
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
	             width, height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(pixels);

	/* Check for errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "GL error creating test pattern: 0x%x\n", error);
		glDeleteTextures(1, &texture);
		return 0;
	}

	return texture;
}

/**
 * Run blur test with given parameters
 */
static bool run_blur_test(
	struct wlblur_kawase_renderer *renderer,
	int width,
	int height,
	const struct wlblur_blur_params *params
) {
	printf("Testing blur: %dx%d, passes=%d, radius=%.1f\n",
	       width, height, params->num_passes, params->radius);

	/* Create test texture */
	GLuint test_tex = create_test_pattern(width, height);
	if (!test_tex) {
		fprintf(stderr, "  FAILED: Could not create test pattern\n");
		return false;
	}

	/* Measure blur time */
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	GLuint blurred_tex = wlblur_kawase_blur(
		renderer, test_tex, width, height, params
	);

	/* Force GPU sync */
	glFinish();

	clock_gettime(CLOCK_MONOTONIC, &end);
	double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
	                    (end.tv_nsec - start.tv_nsec) / 1000000.0;

	/* Cleanup */
	glDeleteTextures(1, &test_tex);

	if (!blurred_tex) {
		fprintf(stderr, "  FAILED: Blur returned null texture\n");
		return false;
	}

	printf("  PASSED: Completed in %.2f ms\n", elapsed_ms);

	/* Check performance target */
	if (width == 1920 && height == 1080 && params->num_passes == 3) {
		if (elapsed_ms > 2.0) {
			printf("  WARNING: Performance target missed (%.2f ms > 2.0 ms)\n",
			       elapsed_ms);
		} else {
			printf("  Performance target met: %.2f ms < 2.0 ms\n",
			       elapsed_ms);
		}
	}

	return true;
}

int main(int argc, char **argv) {
	printf("wlblur Dual Kawase blur test\n");
	printf("=============================\n\n");

	/* Initialize EGL */
	printf("Initializing EGL...\n");
	struct wlblur_egl_context *egl_ctx = wlblur_egl_create();
	if (!egl_ctx) {
		fprintf(stderr, "FAILED: Could not create EGL context\n");
		return 1;
	}
	printf("  EGL context created\n\n");

	/* Create blur renderer */
	printf("Creating Kawase blur renderer...\n");
	struct wlblur_kawase_renderer *renderer = wlblur_kawase_create(egl_ctx);
	if (!renderer) {
		fprintf(stderr, "FAILED: Could not create blur renderer\n");
		wlblur_egl_destroy(egl_ctx);
		return 1;
	}
	printf("  Renderer created\n\n");

	/* Test 1: Default parameters */
	printf("Test 1: Default parameters (3 passes, radius 5.0)\n");
	struct wlblur_blur_params params_default = wlblur_params_default();
	if (!run_blur_test(renderer, 512, 512, &params_default)) {
		goto cleanup;
	}
	printf("\n");

	/* Test 2: 1080p performance test */
	printf("Test 2: 1080p performance test\n");
	if (!run_blur_test(renderer, 1920, 1080, &params_default)) {
		goto cleanup;
	}
	printf("\n");

	/* Test 3: Single pass (fast) */
	printf("Test 3: Single pass blur\n");
	struct wlblur_blur_params params_fast = params_default;
	params_fast.num_passes = 1;
	if (!run_blur_test(renderer, 512, 512, &params_fast)) {
		goto cleanup;
	}
	printf("\n");

	/* Test 4: High quality (5 passes) */
	printf("Test 4: High quality blur (5 passes)\n");
	struct wlblur_blur_params params_hq = params_default;
	params_hq.num_passes = 5;
	if (!run_blur_test(renderer, 512, 512, &params_hq)) {
		goto cleanup;
	}
	printf("\n");

	/* Test 5: Different resolutions */
	printf("Test 5: 720p resolution\n");
	if (!run_blur_test(renderer, 1280, 720, &params_default)) {
		goto cleanup;
	}
	printf("\n");

	printf("Test 6: 4K resolution\n");
	if (!run_blur_test(renderer, 3840, 2160, &params_default)) {
		goto cleanup;
	}
	printf("\n");

	/* Test 7: Post-processing effects */
	printf("Test 7: Post-processing effects\n");
	struct wlblur_blur_params params_fx = params_default;
	params_fx.brightness = 0.8f;
	params_fx.contrast = 1.2f;
	params_fx.saturation = 1.3f;
	params_fx.noise = 0.05f;
	if (!run_blur_test(renderer, 512, 512, &params_fx)) {
		goto cleanup;
	}
	printf("\n");

	/* All tests passed */
	printf("=============================\n");
	printf("All tests PASSED!\n");

	/* Cleanup */
	wlblur_kawase_destroy(renderer);
	wlblur_egl_destroy(egl_ctx);

	return 0;

cleanup:
	fprintf(stderr, "\nTest suite FAILED\n");
	wlblur_kawase_destroy(renderer);
	wlblur_egl_destroy(egl_ctx);
	return 1;
}
