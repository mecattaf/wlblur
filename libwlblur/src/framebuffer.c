/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * framebuffer.c - Framebuffer object pooling
 */

#include "../private/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wlblur_fbo* wlblur_fbo_create(int width, int height) {
	if (width <= 0 || height <= 0) {
		fprintf(stderr, "[wlblur] Invalid FBO dimensions: %dx%d\n",
		        width, height);
		return NULL;
	}

	struct wlblur_fbo *fbo = calloc(1, sizeof(*fbo));
	if (!fbo) {
		fprintf(stderr, "[wlblur] Failed to allocate FBO\n");
		return NULL;
	}

	fbo->width = width;
	fbo->height = height;
	fbo->in_use = false;

	/* Create texture */
	glGenTextures(1, &fbo->texture);
	glBindTexture(GL_TEXTURE_2D, fbo->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
	             width, height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Create framebuffer */
	glGenFramebuffers(1, &fbo->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                      GL_TEXTURE_2D, fbo->texture, 0);

	/* Validate framebuffer */
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[wlblur] FBO incomplete: 0x%x\n", status);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		wlblur_fbo_destroy(fbo);
		return NULL;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* Check for GL errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error during FBO creation: 0x%x\n", error);
		wlblur_fbo_destroy(fbo);
		return NULL;
	}

	return fbo;
}

void wlblur_fbo_destroy(struct wlblur_fbo *fbo) {
	if (!fbo) {
		return;
	}

	if (fbo->fbo) {
		glDeleteFramebuffers(1, &fbo->fbo);
	}
	if (fbo->texture) {
		glDeleteTextures(1, &fbo->texture);
	}

	free(fbo);
}

void wlblur_fbo_bind(struct wlblur_fbo *fbo) {
	if (!fbo) {
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
}

void wlblur_fbo_unbind(void) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

struct wlblur_fbo_pool* wlblur_fbo_pool_create(void) {
	struct wlblur_fbo_pool *pool = calloc(1, sizeof(*pool));
	if (!pool) {
		fprintf(stderr, "[wlblur] Failed to allocate FBO pool\n");
		return NULL;
	}

	pool->count = 0;
	memset(pool->fbos, 0, sizeof(pool->fbos));

	return pool;
}

void wlblur_fbo_pool_destroy(struct wlblur_fbo_pool *pool) {
	if (!pool) {
		return;
	}

	/* Destroy all FBOs in pool */
	for (int i = 0; i < pool->count; i++) {
		wlblur_fbo_destroy(pool->fbos[i]);
	}

	free(pool);
}

struct wlblur_fbo* wlblur_fbo_pool_acquire(
	struct wlblur_fbo_pool *pool,
	int width,
	int height
) {
	if (!pool) {
		return NULL;
	}

	/* Try to find existing FBO with matching dimensions */
	for (int i = 0; i < pool->count; i++) {
		struct wlblur_fbo *fbo = pool->fbos[i];
		if (fbo && !fbo->in_use &&
		    fbo->width == width && fbo->height == height) {
			fbo->in_use = true;
			return fbo;
		}
	}

	/* No matching FBO found, create new one */
	if (pool->count >= WLBLUR_FBO_POOL_SIZE) {
		fprintf(stderr, "[wlblur] FBO pool exhausted (max %d)\n",
		        WLBLUR_FBO_POOL_SIZE);
		return NULL;
	}

	struct wlblur_fbo *fbo = wlblur_fbo_create(width, height);
	if (!fbo) {
		return NULL;
	}

	fbo->in_use = true;
	pool->fbos[pool->count++] = fbo;

	return fbo;
}

void wlblur_fbo_pool_release(
	struct wlblur_fbo_pool *pool,
	struct wlblur_fbo *fbo
) {
	if (!pool || !fbo) {
		return;
	}

	/* Mark FBO as not in use */
	fbo->in_use = false;
}
