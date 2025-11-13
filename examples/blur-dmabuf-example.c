/**
 * Complete example of using libwlblur API
 *
 * Demonstrates:
 * - Context creation
 * - DMA-BUF-based blur
 * - Error handling
 * - Cleanup
 */

#include <wlblur/wlblur.h>
#include <wlblur/blur_params.h>
#include <wlblur/dmabuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES3/gl3.h>
#include <drm_fourcc.h>

#define TEST_WIDTH 1920
#define TEST_HEIGHT 1080

/**
 * Create a test DMA-BUF texture (simulates compositor backdrop)
 *
 * In real usage, this would come from wlr_buffer_get_dmabuf() or similar.
 */
static struct wlblur_dmabuf_attribs create_test_dmabuf(
	struct wlblur_egl_context *egl_ctx,
	int width,
	int height
) {
	struct wlblur_dmabuf_attribs attribs = {0};

	/* Create a gradient texture for testing */
	uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
	if (!pixels) {
		fprintf(stderr, "Failed to allocate pixel buffer\n");
		return attribs;
	}

	/* Generate gradient pattern */
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t r = (x * 255) / width;
			uint8_t g = (y * 255) / height;
			uint8_t b = 128;
			pixels[y * width + x] = 0xFF000000 | (b << 16) | (g << 8) | r;
		}
	}

	/* Create GL texture */
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(pixels);

	/* Export as DMA-BUF */
	attribs.width = width;
	attribs.height = height;
	if (!wlblur_dmabuf_export(egl_ctx, texture, width, height, &attribs)) {
		fprintf(stderr, "Failed to export test texture as DMA-BUF\n");
		glDeleteTextures(1, &texture);
		return attribs;
	}

	glDeleteTextures(1, &texture);
	return attribs;
}

int main() {
	printf("=== libwlblur Complete Example ===\n\n");

	/* Check version */
	if (!wlblur_check_version(0, 1)) {
		fprintf(stderr, "libwlblur version too old, need 0.1+\n");
		return 1;
	}

	struct wlblur_version ver = wlblur_version();
	printf("Using libwlblur %s\n", ver.string);
	printf("  Version: %d.%d.%d\n\n", ver.major, ver.minor, ver.patch);

	/* Create context */
	printf("Creating blur context...\n");
	struct wlblur_context *ctx = wlblur_context_create();
	if (!ctx) {
		fprintf(stderr, "Failed to create context: %s\n",
		        wlblur_error_string(wlblur_get_error()));
		return 1;
	}
	printf("  ✓ Context created successfully\n\n");

	/* Simulate getting DMA-BUF from compositor */
	/* In real usage, this comes from wlr_buffer_get_dmabuf() */
	printf("Creating test DMA-BUF (%dx%d)...\n", TEST_WIDTH, TEST_HEIGHT);

	/* We need direct EGL access for test setup - in real usage,
	 * the compositor provides the DMA-BUF directly */
	extern struct wlblur_egl_context* wlblur_egl_create(void);
	extern void wlblur_egl_destroy(struct wlblur_egl_context *ctx);
	extern bool wlblur_egl_make_current(struct wlblur_egl_context *ctx);

	struct wlblur_egl_context *test_egl = wlblur_egl_create();
	if (!test_egl) {
		fprintf(stderr, "Failed to create test EGL context\n");
		wlblur_context_destroy(ctx);
		return 1;
	}
	wlblur_egl_make_current(test_egl);

	struct wlblur_dmabuf_attribs input = create_test_dmabuf(
		test_egl, TEST_WIDTH, TEST_HEIGHT
	);
	if (input.planes[0].fd == 0) {
		fprintf(stderr, "Failed to create test DMA-BUF\n");
		wlblur_egl_destroy(test_egl);
		wlblur_context_destroy(ctx);
		return 1;
	}
	printf("  ✓ Test DMA-BUF created (fd=%d)\n\n", input.planes[0].fd);

	/* Configure blur parameters */
	struct wlblur_blur_params params = wlblur_params_default();
	printf("Blur configuration:\n");
	printf("  Passes:     %d\n", params.num_passes);
	printf("  Radius:     %.1f\n", params.radius);
	printf("  Brightness: %.2f\n", params.brightness);
	printf("  Contrast:   %.2f\n", params.contrast);
	printf("  Saturation: %.2f\n", params.saturation);
	printf("  Noise:      %.3f\n\n", params.noise);

	/* Apply blur */
	printf("Applying blur...\n");
	struct wlblur_dmabuf_attribs output;
	if (!wlblur_apply_blur(ctx, &input, &params, &output)) {
		fprintf(stderr, "Blur failed: %s\n",
		        wlblur_error_string(wlblur_get_error()));
		wlblur_dmabuf_close(&input);
		wlblur_egl_destroy(test_egl);
		wlblur_context_destroy(ctx);
		return 1;
	}

	printf("  ✓ Blur applied successfully\n");
	printf("  Output: %dx%d format=0x%x fd=%d\n\n",
	       output.width, output.height, output.format,
	       output.planes[0].fd);

	/* Compositor would now use output FD for rendering */
	/* For example:
	 *   wlr_texture_from_dmabuf(renderer, &output);
	 *   wlr_render_texture(renderer, texture, ...);
	 */
	printf("Note: In a real compositor, you would now:\n");
	printf("  1. Import output.planes[0].fd as a texture\n");
	printf("  2. Composite the blurred texture into your scene\n");
	printf("  3. Call wlblur_dmabuf_close() when done\n\n");

	/* Cleanup */
	printf("Cleaning up...\n");
	wlblur_dmabuf_close(&output);
	wlblur_dmabuf_close(&input);
	wlblur_egl_destroy(test_egl);
	wlblur_context_destroy(ctx);

	printf("  ✓ Complete example passed\n\n");
	printf("=== Success ===\n");
	return 0;
}
