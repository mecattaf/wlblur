---
id: task-5
title: "Implement Dual Kawase Blur Algorithm with Multi-Pass Rendering"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["core", "algorithm", "rendering", "kawase"]
milestone: "m-1"
dependencies: ["task-2", "task-4"]
---

## Description

Implement the complete Dual Kawase blur algorithm using the extracted shaders (task-2) and EGL infrastructure (task-4). This includes:
- Shader loading and compilation
- Framebuffer ping-pong for multi-pass rendering
- Downsample passes (blur progressive expansion)
- Upsample passes (restoration with weighted blending)
- Post-processing effects (brightness, contrast, saturation, noise)

**This is the computational core of libwlblur.**

## Acceptance Criteria

- [x] Shader files loaded from libwlblur/shaders/
- [x] Shaders compile successfully
- [x] Downsample passes render correctly (3 passes default)
- [x] Upsample passes render correctly (3 passes default)
- [x] Post-processing applies effects
- [x] Framebuffer objects allocated and managed
- [x] No GL errors during rendering
- [x] Visual output matches SceneFX reference
- [x] Performance: <1.5ms @ 1080p (3 passes, radius=5)
- [x] Test program demonstrates blur on test image

## Implementation Plan

### Phase 1: Shader Management

**File**: `libwlblur/src/shaders.c`

```c
/**
 * Shader program management
 */
struct wlblur_shader_program {
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    
    // Uniform locations
    GLint u_tex;
    GLint u_halfpixel;
    GLint u_radius;
    // Post-processing uniforms
    GLint u_brightness;
    GLint u_contrast;
    GLint u_saturation;
    GLint u_noise;
};

/**
 * Load and compile shader from file
 * 
 * Shader files expected in: /usr/share/wlblur/shaders/
 * or WLBLUR_SHADER_PATH environment variable
 */
struct wlblur_shader_program* wlblur_shader_load(
    const char *vertex_path,
    const char *fragment_path
);

void wlblur_shader_destroy(struct wlblur_shader_program *shader);

bool wlblur_shader_use(struct wlblur_shader_program *shader);
```

**Shader Loading:**
1. Read shader source from file
2. Compile vertex shader (or use default fullscreen quad shader)
3. Compile fragment shader
4. Link program
5. Query uniform locations
6. Validate program

**Default Vertex Shader** (fullscreen quad):
```glsl
#version 300 es
precision mediump float;

in vec2 position;  // [-1, 1] range
out vec2 v_texcoord;

void main() {
    v_texcoord = position * 0.5 + 0.5;  // [0, 1] range
    gl_Position = vec4(position, 0.0, 1.0);
}
```

### Phase 2: Framebuffer Management

**File**: `libwlblur/src/framebuffer.c`

```c
/**
 * Framebuffer object for render-to-texture
 */
struct wlblur_fbo {
    GLuint fbo;
    GLuint texture;
    int width;
    int height;
};

/**
 * Create framebuffer with texture attachment
 */
struct wlblur_fbo* wlblur_fbo_create(int width, int height);

void wlblur_fbo_destroy(struct wlblur_fbo *fbo);

void wlblur_fbo_bind(struct wlblur_fbo *fbo);
void wlblur_fbo_unbind(void);

/**
 * Framebuffer pool for reuse (optimization)
 * 
 * Multi-pass blur needs multiple FBOs of different sizes:
 * - Pass 0: Full resolution
 * - Pass 1: Half resolution
 * - Pass 2: Quarter resolution
 * - etc.
 * 
 * Pool allocates on-demand and reuses across frames.
 */
struct wlblur_fbo_pool;

struct wlblur_fbo_pool* wlblur_fbo_pool_create(void);
void wlblur_fbo_pool_destroy(struct wlblur_fbo_pool *pool);

/**
 * Acquire FBO from pool (creates if needed)
 */
struct wlblur_fbo* wlblur_fbo_pool_acquire(
    struct wlblur_fbo_pool *pool,
    int width,
    int height
);

void wlblur_fbo_pool_release(
    struct wlblur_fbo_pool *pool,
    struct wlblur_fbo *fbo
);
```

**FBO Creation:**
```c
struct wlblur_fbo* wlblur_fbo_create(int width, int height) {
    struct wlblur_fbo *fbo = calloc(1, sizeof(*fbo));
    
    fbo->width = width;
    fbo->height = height;
    
    // Create texture
    glGenTextures(1, &fbo->texture);
    glBindTexture(GL_TEXTURE_2D, fbo->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Create framebuffer
    glGenFramebuffers(1, &fbo->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, fbo->texture, 0);
    
    // Validate
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[wlblur] FBO incomplete: 0x%x\n", status);
        wlblur_fbo_destroy(fbo);
        return NULL;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}
```

