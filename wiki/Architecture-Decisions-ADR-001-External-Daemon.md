# ADR-001: Why External Daemon Architecture

**Status**: Proposed
**Date**: 2025-01-15

## Context

Wayland compositors like scroll, niri, Sway, and others currently lack macOS-level background blur effects. Users want frosted glass aesthetics similar to Apple's design language, but implementing blur in-compositor requires significant maintenance burden and tight coupling with rendering internals.

### Problem Analysis

Background blur (frosted glass effect) is fundamentally different from simpler visual effects like rounded corners or drop shadows:

**Why Blur is Complex:**
- **Multi-pass rendering**: Requires 4-6 GPU passes with ping-pong framebuffers (vs 1 pass for decorations)
- **Background dependency**: Must capture and process scene content behind windows (can't be self-contained)
- **Damage tracking**: Requires exponential expansion (2^passes × radius) with artifact prevention
- **Performance sensitive**: Costs 1-3ms GPU time (vs 0.1ms for decorations), needs optimization
- **Renderer invasive**: Must modify render passes, add FBO management, change damage flow

**Evidence from Maintainers:**

The scroll maintainer (who successfully added rounded corners and shadows) explicitly stated blur is too invasive:
> "Blur requires more changes to the renderers. I had to modify the gles2 and vulkan renderers to add the new FX stuff... It needs multiple rendering passes, affects damage tracking... wlroots's main renderer is still GLES2, and there are plans to move to Vulkan as main target. Rewriting a GLES2 renderer when it will not be the main one soon feels like a wasted effort."

This confirms that even maintainers willing to add decorations (50+ files changed) find blur too burdensome for in-compositor integration.

## Decision

**We will implement blur as an external standalone daemon process that communicates with compositors via IPC using DMA-BUF for zero-copy GPU memory sharing.**

### Architecture Overview

```
┌──────────────────────────────┐
│ Compositor (scroll/niri/etc) │
│  • Standard renderer         │
│  • ~200 line IPC client      │
│  • DMA-BUF export            │
└──────────┬───────────────────┘
           │ Unix Socket + SCM_RIGHTS
           ↓
┌──────────────────────────────┐
│ Blur Daemon (separate)       │
│  • All blur algorithms       │
│  • All shader code           │
│  • Independent GL context    │
│  • DMA-BUF import/export     │
└──────────────────────────────┘
```

## Alternatives Considered

### Alternative 1: In-Compositor Integration (Like Hyprland/SceneFX)

**Approach:** Fork compositor to add blur rendering directly into render pipeline.

**Pros:**
- Maximum performance (~1.2ms for 1080p blur)
- Direct framebuffer access, no IPC overhead
- Tight damage tracking integration
- Can implement advanced optimizations (caching, xray mode)

**Cons:**
- Requires forking wlroots or maintaining large patches
- Maintainers explicitly reject this approach (scroll maintainer's comments)
- Must reimplement for each compositor (Hyprland code ≠ Wayfire code ≠ SceneFX code)
- Compositor crashes take down blur
- Needs reimplementation for GLES2 → Vulkan transition
- 50+ files changed per compositor

**Why Rejected:** Maintainer resistance, maintenance burden, no code sharing across compositors.

### Alternative 2: Plugin Architecture (Like Wayfire)

**Approach:** Implement blur as a loadable .so plugin with compositor-provided plugin API.

**Pros:**
- Clean separation boundary
- Can be enabled/disabled at runtime
- Shared GL context (slightly better performance than IPC)

**Cons:**
- Requires each compositor to design and maintain a plugin API
- Still requires compositor changes to support plugins
- Crash in plugin can kill compositor (shared address space)
- Not applicable to compositors without plugin systems (scroll, niri, Sway)
- Language binding complexity (niri uses Rust)

**Why Rejected:** Requires significant compositor buy-in (plugin API design), not universally applicable.

### Alternative 3: Wayland Protocol Extension

**Approach:** Define a Wayland protocol (e.g., ext-background-effect-v1) for clients to request blur.

**Pros:**
- Standardized approach across compositors
- Client-driven blur policy
- Good for application-specific blur needs

**Cons:**
- Still requires in-compositor blur implementation (doesn't solve the complexity problem)
- Protocol standardization takes years
- Only helps with "which windows get blur", not "how to implement blur"
- Compositors still need the rendering infrastructure

**Why Rejected:** Orthogonal to implementation strategy. We may add protocol support later, but it doesn't solve the fundamental implementation challenge.

### Alternative 4: Compositor-Agnostic Rendering Library

**Approach:** Create a libblur.so that compositors link against and call directly.

**Pros:**
- Code sharing across compositors
- Better performance than IPC (direct function calls)
- Can evolve independently

**Cons:**
- Requires compositor integration (adding rendering hooks)
- Crashes in library crash compositor
- Language binding issues (C library, Rust compositor)
- Still requires compositor changes to rendering pipeline
- ABI stability concerns

**Why Rejected:** Still requires significant compositor changes, no crash isolation.

## Consequences

### Positive

1. **Minimal Compositor Changes**: Only 3-5 files modified per compositor (~200 lines of IPC client code)
   - vs 50+ files for in-compositor implementation

2. **Multi-Compositor Support**: Single daemon works with scroll, niri, Sway, Hyprland, River, etc.
   - No need to fork each compositor
   - Centralized blur quality improvements benefit everyone

3. **Crash Isolation**: Daemon failure doesn't kill compositor
   - Compositor can gracefully disable blur and restart daemon
   - GPU hangs isolated to daemon process

4. **Independent Versioning**: Daemon can be updated without recompiling compositors
   - New blur algorithms added without compositor changes
   - Material system can evolve independently

5. **Maintainer-Friendly**: Addresses scroll maintainer's concerns directly
   - No wlroots fork required
   - Stay close to upstream
   - GLES2 → Vulkan transition doesn't affect integration

6. **Language Flexibility**: Compositors can use language-appropriate IPC clients
   - C client for wlroots-based compositors
   - Rust client for niri/cosmic

7. **Tight Coupling Not Required**: Blur operates on rendered pixels, not window metadata
   - Compositor handles state (window positions, stacking order)
   - Daemon handles computation (multi-pass blur)
   - Clean separation of concerns

### Negative

1. **IPC Overhead**: ~0.2ms per blur operation
   - 1.4ms total vs 1.2ms in-process (17% overhead)
   - Acceptable for 60 FPS (16.67ms budget), marginal for 144 FPS
   - Mitigation: Async pipeline (hide IPC latency behind GPU work)

2. **Some Optimizations Harder**: Certain in-compositor optimizations become difficult
   - Blur caching: Requires compositor cooperation (send dirty flag)
   - xray mode: Compositor must decide which windows to blur
   - Per-monitor FBO reuse: Daemon must track output IDs
   - Mitigation: Design protocol to support these patterns

3. **State Replication**: Daemon maintains virtual scene graph
   - Compositor must send blur node creation/destruction messages
   - ~200 lines of state tracking code in daemon
   - Mitigation: Keep state minimal (blur nodes, buffer IDs only)

4. **Additional Process**: Users must run blur daemon
   - Extra process to manage (systemd service, socket activation)
   - Mitigation: Auto-start via compositor, graceful degradation if unavailable

5. **Testing Complexity**: Must test compositor + daemon integration
   - More moving parts
   - IPC protocol versioning
   - Mitigation: Comprehensive integration test suite

## Performance Targets

**Target:** 60 FPS = 16.67ms budget per frame

**Budget breakdown (1920×1080, mid-range GPU):**
- Compositor rendering: 4-8ms
- Blur (daemon): 1.4ms (1.2ms blur + 0.2ms IPC)
- Compositing result: 0.5ms
- **Total:** 5.9-9.9ms
- **Headroom:** 6.77-10.77ms (40-65% remaining)

**Conclusion:** IPC overhead is acceptable for target use case.

## Integration Example

### Compositor Side (scroll)

```c
// sway/desktop/blur_integration.c (~200 lines)
void blur_init(void) {
    blur_daemon_fd = connect_to_daemon("/run/user/1000/blur.sock");
}

struct wlr_buffer *blur_request(struct sway_container *con,
                                struct wlr_buffer *backdrop) {
    // Export backdrop as DMA-BUF
    struct wlr_dmabuf_attributes dmabuf;
    wlr_buffer_get_dmabuf(backdrop, &dmabuf);

    // Send blur request
    send_dmabuf_fd(blur_daemon_fd, dmabuf.fd[0], ...);

    // Receive blurred result
    int blurred_fd = recv_dmabuf_fd(blur_daemon_fd);
    return wlr_buffer_from_dmabuf(...);
}
```

### Daemon Side

```c
// daemon/main.c
while (accept_connection(server_fd)) {
    struct blur_request req = recv_request();

    // Import DMA-BUF
    EGLImageKHR image = eglCreateImageKHR(..., req.dmabuf_fd, ...);
    GLuint tex = import_egl_image(image);

    // Render blur (Kawase algorithm)
    GLuint blurred = kawase_blur(tex, req.radius, req.passes);

    // Export result
    int result_fd = export_texture_to_dmabuf(blurred);
    send_dmabuf_fd(client_fd, result_fd);
}
```

## Validation

This decision will be validated by:

1. **Proof of Concept (Week 1-2)**: DMA-BUF import/export test shows <0.2ms latency
2. **MVP (Week 8-9)**: scroll integration with stable rendering at 60 FPS
3. **Multi-Compositor (Week 13-14)**: niri integration proves architecture is compositor-agnostic
4. **Community Feedback**: scroll/niri maintainer acceptance of integration approach

## References

- Investigation docs:
  - [Blur Daemon Approach](Architecture-Daemon-Approach-Rationale) - Detailed architectural justification
  - [Why IPC Is Better](Why-IPC-Is-Better) - Tight vs loose coupling analysis
  - [SceneFX Investigation Summary](Technical-Investigation-SceneFX-Summary) - SceneFX blur implementation
  - [Comparative Analysis](Technical-Investigation-Comparative-Analysis) - Three-compositor comparison

- [scroll maintainer discussion](Background-ScrollWM-Motivation)
- Related: [ADR-002 (DMA-BUF choice)](ADR-002-DMA-BUF), [ADR-004 (IPC protocol design)](ADR-004-IPC-Protocol)

## Community Feedback

We invite feedback on this decision:

- **Compositor maintainers**: Is ~200 lines of integration code acceptable?
- **Users**: Is 0.2ms IPC overhead acceptable for blur quality?
- **Developers**: Are there alternative architectures we should consider?

Please open issues at [project repository] or discuss in [community forum].
