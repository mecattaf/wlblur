# libwlblur API Reference

**Version:** 0.1.0
**License:** MIT
**Author:** mecattaf

## Table of Contents

- [Overview](#overview)
- [Context Management](#context-management)
- [Blur Operations](#blur-operations)
- [Error Handling](#error-handling)
- [Version Information](#version-information)
- [Data Structures](#data-structures)
- [Usage Examples](#usage-examples)
- [Performance Notes](#performance-notes)
- [Thread Safety](#thread-safety)

## Overview

libwlblur is a compositor-agnostic blur library for Wayland compositors. It provides zero-copy DMA-BUF-based blur effects using the Dual Kawase algorithm.

**Key Features:**
- Zero-copy texture sharing via DMA-BUF
- Hardware-accelerated blur (OpenGL ES 3.0)
- Dual Kawase algorithm (Apple-level quality)
- Configurable post-processing effects
- Thread-local error handling
- MIT licensed

**System Requirements:**
- OpenGL ES 3.0+
- EGL with surfaceless context support
- DMA-BUF import/export extensions:
  - `EGL_EXT_image_dma_buf_import`
  - `EGL_KHR_image_base`
  - `EGL_MESA_image_dma_buf_export`

## Context Management

### `wlblur_context_create()`

```c
struct wlblur_context* wlblur_context_create(void);
```

Creates a new blur context with all necessary resources.

**Returns:**
- Context handle on success
- `NULL` on failure (check `wlblur_get_error()`)

**Initializes:**
- EGL display and context (GLES 3.0)
- Shader programs (downsample, upsample, post-processing)
- Framebuffer object pool
- Extension function pointers

**Error Codes:**
- `WLBLUR_ERROR_OUT_OF_MEMORY` - Memory allocation failed
- `WLBLUR_ERROR_EGL_INIT` - EGL initialization failed
- `WLBLUR_ERROR_MISSING_EXTENSION` - Required extension missing
- `WLBLUR_ERROR_SHADER_COMPILE` - Shader compilation failed

**Example:**
```c
struct wlblur_context *ctx = wlblur_context_create();
if (!ctx) {
    fprintf(stderr, "Failed to create blur context: %s\n",
            wlblur_error_string(wlblur_get_error()));
    return -1;
}
```

**Performance:** ~50ms (one-time initialization cost)

### `wlblur_context_destroy()`

```c
void wlblur_context_destroy(struct wlblur_context *ctx);
```

Destroys blur context and frees all resources.

**Parameters:**
- `ctx` - Context to destroy (NULL-safe)

**Cleanup Order:**
1. Shader programs
2. Framebuffer object pool
3. EGL context
4. Context structure

**Thread Safety:** Must be called from same thread that created context

**Example:**
```c
wlblur_context_destroy(ctx);
ctx = NULL;  // Good practice
```

## Blur Operations

### `wlblur_apply_blur()`

```c
bool wlblur_apply_blur(
    struct wlblur_context *ctx,
    const struct wlblur_dmabuf_attribs *input_attribs,
    const struct wlblur_blur_params *params,
    struct wlblur_dmabuf_attribs *output_attribs
);
```

Main API function that applies blur to a DMA-BUF texture.

**Parameters:**
- `ctx` - Blur context (required)
- `input_attribs` - Input DMA-BUF attributes from compositor (required)
- `params` - Blur parameters (required)
- `output_attribs` - Output DMA-BUF attributes (filled by function)

**Returns:**
- `true` on success
- `false` on failure (check `wlblur_get_error()`)

**Processing Steps:**
1. Validate parameters
2. Import input DMA-BUF as GL texture
3. Apply Dual Kawase blur (multi-pass downsampling/upsampling)
4. Apply post-processing effects (brightness, contrast, saturation, noise)
5. Export result as DMA-BUF

**Ownership:**
- `input_attribs`: Caller retains ownership (wlblur does NOT close FDs)
- `output_attribs`: Caller owns FDs, MUST call `wlblur_dmabuf_close()` when done

**Error Codes:**
- `WLBLUR_ERROR_INVALID_PARAMS` - Invalid parameters
- `WLBLUR_ERROR_EGL_INIT` - Failed to make context current
- `WLBLUR_ERROR_DMABUF_IMPORT` - DMA-BUF import failed
- `WLBLUR_ERROR_GL_ERROR` - OpenGL error during rendering
- `WLBLUR_ERROR_DMABUF_EXPORT` - DMA-BUF export failed

**Performance:** ~1.4ms @ 1080p (3 passes, radius=5)

**Example:**
```c
struct wlblur_dmabuf_attribs input = get_backdrop_dmabuf();
struct wlblur_blur_params params = wlblur_params_default();
struct wlblur_dmabuf_attribs output;

if (!wlblur_apply_blur(ctx, &input, &params, &output)) {
    fprintf(stderr, "Blur failed: %s\n",
            wlblur_error_string(wlblur_get_error()));
    return;
}

// Use output.planes[0].fd as blurred texture
composite_blurred_texture(output.planes[0].fd);

// IMPORTANT: Close output FDs when done
wlblur_dmabuf_close(&output);
```

## Error Handling

### `wlblur_get_error()`

```c
enum wlblur_error wlblur_get_error(void);
```

Returns the last error code for the current thread.

**Returns:** Error code or `WLBLUR_ERROR_NONE`

**Thread Safety:** Thread-local storage (each thread has independent error state)

**Behavior:**
- Error state persists until next successful operation
- Automatically cleared on success
- Never cleared on subsequent failures

**Example:**
```c
if (!wlblur_context_create()) {
    enum wlblur_error err = wlblur_get_error();
    fprintf(stderr, "Error: %s (code=%d)\n",
            wlblur_error_string(err), err);
}
```

### `wlblur_error_string()`

```c
const char* wlblur_error_string(enum wlblur_error error);
```

Converts error code to human-readable string.

**Parameters:**
- `error` - Error code to convert

**Returns:** Static string (do NOT free)

**Error Codes:**
- `WLBLUR_ERROR_NONE` - "No error"
- `WLBLUR_ERROR_EGL_INIT` - "EGL initialization failed"
- `WLBLUR_ERROR_MISSING_EXTENSION` - "Required EGL extension missing (DMA-BUF support)"
- `WLBLUR_ERROR_SHADER_COMPILE` - "Shader compilation failed"
- `WLBLUR_ERROR_DMABUF_IMPORT` - "DMA-BUF import failed"
- `WLBLUR_ERROR_DMABUF_EXPORT` - "DMA-BUF export failed"
- `WLBLUR_ERROR_INVALID_PARAMS` - "Invalid parameters"
- `WLBLUR_ERROR_GL_ERROR` - "OpenGL error occurred"
- `WLBLUR_ERROR_OUT_OF_MEMORY` - "Out of memory"

**Example:**
```c
const char *msg = wlblur_error_string(WLBLUR_ERROR_EGL_INIT);
printf("Error: %s\n", msg);
```

## Version Information

### `wlblur_version()`

```c
struct wlblur_version wlblur_version(void);
```

Returns library version information.

**Returns:** Version structure (static storage, do NOT free)

**Structure:**
```c
struct wlblur_version {
    int major;         // Major version (breaking changes)
    int minor;         // Minor version (new features)
    int patch;         // Patch version (bug fixes)
    const char *string;  // e.g., "0.1.0"
};
```

**Example:**
```c
struct wlblur_version ver = wlblur_version();
printf("libwlblur %s (%d.%d.%d)\n",
       ver.string, ver.major, ver.minor, ver.patch);
```

### `wlblur_check_version()`

```c
bool wlblur_check_version(int required_major, int required_minor);
```

Checks if library version meets minimum requirements.

**Parameters:**
- `required_major` - Minimum required major version
- `required_minor` - Minimum required minor version

**Returns:**
- `true` if library version >= required version
- `false` otherwise

**Comparison Logic:**
```
library_major > required_major  => true
library_major == required_major AND library_minor >= required_minor  => true
otherwise  => false
```

**Example:**
```c
if (!wlblur_check_version(0, 1)) {
    fprintf(stderr, "libwlblur too old, need 0.1+\n");
    return -1;
}
```

## Data Structures

### `struct wlblur_context`

Opaque blur context handle. Contains:
- EGL context
- Shader programs
- Framebuffer object pool
- Internal rendering state

**Thread Safety:** One context per thread. Do NOT share across threads.

### `struct wlblur_version`

Library version information (see [Version Information](#version-information))

### `enum wlblur_error`

Error codes (see [Error Handling](#error-handling))

### Related Structures

See also:
- `struct wlblur_blur_params` - Blur parameters (blur_params.h)
- `struct wlblur_dmabuf_attribs` - DMA-BUF attributes (dmabuf.h)

## Usage Examples

### Basic Usage

```c
#include <wlblur/wlblur.h>
#include <wlblur/blur_params.h>
#include <wlblur/dmabuf.h>

// Create context once at startup
struct wlblur_context *ctx = wlblur_context_create();
if (!ctx) {
    // Handle error
}

// Per-frame: apply blur to compositor backdrop
struct wlblur_dmabuf_attribs input = get_backdrop_dmabuf();
struct wlblur_blur_params params = wlblur_params_default();
struct wlblur_dmabuf_attribs output;

if (wlblur_apply_blur(ctx, &input, &params, &output)) {
    // Use output.planes[0].fd for rendering
    composite_blurred_backdrop(output.planes[0].fd);
    wlblur_dmabuf_close(&output);
}

// Cleanup at shutdown
wlblur_context_destroy(ctx);
```

### Custom Blur Parameters

```c
struct wlblur_blur_params params = wlblur_params_default();

// Stronger blur
params.num_passes = 5;
params.radius = 10.0;

// Darker, more contrast (better readability)
params.brightness = 0.7;
params.contrast = 1.2;

// Less saturation
params.saturation = 0.8;

wlblur_apply_blur(ctx, &input, &params, &output);
```

### Error Handling Pattern

```c
struct wlblur_context *ctx = wlblur_context_create();
if (!ctx) {
    enum wlblur_error err = wlblur_get_error();

    if (err == WLBLUR_ERROR_MISSING_EXTENSION) {
        fprintf(stderr, "GPU does not support DMA-BUF export\n");
        fprintf(stderr, "Try updating your graphics drivers\n");
    } else {
        fprintf(stderr, "Blur context creation failed: %s\n",
                wlblur_error_string(err));
    }

    return -1;
}
```

## Performance Notes

### Initialization Cost

- `wlblur_context_create()`: ~50ms (one-time cost)
  - EGL initialization: ~30ms
  - Shader compilation: ~20ms

### Per-Frame Cost

Performance of `wlblur_apply_blur()` at 1080p:

| Passes | Radius | Time     | Quality      |
|--------|--------|----------|--------------|
| 1      | 5.0    | ~0.5ms   | Light blur   |
| 3      | 5.0    | ~1.4ms   | Balanced ⭐   |
| 5      | 5.0    | ~2.3ms   | Heavy blur   |
| 3      | 10.0   | ~1.6ms   | Strong blur  |

⭐ = Recommended default

### GPU Memory

- Framebuffer pool: ~32MB (reused across frames)
- Per-frame allocation: minimal (DMA-BUF metadata only)

### Optimization Tips

1. **Reuse context**: Create once, use for entire session
2. **DMA-BUF zero-copy**: No CPU readback/upload overhead
3. **Damage tracking**: Only blur damaged regions in compositor
4. **Resolution scaling**: Consider blurring at 0.5x resolution

## Thread Safety

### Thread-Local Error State

Each thread maintains independent error state:
```c
// Thread 1
wlblur_context_create();  // Fails
wlblur_get_error();       // Returns error

// Thread 2 (concurrent)
wlblur_get_error();       // Returns WLBLUR_ERROR_NONE
```

### Context Isolation

Each context is thread-local:
- Create context on thread A
- Use context ONLY on thread A
- Destroy context on thread A

**Invalid:**
```c
// Thread A
struct wlblur_context *ctx = wlblur_context_create();

// Thread B (WRONG!)
wlblur_apply_blur(ctx, ...);  // Undefined behavior
```

**Valid:**
```c
// Thread A
struct wlblur_context *ctx_a = wlblur_context_create();
wlblur_apply_blur(ctx_a, ...);

// Thread B (separate context)
struct wlblur_context *ctx_b = wlblur_context_create();
wlblur_apply_blur(ctx_b, ...);
```

## Compositor Integration

### wlroots Example

```c
#include <wlr/types/wlr_buffer.h>
#include <wlblur/wlblur.h>

// Initialization
struct wlblur_context *blur_ctx = wlblur_context_create();

// Per-frame blur
void render_blurred_background(struct wlr_buffer *buffer) {
    // Get DMA-BUF from wlroots buffer
    struct wlr_dmabuf_attributes wlr_dmabuf;
    if (!wlr_buffer_get_dmabuf(buffer, &wlr_dmabuf)) {
        return;
    }

    // Convert to wlblur format
    struct wlblur_dmabuf_attribs input = {
        .width = wlr_dmabuf.width,
        .height = wlr_dmabuf.height,
        .format = wlr_dmabuf.format,
        .modifier = wlr_dmabuf.modifier,
        .num_planes = wlr_dmabuf.n_planes,
    };
    for (int i = 0; i < input.num_planes; i++) {
        input.planes[i].fd = wlr_dmabuf.fd[i];
        input.planes[i].offset = wlr_dmabuf.offset[i];
        input.planes[i].stride = wlr_dmabuf.stride[i];
    }

    // Apply blur
    struct wlblur_blur_params params = wlblur_params_default();
    struct wlblur_dmabuf_attribs output;

    if (wlblur_apply_blur(blur_ctx, &input, &params, &output)) {
        // Render blurred texture
        struct wlr_texture *tex = wlr_texture_from_dmabuf(
            renderer, &output
        );
        wlr_render_texture(renderer, tex, ...);

        // Cleanup
        wlr_texture_destroy(tex);
        wlblur_dmabuf_close(&output);
    }
}
```

---

**For more information:**
- GitHub: https://github.com/mecattaf/wlblur
- Issues: https://github.com/mecattaf/wlblur/issues
- License: MIT
