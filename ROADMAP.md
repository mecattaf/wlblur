# wlblur Roadmap

**Vision:** Bring Apple-level blur effects to the entire Wayland ecosystem through a compositor-agnostic external daemon architecture.

**Current Status:** Project Setup Phase (Milestone 0)
**Target:** macOS Ventura/Sonoma visual parity by Week 24
**Architecture:** External IPC daemon + minimal compositor integration (~200 lines per compositor)

---

## Quick Overview

| Phase | Milestone | Timeline | Status | Goal |
|-------|-----------|----------|--------|------|
| **Foundation** | m-0 | Week 1 | 🔄 In Progress | Project setup, documentation |
| **Algorithm** | m-1 | Week 2 | 📋 Planned | Shader extraction |
| **Core Library** | m-2 | Weeks 3-5 | 📋 Planned | libwlblur implementation |
| **IPC Daemon** | m-3 | Weeks 6-8 | 📋 Planned | wlblurd daemon |
| **First Integration** | m-4 | Weeks 9-10 | 📋 Planned | ScrollWM integration |
| **Production** | m-5 | Weeks 11-12 | 📋 Planned | v1.0.0 release |
| **Tint & Vibrancy** | m-6 | Weeks 13-15 | 📋 Planned | Big Sur parity |
| **Materials** | m-7 | Weeks 16-18 | 📋 Planned | Ventura parity |
| **Multi-Compositor** | m-8 | Weeks 19-21 | 📋 Planned | niri integration |
| **Polish** | m-9 | Weeks 22-24 | 📋 Planned | Community features |

---

## Current Status

**Where We Are:**
- ✅ Investigation complete (Hyprland, Wayfire, SceneFX analyzed)
- ✅ Architecture decided (external daemon approach)
- ✅ Technical feasibility validated (DMA-BUF zero-copy proven)
- 🔄 Documentation in progress (README, ROADMAP, ADRs)
- 📋 Implementation not yet started

**What's Working:**
- Repository structure established
- Investigation docs comprehensive
- Clear architectural vision

**What's Next:**
- Complete Milestone 0 documentation
- Set up build system (Meson)
- Begin shader extraction (Milestone 1)

---

## Milestone 0: Project Setup & Documentation

**Timeline:** Week 1 (Current)
**Status:** 🔄 In Progress

### Objectives

Establish project foundation with professional documentation and clear architectural direction.

### Deliverables

- [x] Repository structure
- [x] Investigation documentation organized
- [ ] **README.md** - Compelling 2-minute introduction ([task-0.1](backlog/tasks/task-0.1.md))
- [ ] **ADRs** - Five architectural decision records ([task-0.2](backlog/tasks/task-0.2.md))
- [ ] **ROADMAP.md** - Complete project roadmap ([task-0.3](backlog/tasks/task-0.3.md))
- [ ] Meson build skeleton
- [ ] License file (MIT)

### Success Metrics

- **Documentation clarity:** Can a developer understand the project in 5 minutes?
- **Decision transparency:** Are architectural choices well-justified?
- **Community readiness:** Is the project ready for early contributors?

### Risk Factors

- **Low risk:** Purely documentation work
- **Time estimation:** May take 1-2 extra days for polish

### Resources

- Investigation docs: `docs/investigation/`, `docs/post-investigation/`
- Architecture reference: `target-repomap.md`
- Prior art: SceneFX, Hyprland, Wayfire README files

---

## Milestone 1: Shader Extraction & Algorithm Consolidation

**Timeline:** Week 2
**Status:** 📋 Planned
**Depends on:** m-0

### Objectives

Extract blur shaders from Wayfire, Hyprland, and SceneFX. Create unified parameter schema and consolidation documentation.

### Deliverables

- [ ] Kawase downsample shader (GLES 3.0) - from Wayfire/Hyprland
- [ ] Kawase upsample shader (GLES 3.0) - from Wayfire/Hyprland
- [ ] Vibrancy shader (HSL saturation) - from Hyprland
- [ ] Blur prepare shader (contrast/brightness) - from Hyprland
- [ ] Blur finish shader (noise overlay) - from Hyprland
- [ ] Shader consolidation documentation
- [ ] Unified parameter API design
- [ ] Algorithm comparison report

