---
id: m-4
title: "ScrollWM Compositor Integration"
---

## Description

First real-world compositor integration. Implement minimal blur integration for ScrollWM that connects to wlblurd using the preset-based configuration architecture, proving the external daemon approach works with real compositor constraints.

**Timeline:** Weeks 9-10
**Status:** Planned
**Depends on:** m-3 (Configuration System Implementation)

## Objectives

1. Demonstrate ~220-line compositor integration promise
2. Validate preset-based configuration architecture
3. Prove ext_background_effect_v1 protocol integration works
4. Establish integration pattern for other compositors
5. Test with real layershell applications (quickshell, waybar)

## Deliverables

### Core Integration

- [ ] **ScrollWM blur integration module** (`scroll/src/blur_integration.c`, ~220 lines)
  - Unix socket client for wlblurd communication
  - DMA-BUF export from compositor framebuffers
  - Preset name selection based on surface type
  - IPC request/response handling
  - Error handling and graceful degradation

- [ ] **Surface type detection** (~30 lines)
  - Layershell namespace detection (waybar, quickshell, rofi)
  - XDG popup detection (tooltips)
  - Regular window detection
  - Map surface types to preset names

- [ ] **ext_background_effect_v1 protocol handler** (~40 lines)
  - Handle blur region requests from clients
  - Integrate with ScrollWM's rendering pipeline
  - Respect damage regions for performance

- [ ] **DMA-BUF interop** (~50 lines)
  - Export backdrop texture as DMA-BUF
  - Import blurred result from wlblurd
  - Handle multi-plane formats
  - Format/modifier negotiation

- [ ] **Compositor configuration** (`scroll/config/blur.toml`)
  - Enable/disable blur
  - Socket path configuration
  - Preset mapping overrides (optional)
  - Example configuration

### Testing & Validation

- [ ] **Integration testing with quickshell**
  - Panel blur requests work
  - Preset "panel" resolves correctly
  - Blur updates on configuration changes
  - No visual artifacts

- [ ] **Integration testing with waybar**
  - Waybar blur via ext_background_effect_v1
  - Consistent appearance with quickshell
  - Performance within budget

- [ ] **Performance benchmarking**
  - Target: <1.5ms total (blur + IPC)
  - Measure at 1920×1080 with 3 passes
  - Test with multiple blurred surfaces
  - Profile with perf/gpuvis

- [ ] **Stress testing**
  - Multiple clients (quickshell + waybar + windows)
  - Rapid window opening/closing
  - Configuration hot-reload during rendering
  - Daemon restart recovery

### Documentation

- [ ] **Integration guide** (`docs/integration/scrollwm-integration.md`)
  - Step-by-step integration instructions
  - Code walkthrough
  - Common pitfalls and solutions
  - Debugging tips

- [ ] **Compositor integration template** (`docs/integration/compositor-template.md`)
  - Generic integration pattern
  - Applicable to niri, sway, river, etc.
  - Preset mapping examples
  - DMA-BUF export patterns

- [ ] **Demo video/screenshots**
  - Blur in action on ScrollWM
  - Quickshell panels with blur
  - Hot-reload demonstration
  - Side-by-side with Hyprland blur

### Git Artifacts

- [ ] **Git patch for ScrollWM** (`integrations/scroll/blur-integration.patch`)
  - Clean, reviewable patch
  - Proper commit messages
  - No unrelated changes
  - Ready for upstream submission

- [ ] **Build instructions** (`integrations/scroll/README.md`)
  - How to apply patch
  - Build dependencies
  - Testing instructions

## Success Criteria

- [ ] Integration code ≤ 250 lines (target: ~220 lines)
- [ ] Works with quickshell layershell surfaces
- [ ] Works with waybar layershell surfaces
- [ ] Preset resolution functional (panel, window, hud, tooltip)
- [ ] No compositor restart needed for wlblur config changes
- [ ] Performance within target (<1.5ms per blur @ 1080p)
- [ ] No memory leaks (validated with valgrind)
- [ ] No GPU hangs or crashes
- [ ] Graceful degradation if wlblurd is unavailable
- [ ] Clean separation: compositor handles state, daemon handles computation

## Implementation Notes

### Integration Architecture

```
┌────────────────────────────────────┐
│ ScrollWM                           │
│                                    │
│  ext_background_effect_v1          │
│  handler (40 lines)                │
│         ↓                          │
│  Surface type detector (30 lines)  │
│         ↓                          │
│  Preset name mapper (10 lines)    │
│         ↓                          │
│  IPC client (100 lines)            │
│         ↓                          │
│  DMA-BUF export/import (40 lines)  │
│                                    │
└─────────────┬──────────────────────┘
              │ Unix socket
              ↓
┌────────────────────────────────────┐
│ wlblurd                            │
│  Preset resolution                 │
│  Blur rendering                    │
└────────────────────────────────────┘
```

### Minimal Preset Mapping Logic

