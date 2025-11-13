---
id: task-4
title: "Implement EGL Context Management and DMA-BUF Infrastructure"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["core", "egl", "dmabuf", "infrastructure"]
milestone: "m-1"
dependencies: ["task-1", "task-3"]
---

## Description

Implement the foundational EGL context management and DMA-BUF import/export infrastructure. This is the critical GPU interop layer that enables zero-copy texture sharing between compositor and daemon.

**Key Components:**
- EGL context initialization (GLES 3.0+)
- DMA-BUF texture import (compositor → daemon)
- DMA-BUF texture export (daemon → compositor)
- Extension detection and validation
- Error handling and cleanup

## Acceptance Criteria

- [x] EGL context creates successfully with GLES 3.0
- [x] Required extensions detected: EGL_EXT_image_dma_buf_import, EGL_MESA_image_dma_buf_export
- [x] DMA-BUF import creates GL texture from FD
- [x] DMA-BUF export creates FD from GL texture
- [x] Format negotiation (DRM_FORMAT_ARGB8888, XRGB8888)
- [x] Error handling for all GL/EGL operations
- [x] Test program validates import/export roundtrip
- [x] No GL errors logged (glGetError() clean)
- [x] FD lifecycle managed correctly (no leaks)

## Implementation Plan

### Phase 0: Study Reference Implementations

```bash
cd /tmp
# Clone if not already done
git clone https://github.com/wlrfx/scenefx
git clone https://github.com/hyprwm/Hyprland

# Study EGL patterns
cat scenefx/render/egl.c | grep -A 50 "eglCreateImageKHR"
cat scenefx/render/fx_renderer/fx_texture.c | grep -A 30 "EGL_LINUX_DMA_BUF"

# Study DMA-BUF import
cat scenefx/render/fx_renderer/fx_texture.c:353-404

# Study format handling
cat scenefx/render/egl.c:751-841
```

**Key learnings to extract:**
- EGL device selection
- Context attributes (GLES version, debug context)
- Extension string parsing
- DMA-BUF attribute structures
- FD passing patterns

### Phase 1: EGL Context Management

**File**: `libwlblur/src/egl_helpers.c`

**Implement:**

```c
/**
 * Create EGL display and context for offscreen rendering
 * 
 * Requirements:
 * - GLES 3.0 minimum
 * - Surfaceless context (EGL_KHR_surfaceless_context)
 * - DMA-BUF import/export extensions
 * 
 * Returns: EGL context handle or NULL on failure
 */
struct wlblur_egl_context {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    bool has_dmabuf_import;
    bool has_dmabuf_export;
};

struct wlblur_egl_context* wlblur_egl_create(void);
void wlblur_egl_destroy(struct wlblur_egl_context *ctx);
bool wlblur_egl_make_current(struct wlblur_egl_context *ctx);
```

**EGL Initialization Steps:**
1. Get default display: `eglGetDisplay(EGL_DEFAULT_DISPLAY)`
2. Initialize: `eglInitialize()`
3. Check extensions (critical):
   - `EGL_KHR_image_base`
   - `EGL_EXT_image_dma_buf_import`
   - `EGL_MESA_image_dma_buf_export`
   - `EGL_KHR_surfaceless_context`
4. Choose config (GLES 3.0, RGBA8, no surface)
5. Create context with attributes:
   ```c
   EGLint context_attribs[] = {
       EGL_CONTEXT_MAJOR_VERSION, 3,
       EGL_CONTEXT_MINOR_VERSION, 0,
       EGL_NONE,
   };
   ```
6. Make current: `eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)`

**Error Handling:**
- Check `eglGetError()` after every EGL call
- Log clear error messages with function names
- Cleanup partial initialization on failure

### Phase 2: DMA-BUF Import Implementation

**File**: `libwlblur/src/dmabuf.c`

**Header**: `libwlblur/include/wlblur/dmabuf.h`

