/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur_kawase.c - Dual Kawase blur algorithm implementation
 */

#include "../private/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fullscreen quad vertices: two triangles covering [-1, 1] */
static const float QUAD_VERTICES[] = {
	/* Position */
	-1.0f, -1.0f,
	 1.0f, -1.0f,
	-1.0f,  1.0f,
	 1.0f,  1.0f,
};

/**
 * Create fullscreen quad geometry
 */
static bool create_fullscreen_quad(GLuint *vao, GLuint *vbo) {
	/* Generate VAO */
	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);

	/* Generate VBO */
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES),
	             QUAD_VERTICES, GL_STATIC_DRAW);

	/* Setup vertex attributes */
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	/* Unbind */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	/* Check for errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error creating quad: 0x%x\n", error);
		return false;
	}

	return true;
}

/**
 * Render fullscreen quad
 */
static void render_fullscreen_quad(struct wlblur_kawase_renderer *renderer) {
	glBindVertexArray(renderer->vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

/**
 * Load shader from file with embedded shader directory path
 */
static struct wlblur_shader_program* load_shader_from_relative(
	const char *relative_path
) {
	/* Try to load from shader directory */
	char full_path[512];

	/* First try: relative to current directory (for development) */
	snprintf(full_path, sizeof(full_path), "libwlblur/shaders/%s", relative_path);
	char *source = NULL;
	FILE *file = fopen(full_path, "r");

	if (!file) {
		/* Second try: absolute path or WLBLUR_SHADER_PATH */
		const char *shader_dir = getenv("WLBLUR_SHADER_PATH");
		if (shader_dir) {
			snprintf(full_path, sizeof(full_path), "%s/%s", shader_dir, relative_path);
			file = fopen(full_path, "r");
		}
	}

	if (!file) {
		/* Third try: installed location */
		snprintf(full_path, sizeof(full_path), "/usr/share/wlblur/shaders/%s", relative_path);
		file = fopen(full_path, "r");
	}

	if (!file) {
		fprintf(stderr, "[wlblur] Failed to open shader: %s\n", relative_path);
		return NULL;
	}

	/* Read file */
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	source = malloc(size + 1);
	if (!source) {
		fclose(file);
		return NULL;
	}

	size_t read_size = fread(source, 1, size, file);
	source[read_size] = '\0';
	fclose(file);

	/* Compile shader */
	struct wlblur_shader_program *shader =
		wlblur_shader_load_from_source(NULL, source);

	free(source);
	return shader;
}

struct wlblur_kawase_renderer* wlblur_kawase_create(
	struct wlblur_egl_context *egl_ctx
) {
	if (!egl_ctx) {
		fprintf(stderr, "[wlblur] EGL context required\n");
		return NULL;
	}

	struct wlblur_kawase_renderer *renderer = calloc(1, sizeof(*renderer));
	if (!renderer) {
		fprintf(stderr, "[wlblur] Failed to allocate renderer\n");
		return NULL;
	}

	renderer->egl_ctx = egl_ctx;

	/* Make context current */
	if (!wlblur_egl_make_current(egl_ctx)) {
		fprintf(stderr, "[wlblur] Failed to make context current\n");
		free(renderer);
		return NULL;
	}

	/* Create FBO pool */
	renderer->fbo_pool = wlblur_fbo_pool_create();
	if (!renderer->fbo_pool) {
		free(renderer);
		return NULL;
	}

	/* Load shaders */
	renderer->downsample_shader = load_shader_from_relative("kawase_downsample.frag.glsl");
	if (!renderer->downsample_shader) {
		fprintf(stderr, "[wlblur] Failed to load downsample shader\n");
		goto error;
	}

	renderer->upsample_shader = load_shader_from_relative("kawase_upsample.frag.glsl");
	if (!renderer->upsample_shader) {
		fprintf(stderr, "[wlblur] Failed to load upsample shader\n");
		goto error;
	}

	renderer->finish_shader = load_shader_from_relative("blur_finish.frag.glsl");
	if (!renderer->finish_shader) {
		fprintf(stderr, "[wlblur] Failed to load finish shader\n");
		goto error;
	}

	/* Create fullscreen quad */
	if (!create_fullscreen_quad(&renderer->vao, &renderer->vbo)) {
		fprintf(stderr, "[wlblur] Failed to create fullscreen quad\n");
		goto error;
	}

	fprintf(stderr, "[wlblur] Kawase renderer created successfully\n");
	return renderer;

error:
	wlblur_kawase_destroy(renderer);
	return NULL;
}

void wlblur_kawase_destroy(struct wlblur_kawase_renderer *renderer) {
	if (!renderer) {
		return;
	}

	/* Destroy shaders */
	if (renderer->downsample_shader) {
		wlblur_shader_destroy(renderer->downsample_shader);
	}
	if (renderer->upsample_shader) {
		wlblur_shader_destroy(renderer->upsample_shader);
	}
	if (renderer->finish_shader) {
		wlblur_shader_destroy(renderer->finish_shader);
	}

	/* Destroy geometry */
	if (renderer->vao) {
		glDeleteVertexArrays(1, &renderer->vao);
	}
	if (renderer->vbo) {
		glDeleteBuffers(1, &renderer->vbo);
	}

	/* Destroy FBO pool */
	if (renderer->fbo_pool) {
		wlblur_fbo_pool_destroy(renderer->fbo_pool);
	}

	free(renderer);
}

GLuint wlblur_kawase_blur(
	struct wlblur_kawase_renderer *renderer,
	GLuint input_texture,
	int width,
	int height,
	const struct wlblur_blur_params *params
) {
	if (!renderer || !input_texture || width <= 0 || height <= 0) {
		fprintf(stderr, "[wlblur] Invalid blur parameters\n");
		return 0;
	}

	/* Validate parameters */
	if (!wlblur_params_validate(params)) {
		fprintf(stderr, "[wlblur] Invalid blur params\n");
		return 0;
	}

	int num_passes = params->num_passes;
	if (num_passes < 1 || num_passes > 8) {
		fprintf(stderr, "[wlblur] Invalid number of passes: %d\n", num_passes);
		return 0;
	}

	/* Allocate FBOs for each resolution level */
	struct wlblur_fbo *fbos[8];  /* Max 8 passes */
	for (int i = 0; i < num_passes; i++) {
		int fbo_width = width >> (i + 1);   /* Divide by 2^(i+1) */
		int fbo_height = height >> (i + 1);

		/* Ensure minimum size of 1x1 */
		if (fbo_width < 1) fbo_width = 1;
		if (fbo_height < 1) fbo_height = 1;

		fbos[i] = wlblur_fbo_pool_acquire(renderer->fbo_pool,
		                                  fbo_width, fbo_height);
		if (!fbos[i]) {
			fprintf(stderr, "[wlblur] Failed to acquire FBO for pass %d\n", i);
			/* Release acquired FBOs */
			for (int j = 0; j < i; j++) {
				wlblur_fbo_pool_release(renderer->fbo_pool, fbos[j]);
			}
			return 0;
		}
	}

	GLuint current_tex = input_texture;

	/* === DOWNSAMPLE PASSES === */
	wlblur_shader_use(renderer->downsample_shader);

	for (int pass = 0; pass < num_passes; pass++) {
		struct wlblur_fbo *target_fbo = fbos[pass];

		/* Bind target framebuffer */
		wlblur_fbo_bind(target_fbo);
		glViewport(0, 0, target_fbo->width, target_fbo->height);

		/* Set uniforms */
		glUniform1i(renderer->downsample_shader->u_tex, 0);
		glUniform2f(renderer->downsample_shader->u_halfpixel,
		            0.5f / target_fbo->width,
		            0.5f / target_fbo->height);
		glUniform1f(renderer->downsample_shader->u_radius,
		            params->radius + (float)pass);

		/* Bind input texture */
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, current_tex);

		/* Draw fullscreen quad */
		render_fullscreen_quad(renderer);

		/* Output becomes input for next pass */
		current_tex = target_fbo->texture;
	}

	/* === UPSAMPLE PASSES === */
	wlblur_shader_use(renderer->upsample_shader);

	for (int pass = num_passes - 1; pass >= 0; pass--) {
		struct wlblur_fbo *target_fbo;

		if (pass == 0) {
			/* Final upsample pass: render to full resolution */
			target_fbo = wlblur_fbo_pool_acquire(renderer->fbo_pool,
			                                     width, height);
		} else {
			/* Intermediate pass: render to previous level */
			target_fbo = fbos[pass - 1];
		}

		if (!target_fbo) {
			fprintf(stderr, "[wlblur] Failed to acquire target FBO for upsample\n");
			/* Release FBOs */
			for (int i = 0; i < num_passes; i++) {
				wlblur_fbo_pool_release(renderer->fbo_pool, fbos[i]);
			}
			return 0;
		}

		/* Bind target framebuffer */
		wlblur_fbo_bind(target_fbo);
		glViewport(0, 0, target_fbo->width, target_fbo->height);

		/* Set uniforms */
		glUniform1i(renderer->upsample_shader->u_tex, 0);
		glUniform2f(renderer->upsample_shader->u_halfpixel,
		            0.5f / target_fbo->width,
		            0.5f / target_fbo->height);
		glUniform1f(renderer->upsample_shader->u_radius,
		            params->radius + (float)pass);

		/* Bind input texture */
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, current_tex);

		/* Draw */
		render_fullscreen_quad(renderer);

		/* For the final pass, save the texture */
		if (pass == 0) {
			current_tex = target_fbo->texture;
		} else {
			current_tex = target_fbo->texture;
		}
	}

	/* === POST-PROCESSING === */
	struct wlblur_fbo *final_fbo = wlblur_fbo_pool_acquire(
		renderer->fbo_pool, width, height
	);

	if (!final_fbo) {
		fprintf(stderr, "[wlblur] Failed to acquire final FBO\n");
		/* Release FBOs */
		for (int i = 0; i < num_passes; i++) {
			wlblur_fbo_pool_release(renderer->fbo_pool, fbos[i]);
		}
		return 0;
	}

	wlblur_fbo_bind(final_fbo);
	glViewport(0, 0, width, height);

	wlblur_shader_use(renderer->finish_shader);

	/* Set effect uniforms */
	glUniform1i(renderer->finish_shader->u_tex, 0);
	glUniform1f(renderer->finish_shader->u_brightness, params->brightness);
	glUniform1f(renderer->finish_shader->u_contrast, params->contrast);
	glUniform1f(renderer->finish_shader->u_saturation, params->saturation);
	glUniform1f(renderer->finish_shader->u_noise, params->noise);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, current_tex);

	render_fullscreen_quad(renderer);

	wlblur_fbo_unbind();

	/* Release intermediate FBOs */
	for (int i = 0; i < num_passes; i++) {
		wlblur_fbo_pool_release(renderer->fbo_pool, fbos[i]);
	}

	/* Check for GL errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error during blur: 0x%x\n", error);
		wlblur_fbo_pool_release(renderer->fbo_pool, final_fbo);
		return 0;
	}

	return final_fbo->texture;
}