```c
// scroll/src/blur_integration.c

const char* get_blur_preset(struct wl_surface *surface) {
    // Layershell surfaces
    if (surface_is_layershell(surface)) {
        const char *ns = layershell_get_namespace(surface);

        if (strcmp(ns, "waybar") == 0) return "panel";
        if (strcmp(ns, "quickshell") == 0) return "panel";
        if (strcmp(ns, "rofi") == 0) return "hud";

        return "panel";  // Default for layershell
    }

    // XDG popups
    if (surface_is_xdg_popup(surface)) {
        return "tooltip";
    }

    // Regular windows
    return "window";
}
```

**Total:** ~15 lines for all preset mapping logic.

### IPC Request Example

```c
// scroll/src/blur_integration.c

struct wlblur_buffer* request_blur(struct wl_surface *surface,
                                    struct wlr_buffer *backdrop) {
    // Get preset for this surface
    const char *preset = get_blur_preset(surface);

    // Export backdrop as DMA-BUF
    struct wlr_dmabuf_attributes dmabuf;
    if (!wlr_buffer_get_dmabuf(backdrop, &dmabuf)) {
        return NULL;
    }

    // Build IPC request
    struct wlblur_request req = {
        .header = {
            .magic = WLBLUR_MAGIC,
            .version = WLBLUR_PROTOCOL_VERSION,
            .opcode = WLBLUR_OP_RENDER,
        },
        .preset_name = {0},  // Copy preset name
        .width = dmabuf.width,
        .height = dmabuf.height,
        .format = dmabuf.format,
        .modifier = dmabuf.modifier,
    };
    strncpy(req.preset_name, preset, sizeof(req.preset_name) - 1);

    // Send request with DMA-BUF FD
    if (!send_blur_request(daemon_fd, &req, dmabuf.fd[0])) {
        return NULL;
    }

    // Receive blurred result
    struct wlblur_response resp;
    int blurred_fd;
    if (!recv_blur_response(daemon_fd, &resp, &blurred_fd)) {
        return NULL;
    }

    // Import blurred buffer
    return import_dmabuf_buffer(blurred_fd, resp.width, resp.height,
                                 resp.format, resp.modifier);
}
```

### Error Handling Strategy

```c
// Graceful degradation if daemon unavailable
static int connect_to_daemon(void) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };
    snprintf(addr.sun_path, sizeof(addr.sun_path),
             "/run/user/%d/wlblur.sock", getuid());

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        wlr_log(WLR_INFO, "wlblurd not available, blur disabled");
        close(fd);
        return -1;
    }

    return fd;
}

// In render path:
if (blur_enabled && daemon_fd >= 0) {
    struct wlr_buffer *blurred = request_blur(surface, backdrop);
    if (blurred) {
        render_buffer(blurred);
    } else {
        // Fallback: render without blur
        render_buffer(backdrop);
    }
} else {
    // Blur disabled or daemon unavailable
    render_buffer(backdrop);
}
```

### Preset Mapping Configuration (Optional)

```toml
# scroll/config.toml (user can override preset mappings)

[blur]
enabled = true
daemon_socket = "/run/user/1000/wlblur.sock"

# Optional: Override default preset mappings
[blur.preset_mappings]
quickshell = "panel"
waybar = "panel"
rofi = "hud"
foot = "window"  # Terminal uses window preset
```

**Note:** This compositor-side config is optional. Default mappings in code are sufficient for most users.

## Performance Targets

**Target:** <1.5ms total per blur operation @ 1920×1080

**Budget breakdown:**
- DMA-BUF export: <0.1ms
- IPC request/response: <0.2ms
- Blur rendering (daemon): <1.2ms
- DMA-BUF import: <0.05ms
- **Total:** ~1.5ms

**Validation:**
- Measure with `perf stat`
- Profile with `gpuvis` (GPU timeline)
- Test on mid-range GPU (NVIDIA GTX 1060 or AMD RX 5700)

**Acceptance:**
- 60 FPS stable with 2-3 blurred surfaces
- 120 FPS with 1 blurred surface
- No frame drops during blur

## Related Milestones

**Depends on:**
- m-2: wlblurd IPC Daemon (provides blur service)
- m-3: Configuration System (provides preset resolution)

**Validates:**
- ADR-001: External daemon architecture (proves minimal integration works)
- ADR-006: Daemon configuration with presets (proves preset system works)

**Enables:**
- m-5: Testing & Validation (provides real integration to test)
- Future compositor integrations (niri, sway, river)

## References

- **ADR-001:** Why External Daemon Architecture
- **ADR-004:** IPC Protocol Design
- **ADR-006:** Daemon Configuration with Presets
- **ScrollWM:** https://github.com/[scrollwm-repo] (target compositor)
- **ext_background_effect_v1:** Wayland protocol spec
- **wlroots DMA-BUF docs:** https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/include/wlr/render/dmabuf.h
