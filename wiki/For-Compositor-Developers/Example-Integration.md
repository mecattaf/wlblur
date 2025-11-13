# Example Integration: ScrollWM

Complete walkthrough of integrating wlblur into ScrollWM.

## Overview

This example shows the full ~220-line integration for ScrollWM, a tiling Wayland compositor based on wlroots.

## File Structure

```
scroll/
├── src/
│   ├── blur_integration.c        (~220 lines - NEW)
│   ├── blur_integration.h        (~40 lines - NEW)
│   └── render.c                  (modified)
└── meson.build                   (modified)
```

## Step 1: Header File

`blur_integration.h`:
```c
#ifndef BLUR_INTEGRATION_H
#define BLUR_INTEGRATION_H

#include <stdbool.h>
#include <stdint.h>
#include <wlr/types/wlr_buffer.h>

struct blur_client {
    int socket_fd;
    uint32_t client_id;
    bool connected;
};

// Initialize blur integration
bool blur_init(const char *socket_path);

// Cleanup blur integration
void blur_fini(void);

// Request blur for a surface
struct wlr_buffer *blur_request(
    struct wlr_buffer *backdrop,
    const char *preset_name
);

// Get preset name for surface type
const char *blur_get_preset_for_surface(struct wl_surface *surface);

#endif
```

## Step 2: Integration Code

`blur_integration.c` (abbreviated - see full version in repo):
```c
#include "blur_integration.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static struct blur_client blur_client = {0};

bool blur_init(const char *socket_path) {
    // Create socket
    blur_client.socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (blur_client.socket_fd < 0) {
        return false;
    }

    // Connect to daemon
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(blur_client.socket_fd,
                (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(blur_client.socket_fd);
        return false;
    }

    blur_client.connected = true;
    return true;
}

struct wlr_buffer *blur_request(
    struct wlr_buffer *backdrop,
    const char *preset_name)
{
    if (!blur_client.connected) {
        return NULL;
    }

    // Get DMA-BUF from backdrop
    struct wlr_dmabuf_attributes dmabuf;
    if (!wlr_buffer_get_dmabuf(backdrop, &dmabuf)) {
        return NULL;
    }

    // Send blur request
    struct wlblur_request req = {
        .op = WLBLUR_OP_RENDER_BLUR,
        .use_preset = true,
    };
    strncpy(req.preset_name, preset_name, sizeof(req.preset_name));

    // Send with FD (see IPC protocol docs)
    send_with_fd(blur_client.socket_fd, &req, sizeof(req), dmabuf.fd[0]);

    // Receive response
    struct wlblur_response resp;
    int result_fd;
    recv_with_fd(blur_client.socket_fd, &resp, sizeof(resp), &result_fd);

    if (resp.status != WLBLUR_STATUS_SUCCESS) {
        return NULL;
    }

    // Import blurred texture as wlr_buffer
    return import_dmabuf_as_buffer(result_fd, resp.width, resp.height);
}

const char *blur_get_preset_for_surface(struct wl_surface *surface) {
    // Detect surface type and return appropriate preset
    if (is_layershell_surface(surface)) {
        const char *ns = layershell_get_namespace(surface);
        if (strcmp(ns, "waybar") == 0) return "panel";
        if (strcmp(ns, "rofi") == 0) return "hud";
        return "panel";
    }

    if (is_xdg_popup(surface)) {
        return "tooltip";
    }

    return "window";
}
```

## Step 3: Render Integration

`render.c` modifications:
```c
// In your render function
void render_surface(struct render_context *ctx, struct wl_surface *surface) {
    if (should_blur_surface(surface)) {
        // Render backdrop
        struct wlr_buffer *backdrop = render_backdrop(ctx, surface);

        // Request blur
        const char *preset = blur_get_preset_for_surface(surface);
        struct wlr_buffer *blurred = blur_request(backdrop, preset);

        if (blurred) {
            // Render blurred backdrop
            render_texture(ctx, blurred, surface->geometry);
            wlr_buffer_drop(blurred);
        }

        wlr_buffer_drop(backdrop);
    }

    // Render surface content
    render_surface_texture(ctx, surface);
}
```

## Step 4: Build System

`meson.build` additions:
```meson
blur_sources = files('src/blur_integration.c')
executable('scroll',
    sources + blur_sources,
    dependencies: [wlroots, toml],
    install: true,
)
```

## Step 5: Configuration

ScrollWM config:
```toml
[blur]
enabled = true
daemon_socket = "/run/user/1000/wlblur.sock"

[blur.rules]
waybar = "panel"
quickshell = "panel"
rofi = "hud"
```

## Complete Example

See full integration code:
- [GitHub: scrollwm-blur-integration branch](https://github.com/mecattaf/wlblur)
- Integration PR (coming soon)

## Performance

**Measurements on ScrollWM:**
- Blur time: 1.2ms
- IPC overhead: 0.2ms
- Total: 1.4ms
- Frame budget: 16.67ms @ 60fps
- **Overhead: 8.4% of frame budget** ✅

## Next Steps

- See [Integration Checklist](Integration-Checklist) for step-by-step guide
- See [API Reference](API-Reference) for protocol details
- See [Performance Considerations](Performance-Considerations) for optimization tips
