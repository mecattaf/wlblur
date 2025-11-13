/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * internal.h - Private API and data structures
 */

#ifndef WLBLUR_INTERNAL_H
#define WLBLUR_INTERNAL_H

#include "wlblur/wlblur.h"
#include "wlblur/blur_params.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <stdbool.h>

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
 */
struct wlblur_egl_context* wlblur_egl_create(void);

/**
 * Destroy EGL context and clean up resources
 */
void wlblur_egl_destroy(struct wlblur_egl_context *ctx);

/**
 * Make EGL context current for the calling thread
 */
bool wlblur_egl_make_current(struct wlblur_egl_context *ctx);

/**
 * Shader program management
 */
struct wlblur_shader_program {
	GLuint program;
	GLuint vertex_shader;
	GLuint fragment_shader;

	/* Uniform locations */
	GLint u_tex;
	GLint u_halfpixel;
	GLint u_radius;

	/* Post-processing uniforms */
	GLint u_brightness;
	GLint u_contrast;
	GLint u_saturation;
	GLint u_noise;
};

/**
 * Load and compile shader from file
 *
 * Shader files expected in: /usr/share/wlblur/shaders/
 * or WLBLUR_SHADER_PATH environment variable
 *
 * @param vertex_source Vertex shader source code (if NULL, uses default)
 * @param fragment_path Path to fragment shader file
 * @return Shader program or NULL on failure
 */
struct wlblur_shader_program* wlblur_shader_load(
	const char *vertex_source,
	const char *fragment_path
);

/**
 * Load shader from source strings
 *
 * @param vertex_source Vertex shader source
 * @param fragment_source Fragment shader source
 * @return Shader program or NULL on failure
 */
struct wlblur_shader_program* wlblur_shader_load_from_source(
	const char *vertex_source,
	const char *fragment_source
);

/**
 * Destroy shader program
 */
void wlblur_shader_destroy(struct wlblur_shader_program *shader);

/**
 * Use shader program
 */
bool wlblur_shader_use(struct wlblur_shader_program *shader);

/**
 * Framebuffer object for render-to-texture
 */
struct wlblur_fbo {
	GLuint fbo;
	GLuint texture;
	int width;
	int height;
	bool in_use;
};

/**
 * Create framebuffer with texture attachment
 */
struct wlblur_fbo* wlblur_fbo_create(int width, int height);

/**
 * Destroy framebuffer
 */
void wlblur_fbo_destroy(struct wlblur_fbo *fbo);

/**
 * Bind framebuffer for rendering
 */
void wlblur_fbo_bind(struct wlblur_fbo *fbo);

/**
 * Unbind framebuffer (bind default framebuffer)
 */
void wlblur_fbo_unbind(void);

/**
 * Framebuffer pool for reuse (optimization)
 */
#define WLBLUR_FBO_POOL_SIZE 16

struct wlblur_fbo_pool {
	struct wlblur_fbo *fbos[WLBLUR_FBO_POOL_SIZE];
	int count;
};

/**
 * Create FBO pool
 */
struct wlblur_fbo_pool* wlblur_fbo_pool_create(void);

/**
 * Destroy FBO pool
 */
void wlblur_fbo_pool_destroy(struct wlblur_fbo_pool *pool);

/**
 * Acquire FBO from pool (creates if needed)
 */
struct wlblur_fbo* wlblur_fbo_pool_acquire(
	struct wlblur_fbo_pool *pool,
	int width,
	int height
);

/**
 * Release FBO back to pool
 */
void wlblur_fbo_pool_release(
	struct wlblur_fbo_pool *pool,
	struct wlblur_fbo *fbo
);

/**
 * Kawase blur renderer state
 */
struct wlblur_kawase_renderer {
	struct wlblur_egl_context *egl_ctx;
	struct wlblur_fbo_pool *fbo_pool;

	/* Shaders */
	struct wlblur_shader_program *downsample_shader;
	struct wlblur_shader_program *upsample_shader;
	struct wlblur_shader_program *finish_shader;

	/* Geometry (fullscreen quad) */
	GLuint vao;
	GLuint vbo;
};

/**
 * Create Kawase blur renderer
 */
struct wlblur_kawase_renderer* wlblur_kawase_create(
	struct wlblur_egl_context *egl_ctx
);

/**
 * Destroy Kawase blur renderer
 */
void wlblur_kawase_destroy(struct wlblur_kawase_renderer *renderer);

/**
 * Apply Dual Kawase blur to texture
 *
 * @param renderer Blur renderer
 * @param input_texture GL texture to blur
 * @param width Texture width
 * @param height Texture height
 * @param params Blur parameters
 * @return Blurred texture (managed by FBO pool, do not delete)
 */
GLuint wlblur_kawase_blur(
	struct wlblur_kawase_renderer *renderer,
	GLuint input_texture,
	int width,
	int height,
	const struct wlblur_blur_params *params
);

#endif /* WLBLUR_INTERNAL_H */