### Success Metrics

- **Shader completeness:** All needed shaders extracted and compilable
- **Code reuse:** >70% of Wayfire blur code directly portable
- **Documentation quality:** Parameter differences clearly documented

### Technical Details

**Shader Sources:**
- **Wayfire:** `plugins/blur/kawase.cpp` (~120 lines, highly reusable)
- **Hyprland:** `src/render/shaders/glsl/blur*.frag` (vibrancy, effects)
- **SceneFX:** `render/fx_renderer/fx_shaders/` (post-processing)

**Parameter Unification:**
```c
struct wlblur_params {
    float blur_radius;        // 10-80 (Wayfire: offset, Hyprland: size)
    uint32_t blur_passes;     // 1-8 (Wayfire: iterations, Hyprland: passes)
    float brightness;         // 0.5-2.0 (Hyprland only)
    float contrast;           // 0.5-2.0 (Hyprland only)
    float saturation;         // 0.0-2.0 (all compositors)
    float noise;              // 0.0-0.1 (Hyprland only)
    float vibrancy;           // 0.0-0.3 (Hyprland only)
};
```

### Risk Factors

- **Shader compatibility:** GLSL version differences (mitigated: targeting GLES 3.0)
- **Algorithm behavior:** Parameter mapping may need tuning
- **Licensing:** Need to verify GPL compatibility (mitigated: Wayfire is MIT)

### Resources

- Shader investigation: `docs/investigation/*/blur-algorithm.md`
- Algorithm analysis: `docs/post-investigation/comprehensive-synthesis1.md`

---

## Milestone 2: libwlblur Core Implementation

**Timeline:** Weeks 3-5 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-1

### Objectives

Implement the core blur library with EGL context management, DMA-BUF handling, and Kawase blur algorithm.

### Deliverables

- [ ] EGL context initialization (headless, independent)
- [ ] DMA-BUF import (FD → EGLImageKHR → GL texture)
- [ ] DMA-BUF export (GL texture → EGLImageKHR → FD)
- [ ] Framebuffer management (FBO pool, ping-pong rendering)
- [ ] Kawase blur multi-pass renderer
- [ ] Shader compilation and caching
- [ ] Public API (`wlblur.h`, `blur_params.h`, `dmabuf.h`)
- [ ] Test program: blur PNG images (proof of concept)
- [ ] Unit tests for core functions

### Success Metrics

- **Performance:** <1.5ms blur time for 1920×1080 @ 3 passes (mid-range GPU)
- **Quality:** Visual parity with Hyprland blur at equivalent settings
- **API usability:** Test program requires <50 lines of code
- **Memory efficiency:** <16MB memory overhead per blur context
- **Test coverage:** >80% code coverage on core algorithms

### Technical Architecture

```
libwlblur/
├── src/
│   ├── blur_context.c      # EGL context (eglGetDisplay, eglCreateContext)
│   ├── blur_kawase.c       # Multi-pass algorithm
│   ├── dmabuf.c            # Import/export via EGLImageKHR
│   ├── framebuffer.c       # FBO pool management
│   ├── shaders.c           # Compilation, uniform setting
│   └── egl_helpers.c       # EGL utilities
│
├── include/wlblur/
│   ├── wlblur.h            # Public API
│   ├── blur_params.h       # Parameter structs
│   └── dmabuf.h            # DMA-BUF helpers
│
└── shaders/
    ├── kawase_downsample.frag.glsl
    ├── kawase_upsample.frag.glsl
    └── common.glsl
```

**API Example:**
```c
// Initialize blur context
struct wlblur_context *ctx = wlblur_init();

// Import DMA-BUF
struct wlblur_buffer *input = wlblur_import_dmabuf(ctx, dmabuf_fd, width, height);

// Configure blur
struct wlblur_params params = {
    .blur_radius = 40.0,
    .blur_passes = 3,
    .saturation = 1.1,
};

// Render blur
struct wlblur_buffer *output = wlblur_render(ctx, input, &params);

// Export result
int output_fd = wlblur_export_dmabuf(ctx, output);
```

