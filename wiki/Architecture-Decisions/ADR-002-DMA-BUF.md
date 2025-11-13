# ADR-002: DMA-BUF for Zero-Copy GPU Memory Sharing

**Status**: Proposed
**Date**: 2025-01-15

## Context

Our external blur daemon (ADR-001) requires transferring texture data between the compositor process and daemon process for blur rendering. The textures are large (1920×1080 RGBA8 = 8.3 MB, 4K = 33 MB) and transferred frequently (every frame for dynamic content). The memory transfer mechanism must be:

1. **Fast**: <0.2ms overhead to stay within 60 FPS budget
2. **Zero-copy**: Avoid CPU memory copies (would add 20-50ms for 1080p)
3. **GPU-native**: Keep data in GPU memory throughout pipeline
4. **Cross-process**: Work across compositor and daemon process boundaries
5. **Standard**: Supported by Mesa, wlroots, Smithay, and modern compositors

## Decision

**We will use DMA-BUF (Direct Memory Access Buffer) for zero-copy GPU memory sharing between compositor and daemon.**

### How DMA-BUF Works

```
Compositor Process:
  GL Texture (GPU memory)
       ↓ eglExportDMABUFImageKHR()
  DMA-BUF File Descriptor
       ↓ Unix socket + SCM_RIGHTS

Daemon Process:
  DMA-BUF File Descriptor (received)
       ↓ eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT)
  EGLImageKHR
       ↓ glEGLImageTargetTexture2DOES()
  GL Texture (same GPU memory!)
```

**Key Insight:** File descriptor points to kernel-managed GPU memory. Both processes map the same physical GPU memory. Zero CPU copies.

### Implementation Pattern

**Export (Compositor):**
```c
// 1. Render backdrop to texture
GLuint backdrop_tex = render_backdrop_for_window(window);

// 2. Create EGL image from GL texture
EGLImageKHR image = eglCreateImageKHR(
    egl_display, egl_context,
    EGL_GL_TEXTURE_2D,
    (EGLClientBuffer)(uintptr_t)backdrop_tex,
    NULL
);

// 3. Export as DMA-BUF
int dmabuf_fd;
uint32_t stride, offset;
eglExportDMABUFImageMESA(egl_display, image,
                         &dmabuf_fd, &stride, &offset);

// 4. Send FD to daemon via Unix socket
struct blur_request req = {
    .width = 1920,
    .height = 1080,
    .format = DRM_FORMAT_ARGB8888,
    .stride = stride,
    .offset = offset,
};
sendmsg(daemon_socket, &req, dmabuf_fd);  // SCM_RIGHTS
```

**Import (Daemon):**
```c
// 1. Receive DMA-BUF FD
struct blur_request req;
int dmabuf_fd;
recvmsg(client_socket, &req, &dmabuf_fd);  // SCM_RIGHTS

// 2. Create EGL image from DMA-BUF
EGLint attribs[] = {
    EGL_WIDTH, req.width,
    EGL_HEIGHT, req.height,
    EGL_LINUX_DRM_FOURCC_EXT, req.format,
    EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,
    EGL_DMA_BUF_PLANE0_STRIDE_EXT, req.stride,
    EGL_DMA_BUF_PLANE0_OFFSET_EXT, req.offset,
    EGL_NONE,
};
EGLImageKHR image = eglCreateImageKHR(
    egl_display, EGL_NO_CONTEXT,
    EGL_LINUX_DMA_BUF_EXT,
    NULL, attribs
);

// 3. Import as GL texture
GLuint imported_tex;
glGenTextures(1, &imported_tex);
glBindTexture(GL_TEXTURE_2D, imported_tex);
glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

// 4. Render blur
GLuint blurred_tex = kawase_blur(imported_tex, ...);

// 5. Export blurred result back
// (same export process as compositor)
```

## Alternatives Considered

### Alternative 1: Shared Memory (shm) with CPU Copy

**Approach:** Allocate shared memory segment, compositor copies GPU → CPU → shm, daemon copies shm → CPU → GPU.

**Pros:**
- Simple API (mmap, memcpy)
- No EGL extensions required
- Works on any system

**Cons:**
- **Two CPU copies per frame**: GPU → CPU (20ms) + CPU → GPU (20ms) = 40ms for 1080p
- **Massive performance penalty**: 40ms exceeds entire 16.67ms frame budget
- **Stalls GPU pipeline**: Forces synchronization (glReadPixels blocks)
- **CPU/memory bandwidth waste**: 8.3 MB × 2 copies = 16.6 MB/frame @ 60 FPS = 996 MB/s

**Why Rejected:** Performance completely unacceptable. 40ms >> 0.2ms target.

### Alternative 2: Pixel Buffer Objects (PBO) with Shared Memory

**Approach:** Use GL_PIXEL_PACK_BUFFER to async transfer GPU → shm, daemon uses GL_PIXEL_UNPACK_BUFFER to async transfer shm → GPU.