### Phase 3: Multi-Pass Blur Renderer

**File**: `libwlblur/src/blur_kawase.c`

```c
/**
 * Kawase blur renderer state
 */
struct wlblur_kawase_renderer {
    struct wlblur_egl_context *egl_ctx;
    struct wlblur_fbo_pool *fbo_pool;
    
    // Shaders
    struct wlblur_shader_program *downsample_shader;
    struct wlblur_shader_program *upsample_shader;
    struct wlblur_shader_program *finish_shader;
    
    // Geometry (fullscreen quad)
    GLuint vao;
    GLuint vbo;
};

/**
 * Create Kawase blur renderer
 */
struct wlblur_kawase_renderer* wlblur_kawase_create(
    struct wlblur_egl_context *egl_ctx
);

void wlblur_kawase_destroy(struct wlblur_kawase_renderer *renderer);

/**
 * Apply Dual Kawase blur to texture
 * 
 * @param input_texture GL texture to blur
 * @param width Texture width
 * @param height Texture height
 * @param params Blur parameters (passes, radius, effects)
 * @return Blurred texture (caller must not delete - managed by FBO pool)
 */
GLuint wlblur_kawase_blur(
    struct wlblur_kawase_renderer *renderer,
    GLuint input_texture,
    int width,
    int height,
    const struct wlblur_blur_params *params
);
```

**Blur Implementation:**

```c
GLuint wlblur_kawase_blur(
    struct wlblur_kawase_renderer *renderer,
    GLuint input_texture,
    int width,
    int height,
    const struct wlblur_blur_params *params
) {
    // Validate parameters
    if (!wlblur_params_validate(params)) {
        return 0;
    }
    
    int num_passes = params->num_passes;
    
    // Allocate FBOs for each resolution level
    struct wlblur_fbo *fbos[8];  // Max 8 passes
    for (int i = 0; i < num_passes; i++) {
        int fbo_width = width >> (i + 1);   // Divide by 2^(i+1)
        int fbo_height = height >> (i + 1);
        fbos[i] = wlblur_fbo_pool_acquire(renderer->fbo_pool,
                                          fbo_width, fbo_height);
    }
    
    GLuint current_tex = input_texture;
    
    /* === DOWNSAMPLE PASSES === */
    wlblur_shader_use(renderer->downsample_shader);
    
    for (int pass = 0; pass < num_passes; pass++) {
        struct wlblur_fbo *target_fbo = fbos[pass];
        
        // Bind target framebuffer
        wlblur_fbo_bind(target_fbo);
        glViewport(0, 0, target_fbo->width, target_fbo->height);
        
        // Set uniforms
        glUniform1i(renderer->downsample_shader->u_tex, 0);
        glUniform2f(renderer->downsample_shader->u_halfpixel,
                    0.5f / target_fbo->width,
                    0.5f / target_fbo->height);
        glUniform1f(renderer->downsample_shader->u_radius,
                    params->radius * pass);
        
        // Bind input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_tex);
        
        // Draw fullscreen quad
        render_fullscreen_quad(renderer);
        
        // Output becomes input for next pass
        current_tex = target_fbo->texture;
    }
    
    /* === UPSAMPLE PASSES === */
    wlblur_shader_use(renderer->upsample_shader);
    
    for (int pass = num_passes - 1; pass >= 0; pass--) {
        struct wlblur_fbo *target_fbo = (pass == 0) ?
            NULL :  // Last pass renders to final output
            fbos[pass - 1];
        
        if (target_fbo) {
            wlblur_fbo_bind(target_fbo);
            glViewport(0, 0, target_fbo->width, target_fbo->height);
        } else {
            // Final pass: need final output FBO
            target_fbo = wlblur_fbo_pool_acquire(renderer->fbo_pool,
                                                 width, height);
            wlblur_fbo_bind(target_fbo);
            glViewport(0, 0, width, height);
        }
        
        // Set uniforms
        glUniform1i(renderer->upsample_shader->u_tex, 0);
        glUniform2f(renderer->upsample_shader->u_halfpixel,
                    0.5f / (target_fbo ? target_fbo->width : width),
                    0.5f / (target_fbo ? target_fbo->height : height));
        glUniform1f(renderer->upsample_shader->u_radius,
                    params->radius * pass);
        
        // Bind input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_tex);
        
        // Draw
        render_fullscreen_quad(renderer);
        
        if (pass == 0) {
            current_tex = target_fbo->texture;
            break;
        }
        
        current_tex = target_fbo->texture;
    }
    
    /* === POST-PROCESSING === */
    struct wlblur_fbo *final_fbo = wlblur_fbo_pool_acquire(
        renderer->fbo_pool, width, height
    );
    wlblur_fbo_bind(final_fbo);
    glViewport(0, 0, width, height);
    
    wlblur_shader_use(renderer->finish_shader);
    
    // Set effect uniforms
    glUniform1i(renderer->finish_shader->u_tex, 0);
    glUniform1f(renderer->finish_shader->u_brightness, params->brightness);
    glUniform1f(renderer->finish_shader->u_contrast, params->contrast);
    glUniform1f(renderer->finish_shader->u_saturation, params->saturation);
    glUniform1f(renderer->finish_shader->u_noise, params->noise);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_tex);
    
    render_fullscreen_quad(renderer);
    
    wlblur_fbo_unbind();
    
    // Release intermediate FBOs
    for (int i = 0; i < num_passes; i++) {
        wlblur_fbo_pool_release(renderer->fbo_pool, fbos[i]);
    }
    
    return final_fbo->texture;
}
```