### Risk Factors

- **DMA-BUF complexity:** Import/export may have driver-specific quirks
  - *Mitigation:* Extensive testing on Intel/AMD/NVIDIA
  - *Fallback:* CPU-based blur if DMA-BUF unavailable
- **Performance variance:** GPU performance varies widely
  - *Mitigation:* Adaptive quality settings based on framerate
- **Memory leaks:** EGL resource lifecycle is complex
  - *Mitigation:* Valgrind testing, reference counting

### Resources

- DMA-BUF examples: `docs/investigation/scenefx-investigation/blur-implementation.md`
- Kawase algorithm: Wayfire `plugins/blur/kawase.cpp`
- EGL patterns: SceneFX `render/fx_renderer/fx_texture.c`

---

## Milestone 3: wlblurd IPC Daemon

**Timeline:** Weeks 6-8 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-2

### Objectives

Build the IPC daemon that wraps libwlblur and provides blur-as-a-service to compositors via Unix socket.

### Deliverables

- [ ] IPC protocol definition (`protocol.h`)
- [ ] Unix socket server (SOCK_SEQPACKET)
- [ ] FD passing implementation (SCM_RIGHTS for DMA-BUF)
- [ ] Blur node registry (virtual scene graph)
- [ ] Client state management (per-compositor tracking)
- [ ] Buffer lifecycle management
- [ ] Configuration system (JSON or INI)
- [ ] systemd unit file
- [ ] Logging and debugging infrastructure

### Success Metrics

- **IPC latency:** <0.2ms round-trip for blur request
- **Stability:** 24-hour stress test without crashes
- **Multi-client:** Handle 3+ compositors simultaneously
- **Resource cleanup:** No FD leaks after 10,000 requests
- **Error handling:** Graceful degradation on invalid requests

### IPC Protocol Design

```c
// Message header (32 bytes)
struct wlblur_ipc_header {
    uint32_t magic;        // 0x424C5552 ("BLUR")
    uint32_t version;      // Protocol version (1)
    uint32_t client_id;    // Client identifier
    uint32_t sequence;     // Request sequence number
    uint32_t opcode;       // Operation code
    uint32_t payload_size; // Payload size
    uint64_t timestamp;    // Request timestamp (ns)
};

// Operation codes
enum wlblur_opcode {
    WLBLUR_OP_CREATE_NODE = 1,    // Create blur node
    WLBLUR_OP_DESTROY_NODE = 2,   // Destroy blur node
    WLBLUR_OP_IMPORT_DMABUF = 3,  // Import DMA-BUF
    WLBLUR_OP_RELEASE_BUFFER = 4, // Release buffer
    WLBLUR_OP_RENDER = 5,         // Render blur
    WLBLUR_OP_CONFIGURE = 6,      // Update parameters
};

// Render request
struct wlblur_render_request {
    struct wlblur_ipc_header header;
    uint32_t blur_id;           // Blur node ID
    uint32_t input_buffer_id;   // Input buffer ID
    struct wlblur_params params; // Blur parameters
    uint32_t num_damage_rects;  // Damage region count
    struct {
        int32_t x, y;
        int32_t width, height;
    } damage[32];               // Damage regions
};

// Render response (+ DMA-BUF FD via SCM_RIGHTS)
struct wlblur_render_response {
    uint32_t output_buffer_id;  // Output buffer ID
    uint32_t width, height;     // Dimensions
    uint32_t format;            // DRM format
};
```

### Daemon Architecture

```
wlblurd process:
├── Event loop (poll/epoll)
├── Socket listener (/run/user/1000/wlblurd.sock)
├── Per-client state
│   ├── Client ID
│   ├── Blur node registry
│   └── Buffer registry
├── libwlblur context (shared across clients)
└── Configuration
```

