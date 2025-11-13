/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * shaders.c - Shader compilation and management
 */

#include "../private/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default fullscreen quad vertex shader */
static const char *DEFAULT_VERTEX_SHADER =
	"#version 300 es\n"
	"precision mediump float;\n"
	"\n"
	"in vec2 position;\n"
	"out vec2 v_texcoord;\n"
	"\n"
	"void main() {\n"
	"    v_texcoord = position * 0.5 + 0.5;\n"
	"    gl_Position = vec4(position, 0.0, 1.0);\n"
	"}\n";

/**
 * Read shader source from file
 */
static char* read_shader_file(const char *path) {
	FILE *file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "[wlblur] Failed to open shader: %s\n", path);
		return NULL;
	}

	/* Get file size */
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= 0) {
		fprintf(stderr, "[wlblur] Invalid shader file size: %s\n", path);
		fclose(file);
		return NULL;
	}

	/* Allocate buffer */
	char *source = malloc(size + 1);
	if (!source) {
		fprintf(stderr, "[wlblur] Failed to allocate shader buffer\n");
		fclose(file);
		return NULL;
	}

	/* Read file */
	size_t read_size = fread(source, 1, size, file);
	source[read_size] = '\0';
	fclose(file);

	return source;
}

/**
 * Compile shader
 */
static GLuint compile_shader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
		fprintf(stderr, "[wlblur] Failed to create shader\n");
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	/* Check compilation status */
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		GLint log_length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

		char *log = malloc(log_length);
		if (log) {
			glGetShaderInfoLog(shader, log_length, NULL, log);
			fprintf(stderr, "[wlblur] Shader compilation failed:\n%s\n", log);
			free(log);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

/**
 * Link shader program
 */
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
	GLuint program = glCreateProgram();
	if (!program) {
		fprintf(stderr, "[wlblur] Failed to create shader program\n");
		return 0;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	/* Check link status */
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint log_length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

		char *log = malloc(log_length);
		if (log) {
			glGetProgramInfoLog(program, log_length, NULL, log);
			fprintf(stderr, "[wlblur] Program linking failed:\n%s\n", log);
			free(log);
		}

		glDeleteProgram(program);
		return 0;
	}

	return program;
}

struct wlblur_shader_program* wlblur_shader_load_from_source(
	const char *vertex_source,
	const char *fragment_source
) {
	if (!fragment_source) {
		fprintf(stderr, "[wlblur] Fragment shader source required\n");
		return NULL;
	}

	/* Use default vertex shader if not provided */
	if (!vertex_source) {
		vertex_source = DEFAULT_VERTEX_SHADER;
	}

	struct wlblur_shader_program *shader = calloc(1, sizeof(*shader));
	if (!shader) {
		fprintf(stderr, "[wlblur] Failed to allocate shader program\n");
		return NULL;
	}

	/* Compile shaders */
	shader->vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
	if (!shader->vertex_shader) {
		free(shader);
		return NULL;
	}

	shader->fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
	if (!shader->fragment_shader) {
		glDeleteShader(shader->vertex_shader);
		free(shader);
		return NULL;
	}

	/* Link program */
	shader->program = link_program(shader->vertex_shader, shader->fragment_shader);
	if (!shader->program) {
		glDeleteShader(shader->vertex_shader);
		glDeleteShader(shader->fragment_shader);
		free(shader);
		return NULL;
	}

	/* Query uniform locations */
	shader->u_tex = glGetUniformLocation(shader->program, "tex");
	shader->u_halfpixel = glGetUniformLocation(shader->program, "halfpixel");
	shader->u_radius = glGetUniformLocation(shader->program, "radius");
	shader->u_brightness = glGetUniformLocation(shader->program, "brightness");
	shader->u_contrast = glGetUniformLocation(shader->program, "contrast");
	shader->u_saturation = glGetUniformLocation(shader->program, "saturation");
	shader->u_noise = glGetUniformLocation(shader->program, "noise");

	/* Validate program */
	glValidateProgram(shader->program);
	GLint status;
	glGetProgramiv(shader->program, GL_VALIDATE_STATUS, &status);
	if (status != GL_TRUE) {
		fprintf(stderr, "[wlblur] Program validation failed\n");
		/* Don't fail on validation error - some drivers are strict */
	}

	return shader;
}

struct wlblur_shader_program* wlblur_shader_load(
	const char *vertex_source,
	const char *fragment_path
) {
	if (!fragment_path) {
		fprintf(stderr, "[wlblur] Fragment shader path required\n");
		return NULL;
	}

	/* Try multiple search paths for shader files */
	const char *search_paths[] = {
		fragment_path,  /* Absolute path or relative to CWD */
		NULL,
	};

	/* Check WLBLUR_SHADER_PATH environment variable */
	char env_path[512];
	const char *shader_dir = getenv("WLBLUR_SHADER_PATH");
	if (shader_dir) {
		snprintf(env_path, sizeof(env_path), "%s/%s", shader_dir, fragment_path);
		search_paths[0] = env_path;
	}

	/* Try to read shader file */
	char *fragment_source = NULL;
	for (int i = 0; search_paths[i] != NULL; i++) {
		fragment_source = read_shader_file(search_paths[i]);
		if (fragment_source) {
			break;
		}
	}

	if (!fragment_source) {
		fprintf(stderr, "[wlblur] Failed to load fragment shader: %s\n",
		        fragment_path);
		return NULL;
	}

	/* Load shader from source */
	struct wlblur_shader_program *shader =
		wlblur_shader_load_from_source(vertex_source, fragment_source);

	free(fragment_source);
	return shader;
}

void wlblur_shader_destroy(struct wlblur_shader_program *shader) {
	if (!shader) {
		return;
	}

	if (shader->program) {
		glDeleteProgram(shader->program);
	}
	if (shader->vertex_shader) {
		glDeleteShader(shader->vertex_shader);
	}
	if (shader->fragment_shader) {
		glDeleteShader(shader->fragment_shader);
	}

	free(shader);
}

bool wlblur_shader_use(struct wlblur_shader_program *shader) {
	if (!shader || !shader->program) {
		return false;
	}

	glUseProgram(shader->program);

	/* Check for GL errors */
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fprintf(stderr, "[wlblur] GL error when using shader: 0x%x\n", error);
		return false;
	}

	return true;
}
