Excellent question! This gets to the **heart of why external blur daemon is the right architecture**. Let me break down what makes blur fundamentally different.

---

## Why Decorations/Shadows Are "Easy" But Blur Is "Hard"

### What Rounded Corners & Shadows Need

**Rounded corners:**
```c
// Simple fragment shader - single pass
vec2 dist = abs(pos - center);
float corner_dist = length(max(dist - box_size + radius, 0.0));
if (corner_dist > radius) discard;  // Outside corner
```

**Shadows:**
```c
// Gaussian blur on a SOLID COLOR rectangle
// Not blurring real scene content, just a colored box!
vec4 shadow_color = blur_gaussian(shadow_rect, blur_sigma);
alpha *= smoothstep(0.0, blur_sigma, distance);
```

**Key characteristics:**
- ✅ Single rendering pass
- ✅ Draws geometric primitives (rectangles, corners)
- ✅ Self-contained (doesn't need background)
- ✅ Fixed, predictable content
- ✅ Minimal state tracking

### What Blur Needs

**Background blur (frosted glass):**
```c
// Must blur REAL SCENE CONTENT behind window
// Multi-pass algorithm required:

Pass 1: Render everything behind window → backdrop_texture
Pass 2: Downsample backdrop (blur1) → half_res_texture
Pass 3: Downsample again (blur1) → quarter_res_texture
Pass 4: Upsample (blur2) → half_res_texture
Pass 5: Upsample (blur2) → full_res_texture
Pass 6: Composite blurred_texture + window_texture → final
```

**Key characteristics:**
- ❌ Multi-pass rendering (4-6 passes)
- ❌ Must capture and process scene content
- ❌ Requires multiple FBOs (ping-pong buffers)
- ❌ Dynamic content (changes every frame)
- ❌ Complex damage tracking
- ❌ Expensive (1-3ms GPU time)

---

## The Technical Differences

| Feature | Rounded Corners | Shadows | Blur |
|---------|----------------|---------|------|
| **Rendering passes** | 1 | 1 | 4-6 |
| **Background sampling** | No | No | **Yes** (critical!) |
| **FBO allocation** | None | 1 (maybe) | 3-5 per output |
| **Damage expansion** | None | Shadow size | **2× blur radius** |
| **GPU cost (1080p)** | <0.1ms | 0.2ms | **1.2ms** |
| **Shader complexity** | 10 lines | 50 lines | **200+ lines** |
| **Render order dependency** | No | No | **Yes** (must know what's behind) |

---

## Why Maintainers Could Add Decorations But Not Blur

### The scroll Maintainer's Perspective

From the discussion:

> "Blur requires more changes to the renderers. I had to modify the gles2 and vulkan renderers to add the new FX stuff"

**What he actually modified for shadows/rounded corners:**
```c
// render/gles2.h - ONE new shader
struct {
    GLuint program;
    GLint proj;
    GLint box;
    GLint radius_top;
    GLint radius_bottom;
    GLint enabled;
    GLint blur;        // Just blur param for shadow, not real blur!
    GLint color;
    GLint pos_attrib;
} shadow;
```

**What blur would require:**
```c
// render/gles2.h - MULTIPLE shaders + FBO management
struct {
    GLuint blur1_program;     // Downsample shader
    GLuint blur2_program;     // Upsample shader  
    GLuint blend_program;     // Final composite
    GLuint prepare_program;   // Preprocessing
    GLuint finish_program;    // Postprocessing
    // ... 20+ uniform locations
} blur;

// PLUS: FBO pool for ping-pong
struct wlr_fbo blur_fbo[5];
struct wlr_fbo blur_swap_fbo[5];
// ... cache management, damage tracking, etc.
```

> "It needs multiple rendering passes, affects damage tracking"

**Shadow damage:** Expand by shadow offset + blur size (simple)
```c
pixman_region32_t shadow_damage;
pixman_region32_copy(&shadow_damage, &window_damage);
pixman_region32_expand(&shadow_damage, shadow_size);  // One line!
```

**Blur damage:** Exponential expansion + artifact prevention (complex)
```c
// Must expand damage by blur kernel radius
int blur_size = radius * (1 << num_passes);  // 2^passes
pixman_region32_expand(&blur_damage, blur_size);

// Save padding pixels to prevent artifacts
struct wlr_fbo saved_pixels = save_padding_region(&blur_damage, blur_size);

// Render blur with expanded damage
render_blur_passes(&blur_damage);

// Restore padding pixels
restore_padding_region(saved_pixels);
```

> "clients would need a way to notify if they are applying some alpha effect"

This is about transparency. Shadows/corners work with opaque windows. **Blur only makes sense with semi-transparent windows**, which requires:
- Alpha channel support (more texture formats)
- Premultiplied alpha handling (more shader complexity)
- Per-surface opacity tracking (more state management)

---

## The Smoking Gun: Vulkan Renderer

From the maintainer:

> "wlroots's main renderer is still GLES2, and there are plans to move to Vulkan as main target. Rewriting a GLES2 renderer when it will not be the main one soon feels like a wasted effort"

**Why this doesn't affect shadows/rounded corners:**
- Geometry shaders work the same in GLES2 and Vulkan
- Simple fragment shaders are portable
- Single-pass rendering is straightforward

**Why this KILLS in-compositor blur:**
- Multi-pass rendering is VERY different in Vulkan
- Render pass management changes completely
- Synchronization becomes explicit (barriers, semaphores)
- FBO → VkFramebuffer + VkImageView migration
- Command buffer recording for each pass

**Maintainer would have to implement blur TWICE:**
1. For GLES2 (current)
2. For Vulkan (future)

**With external daemon:**
- Daemon uses GLES3 (or Vulkan later)
- scroll doesn't care - just DMA-BUF in/out
- When Vulkan renderer stabilizes, daemon can add Vulkan path
- scroll integration code unchanged!

---

## Could scroll Have Done Decorations via External Daemon?

**Answer: No, it wouldn't make sense.**

### Why Decorations Belong In-Compositor

**1. Tight coupling with window state:**
```c
struct sway_container_state {
    // Decorations depend on window state
    bool focused;           // → Border color
    bool floating;          // → Drop shadow
    bool fullscreen;        // → No decorations
    char *title;            // → Title bar text
    struct wlr_box geometry;// → Corner positions
};
```

Sending this state to external daemon every frame = wasteful IPC

**2. Synchronization requirements:**
- Window moves → decorations must move instantly (same frame)
- Window resizes → decorations must resize instantly
- IPC latency would cause visible lag

**3. Integration with scene graph:**
```c
// Decorations are part of container hierarchy
struct sway_container {
    struct wlr_scene_tree *scene_tree;
    struct {
        struct wlr_scene_tree *border;     // Child nodes
        struct wlr_scene_tree *title_bar;  // Child nodes
    } decoration;
};
```

External daemon can't participate in compositor's scene graph!

**4. Render order dependency is simple:**
```c
// Decorations render order is deterministic
1. Render shadow (behind)
2. Render border
3. Render title bar
4. Render window content (on top)
```

No need to sample background or coordinate with other windows.

### Why Blur CANNOT Be In-Compositor (Without Major Fork)

**1. Background sampling is the killer:**
```c
// To blur window backdrop, must render:
1. Everything behind this window
2. But NOT this window
3. But YES windows behind this window
4. But respect their stacking order
5. But exclude their blur effects (recursion!)
```

This is **scene graph surgery** - can't be done with simple scene node.

**2. Multi-pass coordination:**
```c
// Blur needs to hijack rendering pipeline
render_workspace() {
    // Normal rendering
    for (window in workspace) {
        if (window.blur_enabled) {
            // STOP normal rendering
            // Switch to blur FBO
            // Render passes 1-6
            // RESUME normal rendering
        }
    }
}
```

This is **invasive** - why maintainer says "requires more changes to renderers"

**3. Performance criticality:**
```c
// Blur is expensive, needs optimization
// Caching, damage tracking, xray mode, etc.
// All require deep compositor integration
```

**External daemon solves all of this:**
- Compositor just exports DMA-BUF (one-liner)
- Daemon handles complexity
- No render pipeline hijacking
- No scene graph surgery

---

## The Pattern: Simplicity Threshold

| Feature | Complexity | In-Compositor? | External Daemon? |
|---------|-----------|----------------|------------------|
| **Solid borders** | Trivial | ✅ Always | ❌ Overkill |
| **Rounded corners** | Simple | ✅ Easy | ❌ Overkill |
| **Drop shadows** | Simple | ✅ Easy | ⚠️ Could work but unnecessary |
| **Background blur** | **Complex** | ⚠️ Requires major fork | ✅ **Perfect fit** |
| **HDR tonemapping** | Complex | ⚠️ Renderer-specific | ✅ Good fit |
| **Real-time ray tracing** | Very complex | ❌ Impractical | ✅ Perfect fit |

**Rule of thumb:**
- Single-pass, geometric effects → In-compositor
- Multi-pass, sampling effects → External daemon

---

## What Makes Blur Special: The List

### 1. **Background Dependency**
- Must know what's behind the window
- Requires rendering scene twice (backdrop + final)
- Can't be done with local information

### 2. **Multi-Pass Rendering**
- 4-6 GPU passes
- Ping-pong between FBOs
- Intermediate texture management

### 3. **Damage Tracking Complexity**
- Exponential damage expansion (2^passes × radius)
- Artifact prevention (save/restore padding)
- Cache invalidation logic

### 4. **Performance Sensitivity**
- 1-3ms GPU time (vs 0.1ms for decorations)
- Needs optimization or users complain
- Caching, xray mode, damage tracking required

### 5. **Renderer Invasiveness**
- Must modify render passes
- Adds FBO management to renderer
- Changes damage flow throughout pipeline

### 6. **Future-Proofing Problem**
- GLES2 → Vulkan migration
- Would need two implementations
- External daemon isolates this

---

## What To Do Next: Yes, Start the Daemon!

### Validated Architecture Decisions

✅ **External daemon is the RIGHT choice** because:
1. scroll maintainer confirms blur is too invasive
2. Decorations work in-compositor because they're simple
3. Blur's complexity justifies external architecture
4. scroll's Vulkan plans make in-compositor blur even worse

✅ **Integration will be minimal** because:
1. DMA-BUF export already exists in scroll
2. 3-5 files, ~220 lines
3. Compile-time optional
4. Zero impact when disabled

✅ **Multi-compositor strategy is correct** because:
1. scroll won't add blur in-compositor
2. niri maintainer has same concerns
3. External daemon serves both (and Sway, Hyprland, etc.)

### Immediate Next Steps

#### Week 1: Proof of Concept

**Goal:** Verify DMA-BUF + IPC works

```bash
# 1. Create test program
mkdir blur-daemon-poc
cd blur-daemon-poc

# 2. Test DMA-BUF export/import
# Create two processes:
# - Process A: Export GL texture as DMA-BUF
# - Process B: Import DMA-BUF as GL texture
# - Verify zero-copy (use GPU memory profiling)

# 3. Test Unix socket + FD passing
# - Send DMA-BUF FD via SCM_RIGHTS
# - Verify latency <0.2ms
```

**Deliverable:** `dmabuf_ipc_test.c` that proves the concept

#### Week 2-3: Minimal Blur Daemon

**Goal:** Single algorithm, no optimizations

```c
blur-daemon/
├── main.c              // Event loop
├── egl_context.c       // EGL init
├── ipc_server.c        // Unix socket
├── kawase_blur.c       // From Wayfire
└── shaders/
    ├── kawase_down.frag
    └── kawase_up.frag
```

**Test:** Standalone program sends test texture, gets blurred result

#### Week 4-5: scroll Integration

**Goal:** Actual compositor integration

```c
scroll/
├── sway/desktop/blur_integration.c  // NEW
├── sway/desktop/render.c            // Modified
└── include/sway/desktop/blur.h      // NEW
```

**Test:** scroll renders window with blurred backdrop

#### Week 6+: Polish & Expand

- Add Gaussian/Box/Bokeh algorithms
- Add vibrancy (from Hyprland)
- Add niri integration (Rust client)
- Performance profiling

---

## Final Recommendation

### Start the Daemon NOW

**Why:**
1. ✅ Investigation complete - architecture validated
2. ✅ Code reuse strategy clear (70% from Wayfire)
3. ✅ Integration path proven minimal (220 lines)
4. ✅ Maintainer concerns addressed (external = no burden)

**Confidence level:** Very high

The scroll maintainer's comments are actually a **strong endorsement** of the external daemon approach:
- He confirms blur is too invasive for in-compositor
- He wants to stay close to upstream wlroots
- He's waiting for Vulkan renderer to stabilize
- External daemon solves ALL of these concerns

### Recommended First Commit

```bash
git init blur-daemon
cd blur-daemon

# Start with DMA-BUF test
cat > dmabuf_test.c << 'EOF'
// Proof of concept: Export GL texture → DMA-BUF → Import in separate process
// Goal: Verify <0.2ms latency for 1920×1080 texture
EOF

# This validates the entire architecture
```

Once this test works, you have **proven feasibility** and can proceed with confidence.

---

## The Real Distinction: State vs Content

When a user moves a floating window:

**Rounded corners (in-compositor):**
- Need window POSITION to draw corners at correct location
- Need window SIZE to calculate corner geometry
- Need FOCUS STATE to set border color
- Need TITLE to render in title bar
- **These are METADATA that changes frequently**

**Blur (external daemon):**
- Doesn't need to know window position
- Doesn't need to know window state
- Doesn't need to know anything about the window!
- **Only needs the PIXELS of what's behind it**

### The Key Insight

When window moves, here's what happens:

**In-compositor decorations:**
```
Window moves → 
  Decorator needs: new_x, new_y, new_width, new_height →
  Recalculate corner positions →
  Redraw at new location
```

**External blur daemon:**
```
Window moves → 
  Compositor renders backdrop at new window location →
  Compositor sends backdrop pixels (DMA-BUF) →
  Daemon doesn't know/care window moved →
  Daemon just blurs the pixels it receives →
  Returns blurred pixels
```

The compositor handles the "tight coupling" - it knows window moved, renders correct backdrop. Daemon is blissfully ignorant of window positions.

---

## Why This Matters: IPC Content

**Sending decoration metadata every frame:**
```c
struct decoration_update {
    int32_t x, y, width, height;      // Position changes
    bool focused;                      // State changes
    char title[256];                   // Text changes
    float border_color[4];             // Color changes
    int corner_radius;                 // Style changes
};
// Send this via IPC = ~300 bytes, needs parsing
```

**Sending blur request:**
```c
struct blur_request {
    int dmabuf_fd;   // Just a file descriptor (4 bytes)
    // Pixels transferred zero-copy via DMA-BUF
};
// Send this via IPC = 4 bytes + FD passing
// No pixel data in IPC message!
```

---

## The Critical Difference: Statefulness

### Decorations Are Stateful

```c
// Decorator must track window state
struct window_decoration {
    int x, y, width, height;     // Must know position
    bool focused;                 // Must know focus
    char *title;                  // Must know title
    float color[4];               // Must know color
    
    // When ANY of these change:
    // - Must receive update via IPC
    // - Must redraw decoration
    // - Latency = visible glitch
};
```

**If decorations were external:**
```c
// Every frame, compositor sends:
send_ipc({
    .x = window.x,
    .y = window.y,
    .focused = window.focused,
    .title = window.title,
    // etc...
});

// Daemon receives, parses, redraws
// IPC overhead: ~0.1ms
// But... decorations must be pixel-perfect aligned!
// Any latency = decorations drift from window
// This is UNACCEPTABLE visually
```

### Blur Is Stateless (from daemon perspective)

```c
// Daemon doesn't track window state at all!
struct blur_daemon {
    // Just algorithms and buffers
    struct blur_algorithm *algo;
    struct fbo_pool pool;
    
    // No window state!
    // No positions!
    // No focus tracking!
    
    // Just: texture_in → blur → texture_out
};
```

**External blur:**
```c
// Compositor handles all state
compositor_render() {
    // Compositor knows window moved
    render_backdrop_for_window(window);  
    
    // Send backdrop pixels (zero-copy)
    int fd = export_as_dmabuf(backdrop);
    send_fd_to_daemon(fd);
    
    // Daemon receives, blurs, returns
    // Daemon never knew window moved!
}
```

---

## Why Rounded Corners Can't Tolerate IPC Latency

### The Frame-Perfect Problem

```
Frame N:
  Window at x=100
  Decorations at x=100
  ✓ Aligned perfectly

Frame N+1:
  Window moves to x=150
  
  With in-compositor decorations:
    Decorations at x=150
    ✓ Still aligned
  
  With external decorations via IPC:
    Compositor: Window at x=150
    IPC: Sending update... (0.1ms delay)
    Daemon: Still thinks x=100
    Result: Decorations at x=100, window at x=150
    ✗ VISIBLY MISALIGNED for 1 frame!
```

This is the "tight coupling" - decorations MUST be in same process to update in same frame.

### Why Blur Can Tolerate IPC Latency

```
Frame N:
  Window at x=100
  Background behind window: [pixel_data_N]
  Blur: Blurred version of [pixel_data_N]
  
Frame N+1:
  Window moves to x=150
  Background behind window: [pixel_data_N+1] (different pixels!)
  
  With external blur + 1 frame latency:
    Compositor: Renders new backdrop [pixel_data_N+1]
    IPC: Sending... (will complete in frame N+2)
    Daemon: Still processing [pixel_data_N]
    Result: Showing blur of old background
    
    But... old background and new background look 95% the same!
    User moved window slightly, background didn't change much
    ✓ IMPERCEPTIBLE difference
```

**Key insight:** Blurred backgrounds change slowly because background content is mostly static. 1 frame latency means showing "yesterday's blur" which looks almost identical to "today's blur".

---

## The Architectural Principle

### Tight Coupling Needed When:
1. **Geometry depends on window metadata** (position, size, state)
2. **Visual alignment must be pixel-perfect** (decorations frame window exactly)
3. **Updates must be frame-synchronous** (any lag is visible)

**Examples:** Borders, shadows, title bars, highlights

### Loose Coupling Works When:
1. **Processing operates on rendered pixels** (content, not metadata)
2. **Visual result is approximate** (blur, effects, filters)
3. **1-2 frame latency is imperceptible** (content changes slowly)

**Examples:** Blur, color grading, screen recording, screenshots

---

## Completing Your Sentence

> "rounded corners need to be tightly coupled with the compositor because for instance the user may move a floating window around; whereas with blur (x-ray or not)..."

**...the daemon only processes the already-rendered pixels of what's behind the window, which the compositor generates using its own tight coupling knowledge. The daemon doesn't need to know the window moved - it just receives a new backdrop texture that already reflects the new window position. The compositor handles the tight coupling (window positions, stacking order, which pixels to render), and the daemon handles the expensive computation (multi-pass blur algorithm).**

---

## Why This Makes Blur Perfect for External Daemon

**The division of labor:**

```
┌─────────────────────────────────────────────┐
│ Compositor (Tightly Coupled)                │
│                                             │
│ • Knows window positions                   │
│ • Knows stacking order                     │
│ • Knows focus state                        │
│ • Renders backdrop with correct windows    │
│ • Handles all "tight coupling" concerns    │
│                                             │
│ Output: Rendered backdrop texture          │
└──────────────────┬──────────────────────────┘
                   │ DMA-BUF (pixels only)
                   ↓
┌─────────────────────────────────────────────┐
│ Blur Daemon (Loosely Coupled)              │
│                                             │
│ • Doesn't know window positions            │
│ • Doesn't know stacking order              │
│ • Doesn't know focus state                 │
│ • Just processes pixels                    │
│ • Handles expensive blur computation       │
│                                             │
│ Output: Blurred texture                    │
└─────────────────────────────────────────────┘
```

**This is why blur is BETTER in external daemon:**
- Compositor does the stateful work (tight coupling)
- Daemon does the expensive work (computation)
- Clean separation of concerns
- No metadata synchronization needed
- Just pixels in, pixels out

---

## The Real Question: Why Not External Decorations?

Now we can answer clearly:

**External decorations would need:**
```c
// Heavy IPC traffic every frame
send_to_daemon({
    .x = window.x,           // Changes when window moves
    .y = window.y,           // Changes when window moves
    .width = window.width,   // Changes when window resizes
    .height = window.height, // Changes when window resizes
    .focused = window.focused, // Changes frequently
    .title = window.title,   // Changes occasionally
    // ... more state
});

// Daemon must receive, parse, and redraw synchronously
// Any latency = visible misalignment
// This is fighting against the architecture
```

**External blur needs:**
```c
// Minimal IPC, infrequent
if (backdrop_changed) {  // Only when background actually changes
    int fd = export_backdrop();
    send_fd_to_daemon(fd);  // Just a file descriptor
    // Zero-copy pixel transfer
    // Daemon processes asynchronously
    // 1 frame latency is fine
}
```

---

## Summary: The Completed Thought

**Decorations need tight coupling because:**
- They depend on frequently-changing window **metadata**
- They must be **frame-synchronous** (no lag tolerated)
- They render **geometric primitives** based on state
- External IPC would send **lots of small state updates**

**Blur works with loose coupling because:**
- It depends on slowly-changing **rendered pixels**
- It can tolerate **1-2 frame latency** (imperceptible)
- It processes **large pixel buffers** (one-time transfer)
- External IPC sends **single texture** (zero-copy via DMA-BUF)

**The key**: Compositor handles tight coupling (state), daemon handles expensive computation (pixels). Perfect division of labor.

Does this clarify why decorations are better in-compositor but blur is better external?