### Risk Factors

- **IPC overhead:** 0.2ms target may be challenging
  - *Mitigation:* Binary protocol, zero-copy FD passing
- **Daemon crashes:** Single daemon serves all compositors
  - *Mitigation:* Compositors track state, can recreate nodes
- **Synchronization:** GPU sync across processes
  - *Mitigation:* Start synchronous (glFinish), add sync objects later
- **Security:** Malicious compositor could DoS daemon
  - *Mitigation:* Resource limits, client quotas

### Resources

- IPC protocol design: `docs/post-investigation/comprehensive-synthesis1.md` (Part 2.2)
- Scene graph pattern: SceneFX `types/scene/wlr_scene.c`

---

## Milestone 4: ScrollWM Compositor Integration

**Timeline:** Weeks 9-10 (2 weeks)
**Status:** 📋 Planned
**Depends on:** m-3

### Objectives

First real-world compositor integration. Prove the external daemon approach with minimal changes to ScrollWM.

### Deliverables

- [ ] ScrollWM integration module (`blur_integration.c`, ~220 lines)
- [ ] IPC client wrapper library (`libwlblur-client`)
- [ ] DMA-BUF export integration (using wlroots APIs)
- [ ] Blur region detection (protocol or heuristic)
- [ ] Compositing blurred results into scene graph
- [ ] Git patch for ScrollWM (`scroll-blur.patch`)
- [ ] Integration documentation
- [ ] Demo video (blur in action)

### Success Metrics

- **Integration footprint:** <250 lines in ScrollWM codebase
- **Files changed:** <5 files
- **Visual quality:** Matches Hyprland blur at equivalent settings
- **Performance:** <2ms total overhead per blur region
- **Stability:** No crashes in 1-hour demo session

### Integration Architecture

```c
// scroll/desktop/blur_integration.c (~220 lines)

// 1. Initialize daemon connection
void scroll_blur_init(struct scroll_server *server) {
    blur_client = wlblur_client_connect("/run/user/1000/wlblurd.sock");
}

// 2. Detect blur-eligible windows
bool should_blur_window(struct scroll_container *con) {
    // Use same logic as decorations/shadows
    return con->decoration.blur_background;
}

// 3. Export backdrop, request blur
void render_blur_for_window(struct scroll_container *con) {
    // Render everything behind window
    struct wlr_buffer *backdrop = render_backdrop(con);

    // Export as DMA-BUF
    struct wlr_dmabuf_attributes dmabuf;
    wlr_buffer_get_dmabuf(backdrop, &dmabuf);

    // Request blur
    struct wlblur_params params = {.blur_radius = 40.0, .blur_passes = 3};
    int blurred_fd = wlblur_client_render(blur_client, dmabuf.fd, &params);

    // Import and composite
    struct wlr_buffer *blurred = import_dmabuf(blurred_fd);
    wlr_scene_buffer_create(con->scene_tree, blurred);
}
```

### Risk Factors

- **Maintainer acceptance:** ScrollWM maintainer may resist blur
  - *Mitigation:* Minimal changes, optional compile flag
- **wlroots API stability:** APIs may change
  - *Mitigation:* Target stable wlroots version
- **Testing burden:** Need ScrollWM test environment
  - *Mitigation:* Virtual machine or container

### Resources

- Integration strategy: `docs/post-investigation/blur-daemon-approach.md`
- ScrollWM context: `docs/pre-investigation/scrollwm-maintainer-discussion.md`

---

## Milestone 5: Production Hardening & Community Launch

**Timeline:** Weeks 11-12 (2 weeks)
**Status:** 📋 Planned
**Depends on:** m-4

### Objectives

Stabilize for daily use, optimize performance, prepare for public v1.0.0 release.

### Deliverables

