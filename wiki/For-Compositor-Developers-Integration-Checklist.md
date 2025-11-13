# Compositor Integration Checklist

Step-by-step guide to integrating wlblur into your compositor.

## Prerequisites

- [ ] Compositor uses wlroots or similar (DMA-BUF support)
- [ ] Compositor renders with OpenGL/GLES
- [ ] Compositor has access to backdrop textures
- [ ] Can export textures as DMA-BUF file descriptors

## Integration Steps

### Step 1: Detect wlblurd

```c
// Check if daemon socket exists
const char *socket_path = "/run/user/1000/wlblur.sock";
if (access(socket_path, F_OK) == 0) {
    blur_available = true;
}
```

### Step 2: Connect to Daemon

```c
int blur_socket = socket(AF_UNIX, SOCK_STREAM, 0);

struct sockaddr_un addr = {
    .sun_family = AF_UNIX,
};
strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

if (connect(blur_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    // Handle connection failure
}
```

### Step 3: Implement Surface Type Detection

```c
const char* get_blur_preset(struct wl_surface *surface) {
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

### Step 4: Export Backdrop as DMA-BUF

```c
// Render backdrop (everything behind blur surface)
render_backdrop_to_texture(backdrop_texture);

// Export as DMA-BUF
struct wlr_dmabuf_attributes dmabuf;
wlr_buffer_get_dmabuf(backdrop_buffer, &dmabuf);
```

### Step 5: Send Blur Request

```c
struct wlblur_request req = {
    .protocol_version = 1,
    .op = WLBLUR_OP_RENDER_BLUR,
    .use_preset = true,
};
strncpy(req.preset_name, get_blur_preset(surface), 32);

// Send request with DMA-BUF FD
send_with_fd(blur_socket, &req, sizeof(req), dmabuf.fd);
```

### Step 6: Receive Blurred Result

```c
struct wlblur_response resp;
int result_fd;

recv_with_fd(blur_socket, &resp, sizeof(resp), &result_fd);

if (resp.status == WLBLUR_STATUS_SUCCESS) {
    // Import blurred texture
    EGLImage result_image = eglCreateImageKHR(
        egl_display, EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
}
```

### Step 7: Composite Blurred Texture

```c
// Bind blurred texture
glBindTexture(GL_TEXTURE_2D, blurred_texture);

// Render to scene graph
render_quad_with_texture(surface->geometry, blurred_texture);

// Render window content on top (if transparent)
render_surface_content(surface);
```

### Step 8: Handle Daemon Unavailable

```c
if (connect(blur_socket, ...) < 0) {
    log_warn("wlblurd not available, blur disabled");
    blur_enabled = false;
    // Fall back to no blur
}
```

### Step 9: Cleanup on Surface Destroy

```c
void surface_destroy(struct wl_surface *surface) {
    if (surface->blur_node_id != 0) {
        struct wlblur_request req = {
            .op = WLBLUR_OP_DESTROY_NODE,
            .node_id = surface->blur_node_id,
        };
        send(blur_socket, &req, sizeof(req), 0);
    }
}
```

## Testing

- [ ] Blur visible on transparent windows
- [ ] Hot reload works (edit wlblur config, `killall -USR1 wlblurd`)
- [ ] Daemon restart doesn't crash compositor
- [ ] Multiple blurred windows work
- [ ] Performance <1.5ms per blur

## Code Size Estimate

- Surface detection: ~30 lines
- IPC client: ~150 lines
- Integration glue: ~40 lines
- **Total: ~220 lines**

## Next Steps

- See [Integration Overview](Integration-Overview) for architecture
- See [API Reference](API-Reference) for protocol details
- See [Example Integration](Example-Integration) for full code
- See [Performance Considerations](Performance-Considerations) for optimization
