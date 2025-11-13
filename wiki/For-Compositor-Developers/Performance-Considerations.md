# Performance Considerations

Tips for optimal blur performance in compositor integration.

## Performance Targets

- **Blur operation:** <1.5ms @ 1920×1080
- **IPC overhead:** <0.2ms
- **Total:** <1.7ms (10% of 16.67ms frame budget @ 60fps)

## Optimization Strategies

### 1. Minimize Texture Copies

**Bad:**
```c
// CPU-side copy (slow!)
glReadPixels(backdrop);
memcpy(shared_memory, cpu_buffer);
```

**Good:**
```c
// Zero-copy DMA-BUF (fast!)
wlr_buffer_get_dmabuf(backdrop, &dmabuf);
send_dmabuf_fd(daemon, dmabuf.fd);
```

**Speedup:** 800× faster (0.05ms vs 40ms)

### 2. Cache Blurred Results

For static content (panels, backgrounds):
```c
struct blur_cache {
    uint32_t surface_id;
    struct wlr_buffer *blurred;
    bool dirty;
};

struct wlr_buffer *get_cached_blur(struct wl_surface *surface) {
    struct blur_cache *cache = find_cache(surface);
    if (cache && !cache->dirty) {
        return cache->blurred;
    }

    // Request new blur
    struct wlr_buffer *blurred = blur_request(backdrop, preset);
    update_cache(surface, blurred);
    return blurred;
}
```

**Speedup:** ~20× faster for static content (0.05ms vs 1.2ms)

### 3. Skip Invisible Surfaces

```c
void render_surfaces(struct render_context *ctx) {
    for (struct surface *s in surfaces) {
        // Skip if not visible
        if (!is_visible(s, ctx->output)) {
            continue;
        }

        // Skip if fully occluded
        if (is_fully_occluded(s, surfaces)) {
            continue;
        }

        render_surface(ctx, s);
    }
}
```

**Speedup:** Varies (eliminates unnecessary blur requests)

### 4. Use Appropriate Presets

Different presets have different performance characteristics:

| Preset | Passes | Radius | Time @ 1080p |
|--------|--------|--------|--------------|
| tooltip | 1 | 2.0 | 0.4ms |
| panel | 2 | 4.0 | 0.8ms |
| window | 3 | 8.0 | 1.2ms |
| hud | 4 | 12.0 | 1.8ms |

**Recommendation:** Use lighter presets (tooltip, panel) where appropriate.

### 5. Damage Tracking

Only blur damaged regions:
```c
void render_with_damage(struct render_context *ctx) {
    pixman_region32_t damage;
    pixman_region32_init(&damage);

    // Calculate damage
    calculate_surface_damage(surface, &damage);

    if (pixman_region32_not_empty(&damage)) {
        // Only blur if damaged
        struct wlr_buffer *blurred = blur_request(backdrop, preset);
        render_texture(ctx, blurred, &damage);
    }
}
```

**Speedup:** ~2-10× for partial updates

### 6. Async Blur Requests

For future versions:
```c
// Send blur request (non-blocking)
blur_request_async(backdrop, preset, callback);

// Continue rendering other surfaces

// Callback when blur is ready
void blur_complete(struct wlr_buffer *blurred) {
    // Composite blurred texture
    render_texture(ctx, blurred);
}
```

**Benefit:** Hide IPC latency behind GPU work

### 7. Reduce Blur Quality for Low-End Hardware

Detect GPU capabilities and adjust:
```c
void configure_blur_quality(void) {
    if (is_integrated_gpu()) {
        // Lower quality for integrated GPUs
        set_default_preset("panel");  // 2 passes instead of 3
    } else {
        // Higher quality for discrete GPUs
        set_default_preset("window");  // 3 passes
    }
}
```

### 8. Profile Your Integration

Use timing measurements:
```c
#include <time.h>

double measure_blur_time(void) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    struct wlr_buffer *blurred = blur_request(backdrop, preset);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                (end.tv_nsec - start.tv_nsec) / 1000000.0;
    return ms;
}
```

**Target:** <1.7ms total (blur + IPC)

## Common Performance Issues

### Issue 1: High Frame Time

**Symptom:** FPS drops when blur is enabled

**Debug:**
```bash
# Check blur timing
wlblurd --log-level debug

# Profile compositor
perf record -g ./your_compositor
perf report
```

**Solutions:**
- Reduce blur passes
- Use smaller radius
- Cache blurred results
- Skip invisible surfaces

### Issue 2: High Memory Usage

**Symptom:** Memory increases with number of blurred surfaces

**Debug:**
```bash
# Check process memory
ps aux | grep wlblurd
top -p $(pgrep wlblurd)
```

**Solutions:**
- Limit number of cached blur textures
- Clean up blur nodes when surfaces destroyed
- Use texture pooling

### Issue 3: GPU Hangs

**Symptom:** GPU resets, compositor freezes

**Debug:**
```bash
# Check kernel logs
dmesg | grep -i gpu

# Check Mesa logs
MESA_DEBUG=1 ./your_compositor
```

**Solutions:**
- Validate DMA-BUF format/modifier compatibility
- Add GPU error handling
- Reduce blur complexity

## Benchmarking

### Test Setup
```bash
# 1920×1080 window
# 3-pass Kawase blur
# Mid-range GPU (Intel UHD 620)

# Expected times:
# Blur: 1.2ms
# IPC: 0.2ms
# Total: 1.4ms
```

### Measurement Tool
```bash
# Use built-in profiling
wlblurd --profile

# Or external profiler
perf stat -e cycles,instructions ./your_compositor
```

## See Also

- [ADR-002: DMA-BUF](../Architecture-Decisions/ADR-002-DMA-BUF) - Zero-copy architecture
- [ADR-003: Kawase Algorithm](../Architecture-Decisions/ADR-003-Kawase-Algorithm) - Algorithm performance
- [Integration Checklist](Integration-Checklist) - Implementation steps
- [Troubleshooting](../User-Guide/Troubleshooting) - Performance issues