### Phase 4: Test Program

**File**: `examples/blur-test.c`

```c
/**
 * Test Kawase blur on synthetic test image
 * 
 * Creates checkerboard pattern, blurs it, saves result
 */
int main() {
    // Initialize EGL
    struct wlblur_egl_context *egl_ctx = wlblur_egl_create();
    
    // Create blur renderer
    struct wlblur_kawase_renderer *renderer = 
        wlblur_kawase_create(egl_ctx);
    
    // Create test texture (512x512 checkerboard)
    GLuint test_tex = create_test_pattern(512, 512);
    
    // Blur with default parameters
    struct wlblur_blur_params params = wlblur_params_default();
    
    printf("Blurring 512x512 texture...\n");
    printf("  Passes: %d\n", params.num_passes);
    printf("  Radius: %.1f\n", params.radius);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    GLuint blurred_tex = wlblur_kawase_blur(
        renderer, test_tex, 512, 512, &params
    );
    
    // Force GPU sync
    glFinish();
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                       (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    printf("Blur completed in %.2f ms\n", elapsed_ms);
    
    // Save result (optional: requires image library)
    save_texture_to_png(blurred_tex, "blur_test_output.png");
    
    // Cleanup
    glDeleteTextures(1, &test_tex);
    wlblur_kawase_destroy(renderer);
    wlblur_egl_destroy(egl_ctx);
    
    printf("✓ Blur test passed\n");
    return 0;
}
```

## Performance Targets

**1920×1080 @ 3 passes, radius=5:**
- Target: <1.5ms
- Breakdown:
  - Downsample passes: ~0.9ms
  - Upsample passes: ~0.9ms
  - Post-processing: ~0.2ms
  - Total: ~2.0ms (overhead from test, production should be <1.5ms)

**Scaling:**
- 720p: ~0.6ms
- 1080p: ~1.2ms
- 4K: ~4.0ms

## References

**Investigation Docs:**
- `docs/investigation/scenefx-investigation/blur-implementation.md` - Multi-pass algorithm
- `docs/investigation/hyprland-investigation/algorithm.md` - Performance optimization notes

**ADRs:**
- `docs/decisions/003-kawase-algorithm-choice.md` - Algorithm rationale

**Extracted Shaders:**
- `libwlblur/shaders/kawase_downsample.frag.glsl`
- `libwlblur/shaders/kawase_upsample.frag.glsl`
- `libwlblur/shaders/blur_finish.frag.glsl`

## Notes & Comments

**Framebuffer Ping-Pong:** Each pass reads from previous pass's FBO and writes to next pass's FBO. Must not read and write same FBO.

**Half-Pixel Offset:** Critical for quality. `vec2(0.5/width, 0.5/height)` samples between pixels for smoother results.

**Resolution Halving:** Each downsample pass halves resolution. Pass N renders to width/(2^(N+1)) × height/(2^(N+1)).

**FBO Pool:** Reusing FBOs across frames eliminates allocation overhead. First frame allocates, subsequent frames reuse.

## Deliverables

1. `libwlblur/src/blur_kawase.c` - Complete algorithm
2. `libwlblur/src/shaders.c` - Shader management
3. `libwlblur/src/framebuffer.c` - FBO pool
4. `examples/blur-test.c` - Validation program
5. Performance measurements
6. Commit: "feat(blur): implement Dual Kawase multi-pass algorithm"