- [ ] Multi-monitor support
- [ ] Performance optimizations (FBO pooling, async rendering)
- [ ] Blur result caching (static content optimization)
- [ ] Comprehensive testing suite
- [ ] Bug fixes from ScrollWM integration
- [ ] Packaging (AUR, nix flake, Fedora COPR)
- [ ] User documentation (installation, configuration, troubleshooting)
- [ ] v1.0.0 release
- [ ] Public announcement (Reddit /r/wayland, Hacker News)

### Success Metrics

- **Production readiness:** Can a user install and use without issues?
- **Performance:** Stable 60 FPS with 3-5 blurred windows
- **Multi-monitor:** Works correctly on 2+ monitors with different refresh rates
- **Community reception:** Positive feedback from early adopters
- **Bug reports:** <5 critical bugs in first week

### Optimization Targets

- **FBO pooling:** Reduce allocation overhead by 80%
- **Caching:** 20× speedup for static content (1.5ms → 0.05ms)
- **Async pipeline:** Hide IPC latency behind GPU work
- **Memory:** <32MB total for daemon with 10 blur nodes

### Packaging

- **Arch Linux (AUR):** `wlblur-git` package
- **NixOS:** Flake with overlay
- **Fedora (COPR):** RPM package
- **Build from source:** Meson setup in <5 minutes

### Risk Factors

- **Multi-monitor complexity:** Different refresh rates, DPI
  - *Mitigation:* Per-output blur contexts
- **Packaging maintenance:** Multiple distros to support
  - *Mitigation:* Focus on AUR and Nix initially
- **Community expectations:** Users may expect all features immediately
  - *Mitigation:* Clear roadmap, transparent about limitations

---

## Milestone 6: Tint & Vibrancy (Big Sur Parity)

**Timeline:** Weeks 13-15 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-5

### Objectives

Implement tint overlays and vibrancy system to achieve macOS Big Sur visual quality.

### Deliverables

- [ ] Tint overlay shader (RGBA blending)
- [ ] Vibrancy shader (HSL saturation boost)
- [ ] Desktop color sampling (adaptive tinting)
- [ ] IPC protocol extension for tint/vibrancy
- [ ] Material presets (basic: Window, Popover, Menu)
- [ ] Configuration API for tint colors
- [ ] Visual comparison with macOS Big Sur
- [ ] Documentation: "Achieving Big Sur Quality"

### Success Metrics

- **Visual parity:** 95%+ match with macOS Big Sur blur
- **Performance impact:** <0.3ms additional overhead
- **User control:** Full control over tint color, vibrancy strength
- **Presets:** 3+ material presets that look professional

### Technical Implementation

**Tint Overlay:**
```glsl
// tint.frag
uniform vec4 tint_color;     // RGBA [0-1]
uniform float tint_strength; // 0.0-1.0

vec4 blurred = texture(blurred_tex, uv);
vec4 tinted = mix(blurred, tint_color, tint_strength);
gl_FragColor = tinted;
```

**Vibrancy (from Hyprland):**
```glsl
// vibrancy.frag
vec3 hsl = rgb2hsl(backdrop.rgb);
hsl.y *= (1.0 + vibrancy_strength);  // Boost saturation
vec3 vibrant = hsl2rgb(hsl);
```

### Material Presets (Phase 1)

| Material | Blur Radius | Tint Color | Saturation | Vibrancy |
|----------|-------------|------------|------------|----------|
| **Window Background** | 40px | #F2F2F7 @ 90% | 1.1 | 1.05 |
| **Popover** | 50px | #F7F7F7 @ 85% | 1.15 | 1.10 |
| **Menu** | 35px | #FAFAFA @ 80% | 1.05 | 1.03 |

### Risk Factors

- **Color accuracy:** HSL conversion may have precision issues
  - *Mitigation:* Reference implementation from Hyprland
- **Desktop sampling overhead:** May impact performance
  - *Mitigation:* Sample only 64 points, cache results

### Resources

- Vibrancy algorithm: Hyprland `src/render/shaders/glsl/blur1.frag`
- macOS parity analysis: `docs/post-investigation/macos-parity-explained.md`
- Material system: `docs/post-investigation/phase4-materials.md`

---

## Milestone 7: Material System (Ventura Parity)