```c
/**
 * DMA-BUF texture attributes (from compositor)
 */
struct wlblur_dmabuf_attribs {
    int width;
    int height;
    uint32_t format;      // DRM_FORMAT_* (drm_fourcc.h)
    uint64_t modifier;    // DRM_FORMAT_MOD_* (usually LINEAR)
    int num_planes;       // 1 for ARGB8888
    struct {
        int fd;           // File descriptor
        uint32_t offset;  // Byte offset in buffer
        uint32_t stride;  // Bytes per row
    } planes[4];
};

/**
 * Import DMA-BUF as OpenGL texture
 * 
 * Steps:
 * 1. Create EGLImage from DMA-BUF FD + attributes
 * 2. Create GL texture
 * 3. Bind EGLImage to texture
 * 
 * Returns: GL texture ID or 0 on failure
 */
GLuint wlblur_dmabuf_import(
    struct wlblur_egl_context *ctx,
    const struct wlblur_dmabuf_attribs *attribs
);

/**
 * Export GL texture as DMA-BUF
 * 
 * Steps:
 * 1. Create EGLImage from GL texture
 * 2. Export EGLImage as DMA-BUF
 * 3. Return FD + attributes
 * 
 * Returns: true on success, fills attribs with FD
 */
bool wlblur_dmabuf_export(
    struct wlblur_egl_context *ctx,
    GLuint texture,
    struct wlblur_dmabuf_attribs *attribs
);

/**
 * Close DMA-BUF file descriptors
 */
void wlblur_dmabuf_close(struct wlblur_dmabuf_attribs *attribs);
```

**Import Implementation Pattern** (from SceneFX):

```c
GLuint wlblur_dmabuf_import(
    struct wlblur_egl_context *ctx,
    const struct wlblur_dmabuf_attribs *attribs
) {
    // Build EGL attribute list
    EGLint egl_attribs[50];
    int i = 0;
    
    egl_attribs[i++] = EGL_WIDTH;
    egl_attribs[i++] = attribs->width;
    egl_attribs[i++] = EGL_HEIGHT;
    egl_attribs[i++] = attribs->height;
    egl_attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    egl_attribs[i++] = attribs->format;
    
    // Add plane 0 (primary plane)
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
    
    egl_attribs[i++] = EGL_NONE;
    
    // Create EGLImage from DMA-BUF
    EGLImageKHR image = eglCreateImageKHR(
        ctx->display,
        EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT,
        NULL,
        egl_attribs
    );
    
    if (image == EGL_NO_IMAGE_KHR) {
        // Log error
        return 0;
    }
    
    // Create GL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Import EGLImage as texture
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Cleanup EGLImage (texture retains reference)
    eglDestroyImageKHR(ctx->display, image);
    
    return texture;
}
```

**Export Implementation Pattern:**

```c
bool wlblur_dmabuf_export(
    struct wlblur_egl_context *ctx,
    GLuint texture,
    struct wlblur_dmabuf_attribs *attribs
) {
    // Create EGLImage from GL texture
    EGLImageKHR image = eglCreateImageKHR(
        ctx->display,
        ctx->context,
        EGL_GL_TEXTURE_2D,
        (EGLClientBuffer)(uintptr_t)texture,
        NULL
    );
    
    if (image == EGL_NO_IMAGE_KHR) {
        return false;
    }
    
    // Export as DMA-BUF
    int fourcc;
    int num_planes;
    uint64_t modifier;
    
    if (!eglExportDMABUFImageQueryMESA(
            ctx->display, image,
            &fourcc, &num_planes, &modifier)) {
        eglDestroyImageKHR(ctx->display, image);
        return false;
    }
    
    attribs->format = fourcc;
    attribs->modifier = modifier;
    attribs->num_planes = num_planes;
    
    // Get plane attributes
    int fds[4];
    EGLint strides[4];
    EGLint offsets[4];
    
    if (!eglExportDMABUFImageMESA(
            ctx->display, image,
            fds, strides, offsets)) {
        eglDestroyImageKHR(ctx->display, image);
        return false;
    }
    
    // Fill plane info
    for (int i = 0; i < num_planes; i++) {
        attribs->planes[i].fd = fds[i];
        attribs->planes[i].stride = strides[i];
        attribs->planes[i].offset = offsets[i];
    }
    
    eglDestroyImageKHR(ctx->display, image);
    return true;
}
```

### Phase 3: Format Negotiation