**Pros:**
- Async transfers (don't block GPU)
- Better than synchronous shm

**Cons:**
- **Still requires CPU memory transfers**: Just async, not eliminated
- **Adds frame latency**: Transfer happens N frames later
- **Complex state management**: Need triple buffering
- **Still wastes bandwidth**: 8.3 MB × 2 = 16.6 MB per frame
- **Not truly zero-copy**: Data goes through CPU memory

**Why Rejected:** Still has fundamental overhead. DMA-BUF is strictly better.

### Alternative 3: Unix Domain Socket with Texture Streaming

**Approach:** Compress texture data (e.g., PNG), send over socket, decompress in daemon.

**Pros:**
- Simple transport mechanism
- Works across network (could have remote blur daemon)

**Cons:**
- **Compression overhead**: 5-20ms encoding + decoding
- **CPU intensive**: Encoder/decoder runs on CPU
- **Quality loss**: Lossy compression affects blur quality
- **Bandwidth**: Even compressed, ~1 MB/frame @ 60 FPS = 60 MB/s
- **Still requires GPU ↔ CPU transfers**

**Why Rejected:** Adds latency instead of reducing it. Completely wrong direction.

### Alternative 4: Separate GPU Context but Shared Memory

**Approach:** Create daemon EGL context that shares objects with compositor EGL context (EGL_KHR_create_context share_context).

**Pros:**
- True zero-copy within same GPU
- No file descriptor passing
- Simple texture sharing

**Cons:**
- **Requires same process or parent-child fork**: Can't share contexts across arbitrary processes
- **Not applicable to external daemon**: Would require compositor to fork daemon (loses crash isolation)
- **Context switching overhead**: Still need to serialize access
- **Driver-dependent behavior**: Some drivers don't support cross-thread sharing well

**Why Rejected:** Incompatible with external daemon architecture (ADR-001).

### Alternative 5: Vulkan External Memory

**Approach:** Use Vulkan's VK_KHR_external_memory_fd extension instead of EGL/OpenGL DMA-BUF.

**Pros:**
- Vulkan is modern, explicit API
- Better synchronization primitives (VkSemaphore, VkFence)
- Forward-compatible with Vulkan compositors

**Cons:**
- **Compositor ecosystem not ready**: wlroots still primarily GLES2/GLES3
- **Requires Vulkan in daemon**: Adds dependency, increases code complexity
- **Same underlying mechanism**: Vulkan external memory uses... DMA-BUF FDs!
- **Can add later**: Not mutually exclusive with EGL approach

**Why Deferred:** Same mechanism underneath. Start with EGL (matches compositor status quo), add Vulkan path later as compositors migrate.

## Consequences

### Positive

1. **True Zero-Copy**: GPU memory accessed directly by both processes
   - No CPU memory copies
   - No GPU ↔ CPU transfers
   - Data stays in GPU memory throughout

2. **Minimal Overhead**: ~0.05ms for FD passing and EGL image creation
   - vs 40ms for shm with CPU copy
   - 800× faster

3. **Standard Linux Mechanism**: Well-supported across stack
   - Kernel DMA-BUF framework (since Linux 3.3, 2012)
   - Mesa EGL_EXT_image_dma_buf_import (2013)
   - wlroots has helpers (wlr_buffer_get_dmabuf)
   - Smithay has DmaBuf type

4. **No Data Copies in Compositor**: Minimal integration code
   - wlroots: `wlr_buffer_get_dmabuf()` (one line)
   - Smithay: `buffer.dmabuf()` (one line)

5. **GPU Vendor Agnostic**: Works on Intel, AMD, NVIDIA
   - Kernel mechanism, not driver-specific
   - Same code for all GPUs

6. **Already Used in Wayland**: Proven technology
   - Wayland uses DMA-BUF for client buffers
   - wlroots screencopy uses DMA-BUF
   - OBS Studio uses DMA-BUF for capture

7. **Synchronization Primitives Available**: Can add sync fences later
   - EGL_ANDROID_native_fence_sync
   - Explicit sync for pipelining
   - Start without, add if needed

### Negative

1. **EGL Extension Dependency**: Requires modern Mesa
   - EGL_EXT_image_dma_buf_import (compositor → daemon)
   - EGL_MESA_image_dma_buf_export (daemon → compositor)
   - Mitigation: These have been in Mesa since 2013, universally available

2. **Format Negotiation Complexity**: Must agree on pixel formats
   - DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888, etc.
   - Different byte orders (ARGB vs BGRA)
   - Tiling modifiers (linear, Intel Y-tiled, AMD DCC)
   - Mitigation: Start with simple formats (ARGB8888 linear), add complex later

3. **Error Handling Complexity**: Many failure modes
   - FD passing can fail (too many open FDs)
   - EGL image creation can fail (unsupported format/modifier)
   - Texture import can fail (driver issues)
   - Mitigation: Comprehensive error handling, fallback to "blur disabled"

4. **Debugging Difficulty**: Hard to inspect
   - Can't just hexdump shared memory
   - GPU memory not accessible from debugger
   - Mitigation: Add debug mode that exports textures to PNG files

5. **GPU Memory Lifetime Tracking**: Reference counting critical
   - Must not free GPU memory while daemon still using it
   - Compositor must keep wlr_buffer locked until daemon done
   - Daemon must close FD after importing
   - Mitigation: Use wlroots buffer lock/unlock, careful FD management

## Implementation Details

### Compositor Side (wlroots)

```c
// Export backdrop texture
struct wlr_buffer *backdrop = render_backdrop(container);

// Get DMA-BUF attributes (wlroots helper)
struct wlr_dmabuf_attributes dmabuf;
if (!wlr_buffer_get_dmabuf(backdrop, &dmabuf)) {
    // Fallback: buffer not exportable (rare)
    return NULL;
}

// Send to daemon
struct blur_request req = {
    .width = dmabuf.width,
    .height = dmabuf.height,
    .format = dmabuf.format,
    .modifier = dmabuf.modifier,
    .num_planes = dmabuf.n_planes,
};
for (int i = 0; i < dmabuf.n_planes; i++) {
    req.planes[i].offset = dmabuf.offset[i];
    req.planes[i].stride = dmabuf.stride[i];
}

// Send FD via SCM_RIGHTS
sendmsg_with_fd(daemon_socket, &req, dmabuf.fd[0]);

// Keep buffer locked until daemon responds
wlr_buffer_lock(backdrop);
```

### Daemon Side (Core)

```c
// Receive DMA-BUF
struct blur_request req;
int dmabuf_fd = recvmsg_with_fd(client_socket, &req);

// Import as EGL image
EGLImageKHR image = create_egl_image_from_dmabuf(
    egl_display, dmabuf_fd,
    req.width, req.height,
    req.format, req.modifier,
    req.planes, req.num_planes
);

// Import as GL texture
GLuint tex = gl_texture_from_egl_image(image);

// Blur
GLuint blurred = kawase_blur(tex, req.radius, req.passes);

// Export result
int result_fd = export_texture_to_dmabuf(blurred);

// Send back
struct blur_response resp = {
    .width = req.width,
    .height = req.height,
    .format = req.format,
};
sendmsg_with_fd(client_socket, &resp, result_fd);

// Cleanup
close(dmabuf_fd);  // Daemon done with input
glDeleteTextures(1, &tex);
eglDestroyImageKHR(egl_display, image);
```

## Performance Validation

**Target:** <0.2ms overhead for DMA-BUF import/export

**Measured in SceneFX investigation:**
- DMA-BUF export: ~0.02ms
- FD passing via socket: ~0.05ms
- DMA-BUF import: ~0.02ms
- FD passing back: ~0.05ms
- **Total:** ~0.14ms ✅ (within target)

**Comparison to alternatives:**
- CPU copy: 40ms (286× slower) ❌
- PBO async: 5-10ms (35-71× slower) ❌
- Compression: 10-25ms (71-178× slower) ❌

## Code Reuse Opportunities

**From SceneFX:**
- `render/fx_renderer/fx_texture.c:353-404` - DMA-BUF import implementation
- `render/egl.c:751-841` - EGLImageKHR from DMA-BUF attributes

**From wlroots:**
- `wlr_buffer_get_dmabuf()` - Extract DMA-BUF from buffer
- `wlr_dmabuf_attributes` - Standard attribute structure

**From Mesa demos:**
- `egl_dma_buf_import_export.c` - Complete example

## Risks and Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| GPU doesn't support DMA-BUF | Critical | Very Low | Mesa has supported since 2013, all modern GPUs work |
| Format/modifier incompatibility | High | Low | Start with simple formats (ARGB8888 linear), validate during handshake |
| FD exhaustion | Medium | Low | Limit max buffers per client (256), proper cleanup |
| Memory leak (FDs not closed) | Medium | Medium | Comprehensive error handling, RAII patterns |
| Compositor crash while daemon holds buffer | Low | Low | wlroots handles this, daemon just sees closed socket |

## References

- Investigation docs:
  - [SceneFX Investigation Summary](../Technical-Investigation/SceneFX-Summary) - DMA-BUF import patterns
  - [Comparative Analysis](../Technical-Investigation/Comparative-Analysis) - Zero-copy architecture analysis

- External resources:
  - [Linux DMA-BUF documentation](https://docs.kernel.org/driver-api/dma-buf.html)
  - [Mesa EGL_EXT_image_dma_buf_import spec](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_image_dma_buf_import.txt)
  - [wlroots DMA-BUF helpers](https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/include/wlr/types/wlr_buffer.h)

- Related ADRs:
  - [ADR-001: External daemon architecture](ADR-001-External-Daemon) (motivates need for cross-process GPU sharing)
  - [ADR-004: IPC protocol design](ADR-004-IPC-Protocol) (DMA-BUF FDs passed via protocol)

## Community Feedback

We invite feedback on this decision:

- **Driver developers**: Are there format/modifier combinations we should prioritize or avoid?
- **Compositor maintainers**: Does `wlr_buffer_get_dmabuf()` cover all use cases, or do we need custom export?
- **Performance testers**: Can you validate <0.2ms overhead on your hardware?

Please open issues at [project repository] or discuss in [community forum].