**Timeline:** Weeks 16-18 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-6

### Objectives

Implement full material preset system matching macOS Ventura/Sonoma visual quality.

### Deliverables

- [ ] 12+ material presets (HUD, Sidebar, Titlebar, Tooltip, Sheet, etc.)
- [ ] Appearance mode support (light/dark/auto)
- [ ] Dynamic adaptation (luminance-based)
- [ ] Material library API
- [ ] Compositor-side material selection
- [ ] User-defined custom materials
- [ ] Material editor tool (optional GUI)
- [ ] Visual showcase documentation

### Success Metrics

- **Visual parity:** 98%+ match with macOS Ventura materials
- **Material count:** 12+ presets covering common use cases
- **Adaptation quality:** Smooth appearance transitions
- **User adoption:** >50% of users use material presets vs manual params

### Material Presets (Complete Set)

| Material | Use Case | Blur | Tint (Light) | Tint (Dark) | Vibrancy |
|----------|----------|------|--------------|-------------|----------|
| **WindowBackground** | Main window content | 40px | #F2F2F7@90% | #1C1C1E@90% | 1.05 |
| **Sidebar** | Sidebar panels | 35px | #F7F7F7@85% | #2C2C2E@85% | 1.03 |
| **Popover** | Popover menus | 50px | #FAFAFA@80% | #323234@80% | 1.10 |
| **HUD** | Overlays, notifications | 60px | #FFFFFF@75% | #101214@95% | 1.15 |
| **Menu** | Dropdown menus | 35px | #F5F5F5@80% | #2A2A2C@80% | 1.05 |
| **Titlebar** | Window title bars | 30px | #ECECEC@95% | #323234@95% | 1.02 |
| **Tooltip** | Tooltips | 25px | #FAFAFA@90% | #2E2E30@90% | 1.00 |
| **Sheet** | Modal sheets | 45px | #F0F0F0@85% | #282828@85% | 1.08 |
| **UnderWindow** | Behind main window | 55px | #E8E8E8@70% | #1A1A1C@70% | 1.12 |
| **ContentBackground** | Content regions | 40px | #F5F5F5@88% | #242426@88% | 1.05 |
| **HeaderView** | Table headers | 30px | #EFEFEF@90% | #2C2C2E@90% | 1.03 |
| **FullScreenUI** | Full-screen overlays | 50px | #F2F2F2@80% | #1E1E20@80% | 1.10 |

### Dynamic Adaptation

```c
// Automatically adjust materials based on:
1. System appearance (light/dark mode)
2. Desktop wallpaper (average color/brightness)
3. Window backdrop content (high/low contrast)
4. Accessibility settings (reduce transparency)
```

### Risk Factors

- **Color calibration:** Getting exact macOS colors may be difficult
  - *Mitigation:* Side-by-side screenshots, user feedback
- **Preset proliferation:** Too many presets may confuse users
  - *Mitigation:* Clear naming, documentation with examples
- **Performance:** 12 presets with adaptation may be complex
  - *Mitigation:* Precompute where possible, cache adapted presets

---

## Milestone 8: Multi-Compositor Expansion (niri)

**Timeline:** Weeks 19-21 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-7

### Objectives

Prove multi-compositor support by integrating with niri (Rust/Smithay compositor).

### Deliverables

- [ ] Rust IPC client library (`wlblur-rs`)
- [ ] niri integration module
- [ ] Smithay DMA-BUF export integration
- [ ] niri-specific configuration
- [ ] Git patch for niri
- [ ] Documentation: "Integrating wlblur with Rust Compositors"
- [ ] Demo: Same materials in both ScrollWM and niri

### Success Metrics

- **Rust client quality:** Idiomatic Rust API
- **Integration effort:** <300 lines in niri
- **Visual consistency:** Identical materials between ScrollWM and niri
- **Performance parity:** Same <2ms overhead as ScrollWM

### Rust Client API