**Supported Formats** (start simple):
- `DRM_FORMAT_ARGB8888` (0x34325241) - ARGB with alpha
- `DRM_FORMAT_XRGB8888` (0x34325258) - RGB opaque

**Modifier Support:**
- `DRM_FORMAT_MOD_LINEAR` (0) - Linear layout (required)
- Handle `DRM_FORMAT_MOD_INVALID` gracefully

**Future formats** (Phase 3+):
- `DRM_FORMAT_ABGR8888` (byte order variants)
- `DRM_FORMAT_ARGB2101010` (10-bit)
- Tiled formats (Intel Y-tiled, AMD DCC)

### Phase 4: Test Program

**File**: `examples/test-dmabuf.c`

```c
/**
 * Test DMA-BUF import/export roundtrip
 * 
 * 1. Create EGL context
 * 2. Create test texture (checkerboard pattern)
 * 3. Export as DMA-BUF
 * 4. Import from DMA-BUF
 * 5. Verify pixel data matches
 */
int main() {
    // Create EGL context
    struct wlblur_egl_context *ctx = wlblur_egl_create();
    assert(ctx != NULL);
    
    // Create test texture (256x256 checkerboard)
    GLuint source_tex = create_checkerboard_texture(256, 256);
    
    // Export as DMA-BUF
    struct wlblur_dmabuf_attribs attribs;
    bool exported = wlblur_dmabuf_export(ctx, source_tex, &attribs);
    assert(exported);
    
    printf("Exported: %dx%d format=0x%x fd=%d\n",
           attribs.width, attribs.height, attribs.format,
           attribs.planes[0].fd);
    
    // Import back
    GLuint imported_tex = wlblur_dmabuf_import(ctx, &attribs);
    assert(imported_tex != 0);
    
    // Verify pixels match (read back and compare)
    verify_textures_match(source_tex, imported_tex, 256, 256);
    
    // Cleanup
    wlblur_dmabuf_close(&attribs);
    glDeleteTextures(1, &source_tex);
    glDeleteTextures(1, &imported_tex);
    wlblur_egl_destroy(ctx);
    
    printf("✓ DMA-BUF roundtrip test passed\n");
    return 0;
}
```

### Phase 5: Error Handling Patterns

**All functions must:**
1. Check return values of GL/EGL calls
2. Call `glGetError()` after GL operations
3. Call `eglGetError()` after EGL operations
4. Log errors with context (function name, parameters)
5. Clean up partial state on failure

**Example error handling:**

```c
GLenum gl_error = glGetError();
if (gl_error != GL_NO_ERROR) {
    fprintf(stderr, "[wlblur] GL error in %s: 0x%x\n",
            __func__, gl_error);
    return false;
}
```

## References

**Investigation Docs:**
- `docs/investigation/scenefx-investigation/investigation-summary.md` - Lines 105-128 (DMA-BUF import)
- `docs/post-investigation/comprehensive-synthesis1.md` - Lines 363-378 (Zero-copy architecture)

**ADRs:**
- `docs/decisions/002-dma-buf-vs-alternatives.md` - DMA-BUF rationale

**External:**
- [EGL_EXT_image_dma_buf_import spec](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_image_dma_buf_import.txt)
- [Mesa DMA-BUF examples](https://gitlab.freedesktop.org/mesa/demos)

## Notes & Comments

**Critical for Zero-Copy:** This is THE foundation. All performance benefits depend on DMA-BUF working correctly.

**FD Lifecycle:** Caller retains ownership of input FDs. Daemon owns output FDs until sent to client.

**Extension Fallback:** If DMA-BUF extensions unavailable, fail gracefully with clear error message (not a crash).

**Testing on Real Hardware:** DMA-BUF behavior varies by GPU driver. Test on Intel, AMD, NVIDIA.

## Deliverables

1. `libwlblur/src/egl_helpers.c` - EGL context management
2. `libwlblur/src/dmabuf.c` - DMA-BUF import/export
3. `libwlblur/include/wlblur/dmabuf.h` - Public API
4. `examples/test-dmabuf.c` - Validation program
5. Test output showing successful roundtrip
6. Commit: "feat(egl): implement EGL context and DMA-BUF infrastructure"