```rust
// wlblur-rs/src/lib.rs
pub struct WlblurClient {
    socket: UnixStream,
    client_id: u32,
}

impl WlblurClient {
    pub fn connect(socket_path: &str) -> Result<Self>;

    pub fn render(&mut self,
                  input_fd: RawFd,
                  params: &BlurParams) -> Result<RawFd>;

    pub fn render_with_material(&mut self,
                                input_fd: RawFd,
                                material: Material) -> Result<RawFd>;
}

// Material presets
pub enum Material {
    WindowBackground,
    Sidebar,
    Popover,
    Hud,
    // ... all 12 materials
}
```

### Risk Factors

- **Rust expertise:** Core team may lack deep Rust knowledge
  - *Mitigation:* Community contributor with Rust experience
- **Smithay API differences:** Different from wlroots patterns
  - *Mitigation:* Study niri codebase, consult maintainers
- **niri maintainer acceptance:** May not want blur
  - *Mitigation:* Minimal changes, optional feature flag

---

## Milestone 9: Community Features & Ecosystem

**Timeline:** Weeks 22-24 (3 weeks)
**Status:** 📋 Planned
**Depends on:** m-8

### Objectives

Polish, community-requested features, ecosystem integration, and long-term sustainability.

### Deliverables

- [ ] Additional algorithms (Gaussian, Box, Bokeh)
- [ ] Material animation support
- [ ] Configuration GUI (optional)
- [ ] Wayland protocol extension (`ext-background-blur-v1`)
- [ ] More compositor integrations (Sway, River, Wayfire)
- [ ] Performance profiling tools
- [ ] Comprehensive benchmark suite
- [ ] Community material library
- [ ] Long-term maintenance plan

### Success Metrics

- **Compositor count:** 4+ compositors supported
- **Community engagement:** >10 contributors
- **Material library:** >20 user-created materials
- **Performance:** Optimization based on real-world usage data
- **Stability:** <1 critical bug per month

### Additional Features

**Algorithm Selection:**
```c
enum wlblur_algorithm {
    WLBLUR_ALGO_KAWASE,      // Default, fastest
    WLBLUR_ALGO_GAUSSIAN,    // Highest quality
    WLBLUR_ALGO_BOX,         // Lightweight
    WLBLUR_ALGO_BOKEH,       // Artistic effect
};
```

**Wayland Protocol (Future):**
```xml
<protocol name="ext_background_blur_v1">
  <interface name="ext_background_blur_manager_v1" version="1">
    <request name="create_blur">
      <arg name="surface" type="object" interface="wl_surface"/>
      <arg name="blur_radius" type="fixed"/>
    </request>
  </interface>
</protocol>
```

### Risk Factors

- **Feature creep:** Too many features may delay stability
  - *Mitigation:* Prioritize based on user feedback
- **Maintenance burden:** More compositors = more testing
  - *Mitigation:* Automated CI for each compositor
- **Protocol adoption:** Wayland protocol may not be accepted
  - *Mitigation:* Start with compositor-specific integration

---

## Success Metrics

### Technical Metrics

| Metric | Target | Current | Notes |
|--------|--------|---------|-------|
| **Blur latency** | <1.5ms @ 1080p | - | Including IPC overhead |
| **IPC overhead** | <0.2ms | - | Round-trip time |
| **Memory usage** | <32MB | - | Daemon with 10 nodes |
| **Compositor integration** | <250 lines | - | Per compositor |
| **Visual quality** | 95%+ macOS parity | - | User survey |
| **Frame rate** | 60 FPS sustained | - | 5 blurred windows |
| **Test coverage** | >80% | - | Core library code |

### Community Metrics

| Metric | m-5 (v1.0) | m-9 (v2.0) | Notes |
|--------|------------|------------|-------|
| **GitHub stars** | 100+ | 500+ | Interest indicator |
| **Compositor support** | 1 (ScrollWM) | 4+ | Multi-compositor goal |
| **Contributors** | 1-2 | 10+ | Community growth |
| **User base** | 50+ | 500+ | Daily active users |
| **Material presets** | 3 | 20+ | Including user-created |

### Personal Success Metrics

- **Learning:** Deep understanding of Wayland, EGL, GPU programming
- **Portfolio:** Production-quality open source project
- **Community:** Build relationships with compositor maintainers
- **Impact:** Improve visual quality for entire Wayland ecosystem

---

## Risk Assessment

### High-Impact Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **IPC overhead too high** | Medium | High | Async pipeline, benchmark early |
| **Compositor maintainer resistance** | Medium | High | Minimal integration, optional |
| **DMA-BUF driver incompatibility** | Low | High | Extensive testing, CPU fallback |
| **Performance regression** | Medium | Medium | Continuous profiling, benchmarks |
| **Scope creep** | High | Medium | Strict milestone focus |

### Medium-Impact Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Multi-monitor complexity** | Medium | Medium | Per-output contexts |
| **GPU crash isolation failure** | Low | Medium | Robust error handling |
| **Protocol evolution breaks compatibility** | Low | Medium | Version IPC protocol |
| **Packaging maintenance burden** | High | Low | Focus on AUR/Nix initially |
| **Community expectations mismatch** | Medium | Low | Clear roadmap, communication |

---

## Timeline Summary

```
Week 1:    [m-0] ████████ Documentation & setup
Week 2:    [m-1] ████████ Shader extraction
Week 3-5:  [m-2] ████████████████████████ libwlblur core
Week 6-8:  [m-3] ████████████████████████ wlblurd daemon
Week 9-10: [m-4] ████████████████ ScrollWM integration
Week 11-12:[m-5] ████████████████ v1.0.0 production release
           ────────────────────────────────────────────────
Week 13-15:[m-6] ████████████████████████ Tint & vibrancy
Week 16-18:[m-7] ████████████████████████ Material system
Week 19-21:[m-8] ████████████████████████ niri integration
Week 22-24:[m-9] ████████████████████████ Community polish
           ────────────────────────────────────────────────
           MVP (12 weeks)              Full macOS parity (24 weeks)
```

---

## References

### Investigation Documentation

- **Comprehensive synthesis:** `docs/post-investigation/comprehensive-synthesis1.md`
- **Daemon approach:** `docs/post-investigation/blur-daemon-approach.md`
- **macOS parity:** `docs/post-investigation/macos-parity-explained.md`
- **Material system:** `docs/post-investigation/phase4-materials.md`

### Technical Deep Dives

- **Hyprland investigation:** `docs/investigation/hyprland-investigation/`
- **Wayfire investigation:** `docs/investigation/wayfire-investigation/`
- **SceneFX investigation:** `docs/investigation/scenefx-investigation/`

### Milestone Details

- **Milestone 0:** `backlog/milestones/m-0.md`
- **Milestone 1:** `backlog/milestones/m-1.md`
- **Milestone 2:** `backlog/milestones/m-2.md`
- **Milestone 3:** `backlog/milestones/m-3.md`
- **Milestone 4:** `backlog/milestones/m-4.md`
- **Milestone 5:** `backlog/milestones/m-5.md`

### Task Breakdowns

- **README:** `backlog/tasks/task-0.1.md`
- **ADRs:** `backlog/tasks/task-0.2.md`
- **ROADMAP:** `backlog/tasks/task-0.3.md`

---

## Getting Involved

**Current Phase:** Milestone 0 - Project Setup & Documentation
**Status:** 🔄 Documentation in progress

### How to Contribute

1. **Give Feedback:** Review architectural decisions in `docs/decisions/`
2. **Test Shaders:** Help validate shader extraction from source compositors
3. **Review Code:** Upcoming milestones will need code review
4. **Documentation:** Improve guides, add examples
5. **Integration:** Help integrate with your favorite compositor

### Communication

- **Issues:** GitHub Issues for bug reports, feature requests
- **Discussions:** GitHub Discussions for architecture questions
- **Updates:** Watch repository for milestone progress

---

**Last Updated:** 2025-01-15
**Version:** 1.0
**Next Review:** After Milestone 1 completion (Week 2)
