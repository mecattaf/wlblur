# From other files:
- scrollwm-maintainer-discussion.md which is what led me to this implementation plan
- niri-discussion.md is quite orthogonal (see later parts of this file) but shows that there is genuine interest for this; some users even willing to donate to achieve this feature
- chatgpt-conv1: high level discussion i had months ago, where i was picking a shell to apply blur to. this decision has been finalized: quickshell. it also highlights the apple-level visuals that i am trying 
- chatgpt-conv2: newer, conversation contains gold nuggets that i need to externalize:
ext background effect manager v1: discusses the implementation
has a mention of "12 apple features" which is desirable
as the discussion progressed i decided to leave the vulkan engine on icebox until i can get full blur capabilities (regardless of the underlying engine, and gles blur very well documented)
we defined the boundaries of the "blur ipc" functionality

# Previous Investigation into hyprland, swayfx and wayfire compositors which already have blur

## Hyprland – Kawase Blur with Vibrancy

**Hyprland** implements a **Dual Kawase blur**, an efficient multi-pass approximation of a Gaussian blur. It applies a small blur kernel repeatedly, downscaling and upscaling intermediate textures so that multiple passes yield a smooth, Gaussian-like effect.

Users can configure the blur **size (radius)** and **number of passes** to balance visual quality and performance — more passes produce smoother blur but increase GPU usage.

The blur is integrated directly into the compositor: when enabled, any transparent window (or one matching a blur rule) will blur the background behind it. Hyprland also offers fine-tuning options such as:

* **Noise addition** to reduce color banding.
* **Vibrancy controls** to boost saturation and make colors “spread” through the blur.

Vibrancy works by increasing color saturation at each blur pass, causing vivid tones to bleed into neutral areas for a richer, frosted-glass effect.

Performance features include:

* **new_optimizations** (enabled by default) to skip redundant operations.
* **xray mode**, which restricts blur to the wallpaper/background behind floating windows, ignoring other window layers to save GPU work.

---

## Wlrfx / SceneFX – Effects Framework for wlroots

**SceneFX (wlrfx)** is a drop-in replacement for wlroots’s scene graph, adding an FX renderer capable of **blur, shadows, rounded corners**, and more. It powers compositors such as **SwayFX** and others that focus on visual enhancements.

SceneFX’s blur is conceptually similar to Hyprland’s — it uses a **multi-pass GPU-based blur** emphasizing efficiency. Compositors like SwayFX expose configuration options such as:

* **blur_passes** and **blur_radius** (equivalent to Kawase blur iterations and kernel size).
* Additional image controls: **blur_noise**, **blur_brightness**, **blur_contrast**, and **blur_saturation** for precise tuning.

It also supports an **xray blur mode** (`blur_xray`), which limits blur to the background layer only, omitting window contents behind the target surface.

SceneFX provides per-surface control, allowing compositors to apply blur selectively — for instance, enabling blur, shadows, and rounded corners on panels or pop-ups via **layer_effects** configuration.


---

## Wayfire – Multiple Blur Algorithms via Plugin

**Wayfire** handles blur effects through a dedicated **plugin** that supports multiple algorithms and fine-grained parameters. The available blur methods include:

* **Gaussian blur**
* **Box blur**
* **Dual Kawase blur**
* **Bokeh blur** (a depth-of-field-style, lens-like blur)

Each method has configurable options:

* **offset** – controls blur radius per pass
* **iterations** – sets the number of passes
* **degrade** – adjusts downscaling for performance vs quality

These parameters allow users to balance strength and speed — e.g., more iterations and larger offset yield stronger blur, while higher degrade values improve speed at the cost of fidelity.

Wayfire’s blur can operate in two modes:

* **Normal mode:** automatically blurs backgrounds of transparent windows (by default, all toplevel windows).
* **Toggle mode:** blur can be enabled per-window via a keybinding.

Unlike Hyprland or SwayFX, Wayfire’s plugin focuses purely on blur quality and doesn’t include built-in vibrancy or brightness enhancements (though a global saturation setting exists).

The **Bokeh blur** stands out as a unique feature, producing a camera-style softness distinct from other algorithms.

Wayfire’s design emphasizes modularity — the blur plugin can be disabled, replaced, or reconfigured without patching the compositor. It uses **OpenGL** for rendering and fits seamlessly into Wayfire’s plugin framework.

In summary, Wayfire provides **multiple blur algorithms and customization options**, offering flexibility and variety, while **Hyprland and SceneFX** deliver optimized **Kawase blur implementations** with advanced visual tweaks like vibrancy and noise.

---


(Hyprland, SceneFX, and Wayfire) are **OpenGL ES–based** (GLES 2.0 or GLES 3.0)
But the *way* Wayfire structures things is what sets it apart — its plugin architecture abstracts the rendering stage so each effect (like blur) can use its own GLSL shader pipeline **without being tied to a single monolithic renderer**.

Let’s break this down carefully.

---

## 1. Common ground: all use GLES2/GLES3

| Compositor           | Graphics API                                    | Notes                                                                                    |
| -------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------------------- |
| **Hyprland**         | OpenGL ES 3.0 (through GLX or EGL)              | Uses custom shader pipeline; all blur passes are fragment shaders on FBOs.               |
| **SceneFX (SwayFX)** | OpenGL ES 2.0/3.0                               | Replaces `wlr_renderer` with its own “fx_renderer”; still GLES-based.                    |
| **Wayfire**          | OpenGL ES 3.0 via `wf::simple-texture` wrappers | Uses `gl_program_t` abstraction inside plugins; compositor core handles context & state. |

So, fundamentally, they’re all GPU-accelerated through the same primitives:

* framebuffer objects (FBOs)
* textures (`glTexImage2D`)
* GLSL fragment shaders
* render-to-texture downsample/upsample chains

---

## 2. How Wayfire differentiates its plugin model

Wayfire’s rendering system is built around a **“scene node” + plugin architecture**, where visual effects are modules that can hook into:

* `pre_render` (before the scene is drawn)
* `post_render` (after scene, for full-screen or background effects)
* `render_view()` (for per-surface effects)

Each plugin gets the compositor’s OpenGL context through the core’s `opengl_api_t`, so it can:

* compile its own shaders
* create FBOs or textures
* draw into the same shared GL pipeline

This is handled through the **Wayfire plugin API**, not via subclassing the renderer itself.

Example lifecycle for the *blur* plugin:

1. **Initialization:**

   * Plugin requests access to the OpenGL context.
   * Loads/compiles the GLSL programs for its chosen blur algorithm (Gaussian, Box, Kawase, Bokeh).
   * Allocates a small set of reusable FBOs/textures for intermediate passes.

2. **Render hook:**

   * Wayfire core invokes the plugin’s `post_render()` callback each frame.
   * The plugin binds its FBOs, performs downsample/upsample passes, then composites the blurred background with window textures.

3. **Algorithm dispatch:**

   * The plugin keeps several shader programs, one per algorithm (`wf_gaussian`, `wf_kawase`, `wf_box`, `wf_bokeh`).
   * The user’s config selects which to use.
   * Each algorithm’s code path differs only in sampling pattern and number of passes.

Because the core exposes the GL context uniformly, plugins don’t need to care whether it’s GLES 2 or 3; Wayfire abstracts that via helper functions (e.g. `OpenGL::use_default_program()`, `GL_CALLS` macros).
Internally it uses GLES 3 features like `sampler2D`, `textureLod`, and FBO blitting when available.

---

## 3. What this means practically

* **Hyprland** hardcodes blur into the compositor’s render pipeline (not optional at runtime).
* **SceneFX** rewrites the renderer but still runs monolithically inside the compositor process.
* **Wayfire** treats blur as a *loadable module*, compiled against the same GLES API but dynamically linked at runtime.

So from a design perspective, Wayfire’s plugins are **GLES clients** of the compositor’s rendering context.

That’s how it differentiates:
rather than having one unified blur system in the compositor, it allows multiple plugins (each possibly using different shaders or methods) to coexist and switch via configuration.

---

## 4. Implication for your external blur daemon

Because Wayfire’s blur plugin already encapsulates its rendering in a self-contained GLES 3 pipeline, it’s **the easiest one to extract wholesale** into a standalone process:

* It already manages its own FBOs and shaders.
* It expects to receive a texture and output a blurred texture.
* It doesn’t depend on the compositor’s scene graph logic.

If you want to reuse an implementation for your external daemon:

* **Wayfire’s plugin code** gives you the cleanest modular example (algorithm selection + FBO management + shader setup).
* **Hyprland’s** code shows how to add vibrancy, color boosts, and dynamic tuning.
* **SceneFX** shows how to attach effects per-surface inside a wlroots compositor.

---

# New references:

- Niri has an experimental PR for blur: https://github.com/YaLTeR/niri/pull/1634/files 
long discussion about whether this implementation is suitable; mention of hyprland's implementation: https://github.com/YaLTeR/niri/pull/1634
mentions drawing inspiration from this https://github.com/nferhat/fht-compositor/tree/main/src/renderer/blur simple blur implementation


----

# Initial discussion on full range of desired capabilities
 
## 1) What Apple actually ships (and why it feels superior)
 
Apple’s “glass/blur/tint/material” look isn’t a single blur shader—it’s a **compositor-level material system** with tight vertical integration:
 
 
-  
**Scene graph with semantic materials** App code asks for *a material* (e.g., sidebar, header, popover) via NSVisualEffectView/UIVisualEffectView. The system maps that semantic request to a tuned *recipe* (backdrop sample → multi-scale blur → vibrancy/tint → saturation/contrast → grain/dither → color space). Apps don’t micromanage radii; the compositor does, per display, scale, power state, and content.
 
 
-  
**Server-side backdrop** The WindowServer/Metal compositor owns the only truthful image of “what’s behind this element *after* transforms, shadows, and scaling.” Materials sample that resolved backdrop **before** the current layer draws, so there’s no readback, no janky copies, and the blur matches the *final* scene, not a guessed texture.
 
 
-  
**Multi-scale, damage-aware blurs** Gaussian isn’t computed naïvely per pixel. They build a mip-pyramid (or dual-kawase-style approximations) with **partial damage**. Only the rectangles that changed are re-blurred (plus a kernel margin). This keeps it cheap even at silly radii.
 
 
-  
**Vibrancy (luminance-aware compositing)** Text/icon colors are remapped using the **backdrop’s luminance/chroma**, so white text on dark glass and black text on light glass stays perfectly legible as the underlying wallpaper/app shifts. It’s not just α-blend; it’s color-space math with adaptive gain.
 
 
-  
**End-to-end color management (sRGB ↔ P3 ↔ HDR)** The whole pipe is color-managed and linearized where it counts. Backdrop sampling happens in a consistent color space; tints and saturation curves use perceptual transforms; output is retargeted per-display (P3, SDR/HDR). That’s why glass doesn’t “gray out” on wide-gamut panels.
 
 
-  
**Temporal hygiene** They avoid shimmer/aliasing by blurring **after** transforms, aligning kernels to pixel grids at the final scale, and sprinkling a super-subtle **film grain/dither** layer to break banding in very flat gradients.
 
 
-  
**Performance governors** Quality (radius, passes, downsample depth) is dynamically traded against power/thermal budget and frame time. Laptop on battery? Recipes quietly turn down.
 
 

 
The headline: **materials are a first-class compositor primitive**, not an app-side hack. The compositor has all the context—final pixels, transforms, color space, performance knobs—so it wins.
  
# 2) Why Wayland stacks lag (even when “blur” exists)
 
 
-  
**Security model: no raw backdrop for clients** Wayland intentionally prevents clients from reading other apps’ pixels. So “client-side blur” is out; **effects must be compositor-side**. Many blurs today (KWin, swayfx, Hyprland extensions) are compositor patches, but they often lack full scene awareness (e.g., fractional scaling, mixed transform stacks, HDR, per-output color space).
 
 
-  
**Renderer limitations & patchwork** wlroots’ default renderer has historically been GLES-centric. Advanced blur wants a **render graph**, multiple intermediate images, mip chains, and precise color management. You can do it with GL, but Vulkan (or GL with FBO orchestration) makes life easier. Most codebases don’t yet own a full render graph.
 
 
-  
**Damage and kernel margins are hard** Fast blur relies on reusing cached pyramids and only re-blurring damaged regions + halo margins. If your damage tracking is simplistic, you’ll either over-recompute (slow) or under-recompute (artifacts at damage edges).
 
 
-  
**Color management & HDR are young** Wayland color-management/HDR protocols are emerging. If you blur in the wrong space (non-linear sRGB), your glass looks “muddy,” tints are off, and gradients band.
 
 
-  
**No semantic materials** Apple’s “material = recipe” abstraction doesn’t exist by default. Linux desktops typically expose one “blur behind this window” toggle. Without a semantics layer, you get uneven results across apps and themes.
 
 
-  
**Performance governors rarely exist** Few compositors dynamically tune blur quality vs. frame time or power.
 
 

  
# 3) What it takes to match Apple on Wayland (the architecture)
 
You need three pillars:
 
## A) Compositor-native **Material System**
 
 
- Define **materials** as named recipes, not just shaders: Backdrop → DownsamplePyramid → (DualKawase passes) → SaturationCurve → Tint (blend mode) → Vibrancy (text color remap) → Grain → Clip (rounded corners/shape mask) → Shadow
 
- Expose them to clients via a small **unstable protocol** (think: zwp_material_v1): Clients set a material ID on a surface or sub-surface; the compositor applies it to that surface’s background/contents. Importantly, the **compositor** samples backdrop; the client never sees raw pixels.
 

 
## B) A **render graph** with cached pyramids & partial damage
 
 
- Adopt a graph that can express: 
 
  - Build/refresh **backdrop mip pyramids** per-output from the resolved scene (pre-material draw).
 
  - Recompute only damaged regions with **kernel margin expansion**.
 
  - Composite materials in correct order with clip masks and transform stacks.
 

 
 
- This is achievable in wlroots by adding a renderer module that owns: 
 
  - a per-output **resolved scene RT**
 
  - a per-output **pyramid atlas** (multiple blur radii share lower mips)
 
  - an **indirection table** mapping blur radii → which mip levels and how many passes.
 

 
 

 
## C) Proper **color pipeline** (linear, wide-gamut, HDR-aware)
 
 
- Linearize into a working space (scRGB or linear P3).
 
- Do blur/tint/saturation in linear or perceptually-uniform space.
 
- Re-encode to the swapchain’s EOTF (SDR/HDR10/HLG) with tone-mapping if needed.
 
- Honor per-output ICC/EDID and Wayland color-management protocols.
 



scenefx will implement ext-background-effect-v1 inside swayfx’s GLES3 renderer. This gives us compositor-owned blur/tint/saturation/vibrancy on arbitrary surfaces. This is how we get correct Apple-style backdrop sampling, not fake client-side blur.

We are NOT creating a second protocol right now. What we used to call “zwp-material-v1” is not a compositor protocol anymore.

Instead, we will ship libvisualeffect as a Rust crate.
libvisualeffect is the high-level material API (like NSVisualEffectView):

It wraps ext-background-effect-v1

It provides semantic materials (Hud, Sidebar, etc.) as presets

It exposes per-surface foreground style hints so text/icons stay readable (Apple-style vibrancy ergonomics)

It manages lifecycle and state sync for Quickshell

Quickshell uses libvisualeffect for every glassy widget. No AGS, no Astal, no GJS.

Apple’s advantage (and what we’re copying) is the split between:

A compositor that does physically correct, cached, damage-aware backdrop blur/tint/vibrancy

A semantic developer API where you ask for “a material”, not “a blur radius”

That’s now your design.



🌒 How It Looks — Now (Phase 1 + 2)
Backdrop: Clean Gaussian blur; underlying wallpaper softly diffused.

Panel body: Neutral gray translucency (no color shift).

Shadow: Soft, slightly lifted from background.

Behavior: Smooth, but visually “flat” — no tint or depth cues yet.

Think macOS Yosemite 10.10 (2014): the screenshots you shared exactly depict this stage.

🌕 How It Will Look — Phase 3 (Tint + Vibrancy)
Once scenefx adds linear-space tinting + vibrancy hints:

Each panel can have its own tint overlay (tint_color, tint_alpha).

Vibrancy adjusts foreground brightness based on blurred backdrop luminance.

Text/icons gain automatic contrast control (recommended_fg_style).

Subtle per-material saturation lift brings the “glass depth” that later macOS versions (Big Sur → Sonoma) show.

Your Raycast-style launcher will shift from Yosemite-flat glass → Big Sur-depth glass: colored tint (e.g., #101214 @ 0.22 α), vivid edges, readable text over any wallpaper.

🔮 Future Phases (Beyond 3)
When you introduce the full Material System, you’ll match macOS’s later refinements:

macOS Version	Key Visual Jump	Feature you’ll parallel
Yosemite (10.10)	Plain blur	✅ Phase 1 + 2
El Capitan – Mojave	Smoother tone curves	minor shader tweaks
Big Sur (11)	Tint + Vibrancy + Depth	✅ Phase 3
Monterey – Sonoma	Material recipes, dynamic adaptation	Phase 4 “Material System”

---

# Comprehensive Blur Compositor Implementation

## ext-background-effect-v1 Protocol Deep Dive

**ext-background-effect-v1 is a newly standardized Wayland protocol (merged May 2025) that enables compositor-side background blur effects.** While only KDE/KWin currently implements it, the protocol provides the architecturally correct foundation for visual effects—far superior to screencopy-based approaches that compromise security and performance.

**Current state: The protocol is in staging phase with limited capabilities.** Version 1 exposes only blur regions without client-configurable parameters (no radius, tint, or saturation controls yet). This means compositor implementations need to define these parameters internally, while client libraries should anticipate future protocol extensions.

### Protocol Specification and Architecture

The ext-background-effect-v1 protocol consists of two core interfaces that follow Wayland's standard request/event model. The **ext_background_effect_manager_v1** global singleton creates effect objects and advertises compositor capabilities through a capabilities event announcing blur support. Clients call get_background_effect(surface) to instantiate an **ext_background_effect_surface_v1** object associated with a specific wl_surface.

The surface-level effect object provides a single critical request: set_blur_region(region) accepts a wl_region defining the area requiring background blur in surface-local coordinates. The protocol implements Wayland's standard double-buffering pattern—region changes are pending until wl_surface.commit, ensuring atomic state updates synchronized with surface content. A NULL region removes the effect entirely, while the compositor automatically clips regions to surface boundaries.

**Critical limitation of v1**: The protocol deliberately does NOT expose client-configurable parameters for blur radius, algorithm, tint color, saturation, or vibrancy. The specification states these are "subject to compositor policies," giving implementers complete freedom in quality/performance trade-offs. This design philosophy separates client intent ("blur here") from compositor implementation ("how to blur"), but means the current protocol implementation won't support macOS-style parameter controls—those would require a future v2 protocol extension.

**Architectural flow**: Clients bind the manager global → receive capabilities event → create effect object for their surface → set blur region → commit surface state. Compositor captures backdrop content during rendering → applies blur → composites translucent surface on top. The compositor maintains complete control over blur implementation while clients simply declare their visual intent.

### Why Compositor-Side Blur is Architecturally Correct

ext-background-effect-v1 follows Wayland's capability-based security model: clients declaratively request "blur this region" on their own surfaces only, never accessing other windows' content. The compositor maintains complete control over implementation (algorithm, quality, power management policies). Security boundaries are preserved—blur is a compositor capability like shadows or window decorations, not a privilege escalation vector.

Performance is optimal: blur happens during the compositor's normal rendering pipeline in a single composition pass, using GPU-accelerated shaders that process backdrop content directly without buffer copies. Damage tracking integration means static blurred windows are "free"—blur only recomputes for damaged regions. On modern GPUs, compositor-side blur adds <1-2ms per window; with caching, static windows drop to <0.5ms overhead.

The rendering pipeline is clean: compositor renders background layers → captures backdrop for surface → applies blur in-place → composites translucent surface → continues with foreground. No redundant operations, optimal GPU utilization, proper effect ordering maintained.

---

## External Blur Daemon Architecture

The external blur service architecture solves the protocol's parameter control limitations while maintaining compositor modularity and multi-API support.

### Conceptual Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Client (Quickshell)                                    │
│    ↓ ext-background-effect-v1 (just regions)            │
├─────────────────────────────────────────────────────────┤
│  Compositor (scroll/scenefx)                            │
│    • Captures backdrop textures                         │
│    • Receives blur region requests                      │
│    • Forwards to blur service ↓                         │
├─────────────────────────────────────────────────────────┤
│  External Blur Service (GLES/Vulkan process)            │
│    • Receives DMA-BUF texture handles                   │
│    • Applies blur/tint/saturation with parameters       │
│    • Returns processed DMA-BUF                          │
│    ↑ THIS IS WHERE ADVANCED CONTROLS LIVE ↑            │
├─────────────────────────────────────────────────────────┤
│  Compositor composites final result                     │
└─────────────────────────────────────────────────────────┘
```

### Why This Works Around Protocol Limitations

**The key insight**: ext-background-effect-v1 becomes just the *signaling* layer between client and compositor ("please blur this region"). But the *implementation* layer (how to blur, with what parameters) lives in a separate service that the compositor controls.

The protocol never needs to expose parameters because:
1. Client → Compositor: "Blur this surface region" (ext-background-effect-v1)
2. Compositor → Blur Service: "Here's a texture, blur with radius=40, tint=#f2f2f7e6, saturation=1.2" (custom protocol)
3. Compositor receives processed texture and composites

**Separated concerns**:
- **Protocol handles intent**: "Apply blur effect"
- **Service handles implementation**: "Use dual kawase, 6 passes, tint overlay, saturation boost"

### Technical Feasibility: DMA-BUF Zero-Copy Architecture

The critical technology enabling this is **DMA-BUF** for zero-copy GPU texture sharing between processes. Both the compositor and blur service can import/export the same GPU memory allocation without pixel data crossing system memory—only metadata and file descriptors travel via IPC.

### Performance Characteristics

**Typical timings on mid-range GPU:**

| Stage | Operation | Approx. latency |
|-------|-----------|-----------------|
| DMA-BUF export from compositor framebuffer | map, create FD, hand off | 0.1–0.2 ms |
| Send metadata struct over Unix socket | copy 100 B, sendmsg() | 0.05 ms |
| Blur compute (6–8 passes dual Kawase) | GPU work | 0.7–1.0 ms |
| DMA-BUF import back into compositor | create image from FD | 0.1–0.2 ms |
| Optional synchronization fence wait | poll fence | 0.05 ms |
| **Total added latency per blur region** | | **≈ 1.1–1.5 ms** |

On a 16.6 ms frame budget (60 Hz), even several blurred panels stay well under budget. Because blur is asynchronous compute work, GPU overlap with other rendering further hides most of this cost.

**Bandwidth cost**: Zero-copy DMA-BUF means no pixel data crosses system memory. Only metadata and a few FDs travel via IPC, so PCIe and RAM bandwidth are untouched.

**CPU cost**: A single extra syscall pair (sendmsg + recvmsg) and FD bookkeeping per frame. Negligible (< 0.1% CPU).

### Architectural Trade-offs vs In-Compositor Blur

| Dimension | In-process blur (e.g. scenefx) | External blur service |
|-----------|--------------------------------|----------------------|
| GPU context | Shared with compositor GL/VK context | Separate device |
| Copy overhead | None (uses same FBO) | Minimal (DMA-BUF import/export) |
| IPC overhead | None | ~0.1 ms per region |
| Scheduling | Coupled to compositor frame loop | Can run async / parallel |
| Isolation | Blur crash kills compositor | Crash-safe; restart service |
| Maintainability | Must track wlroots internals | Independent binary |
| Extensibility | Hard-coded | Pluggable algorithms, parameters |
| Debuggability | Mixed logs/traces | Separate GPU traceable process |

**Bottom line**: The extra ~0.3 ms cost of IPC is usually outweighed by the maintenance, stability, and flexibility benefits. On powerful GPUs you'll never see measurable frame-time difference; on integrated GPUs it's still within tolerance because blur is bandwidth-bound, not compute-bound.

### Integration with ext-background-effect-v1

ext-background-effect-v1 remains the contract between clients and compositor. The external blur service is just the compositor's way of implementing that contract. This keeps full compliance with Wayland's spec while giving complete freedom over implementation.

**Protocol compliance**:
- ext-background-effect-v1 handles the *what* (blur region)
- External process handles the *how* (actual rendering)
- Compositor glues the two together

**To each layer:**

| Layer | Sees / Uses | Knows about external blur? |
|-------|-------------|---------------------------|
| Client app | ext-background-effect-v1 → set region | ❌ No |
| Compositor (scroll/sway) | Implements protocol, forwards blur work | ✅ Yes |
| Blur service | Receives DMA-BUFs and params | ✅ Yes |
| Wayland protocol stack | unchanged | ❌ No |

That's why the design is elegant: it's transparent up the stack.

---

## IPC Boundary Definition

The blur system requires two distinct IPC channels with different purposes and characteristics.

### Two-Channel Architecture

| Layer | Purpose | Mechanism | Bandwidth |
|-------|---------|-----------|-----------|
| Configuration IPC | Control plane: enable blur, change settings, toggle effects | JSON/text over socket | low |
| Blur Data IPC | Data plane: send/receive frame buffers | Unix socket with FD passing | high |

They complement each other perfectly.

### Configuration IPC (Existing Lua/JSON IPC)

The compositor's existing Lua/JSON IPC is meant for control messages:
- Exposing compositor state (workspaces, windows, outputs)
- Handling commands (move, focus, exec, reload, etc.)
- Toggling compositor options at runtime

It's asynchronous, textual, and not designed for:
- sending file descriptors (FBOs, DMA-BUFs)
- binary image data
- per-frame data exchange
- high-frequency communication

**Perfect for configuration, not for per-frame pixel work.**

**Usage example:**
```
scrollmsg 'set blur_radius 10'
scrollmsg 'set blur_vibrancy 1.2'
```
→ Compositor stores these in internal state.

### Blur Data IPC (New Binary Channel)

The blur pipeline needs to:
- send and receive DMA-BUF file descriptors (zero-copy GPU buffers)
- exchange region geometry + metadata at sub-frame latency (milliseconds)
- avoid JSON parse overhead
- operate asynchronously but binary-safe

**Required mechanism:**
- AF_UNIX domain sockets (SOCK_SEQPACKET or SOCK_STREAM)
- sendmsg() / recvmsg() with SCM_RIGHTS ancillary data (for FD passing)
- binary structs (C-structs or packed protobuf)

**IPC designed for data and file descriptors, not textual commands.**

### Protocol Structure

```c
// Request sent from Compositor → blur daemon
struct blur_req {
    uint64_t req_id;
    uint32_t width, height;
    struct {
        int32_t x, y, w, h;
    } region;
    // optional params
    float blur_radius;
    float saturation;
    float tint_rgba[4];
};

// Response from blur daemon → Compositor
struct blur_resp {
    uint64_t req_id;
    int32_t error_code;
    // blurred buffer returned via FD
};
```

Both sides use sendmsg()/recvmsg() with SCM_RIGHTS to pass the buffer FD.

### Integration Architecture

```
               ┌──────────────────────────────────────────┐
               │                Compositor                │
               │------------------------------------------│
               │ Config IPC (text)  →  settings           │
               │ Blur IPC (binary) → region buffers →     │
               │   external daemon                        │
               └──────────────────────────────────────────┘
                              │
                              ▼
                 ┌─────────────────────────────────┐
                 │        Blur Daemon              │
                 │  GL/Vulkan renderer             │
                 │  (listens on /run/user/1000/blur.sock) │
                 └─────────────────────────────────┘
```

**Config IPC** → enable/disable blur, change parameters, restart daemon
**Binary IPC** → send DMA-BUFs + metadata, receive blurred FDs

### Requirements Matrix

| Requirement | Config IPC? | FD-based IPC? |
|-------------|------------|---------------|
| Pass DMA-BUF handle (fd)? | ❌ No | ✅ Yes |
| Real-time per-frame data? | ❌ No | ✅ Yes |
| Text-free binary protocol? | ❌ No | ✅ Yes |
| Compatible with wlroots' buffer sharing? | ❌ No | ✅ Yes |
| Config changes / toggles? | ✅ Yes | ✅ Maybe |

**Use Config IPC for configuration, add new binary IPC for data exchange.**

### Why Compositor-Agnostic

This design is:
- **Renderer-agnostic**: works with GLES2, GLES3, Vulkan backends alike
- **Upstream-friendly**: no patch to wlroots internals; protocol logic is in compositor userspace
- **Modular**: can replace the daemon without touching compositor
- **Stable**: aligns with compositor "stay close to upstream" philosophy

---

## Apple Compositor Material System and Visual Quality Targets

### What Apple Actually Ships

Apple's "glass/blur/tint/material" look isn't a single blur shader—it's a **compositor-level material system** with tight vertical integration:

**Scene graph with semantic materials**: App code asks for *a material* (e.g., sidebar, header, popover) via NSVisualEffectView/UIVisualEffectView. The system maps that semantic request to a tuned *recipe* (backdrop sample → multi-scale blur → vibrancy/tint → saturation/contrast → grain/dither → color space). Apps don't micromanage radii; the compositor does, per display, scale, power state, and content.

**Server-side backdrop**: The WindowServer/Metal compositor owns the only truthful image of "what's behind this element *after* transforms, shadows, and scaling." Materials sample that resolved backdrop **before** the current layer draws, so there's no readback, no janky copies, and the blur matches the *final* scene, not a guessed texture.

**Multi-scale, damage-aware blurs**: Gaussian isn't computed naïvely per pixel. They build a mip-pyramid (or dual-kawase-style approximations) with **partial damage**. Only the rectangles that changed are re-blurred (plus a kernel margin). This keeps it cheap even at silly radii.

**Vibrancy (luminance-aware compositing)**: Text/icon colors are remapped using the **backdrop's luminance/chroma**, so white text on dark glass and black text on light glass stays perfectly legible as the underlying wallpaper/app shifts. It's not just α-blend; it's color-space math with adaptive gain.

**End-to-end color management (sRGB ↔ P3 ↔ HDR)**: The whole pipe is color-managed and linearized where it counts. Backdrop sampling happens in a consistent color space; tints and saturation curves use perceptual transforms; output is retargeted per-display (P3, SDR/HDR). That's why glass doesn't "gray out" on wide-gamut panels.

### The 12 Apple Features

macOS NSVisualEffectView provides 12 semantic material types (windowBackground, contentBackground, titlebar, etc.) with automatic appearance adaptation, vibrancy system for enhanced contrast on translucent backgrounds, and blending modes (behindWindow vs withinWindow). Each material has carefully tuned parameters: blur radius 30-60px, gaussian sigma 15-30, tint overlays at 85-95% alpha, saturation boost 1.1-1.2x, and desktop color sampling for tinting.

### Visual Quality Targets Mapped to macOS Releases

**Phase 1 + 2 Target: macOS Yosemite (10.10)**

With basic ext-background-effect-v1 implementation and fundamental blur parameters:

| Effect | Yosemite Equivalent | Description |
|--------|---------------------|-------------|
| blur_enable + blur_radius + blur_passes | "Frosted glass" backdrop | Same per-surface Gaussian/Kawase blur that defined early macOS translucency |
| blur_brightness, blur_contrast, blur_saturation | Subtle contrast boost | These replicate Apple's simple tone adjustments before they introduced vibrancy |
| blur_noise | Dither / grain | Apple used fine noise to hide banding |
| corner_radius | Rounded window masks | Matches Yosemite's notification-center panels and popovers |
| shadows, shadow_blur_radius, shadow_color | Drop shadows | Gives separation from the backdrop—important for depth perception |
| layer_effects (per-namespace) | Different panels, different treatments | Apple used slightly different translucency for dock, menu bar, notifications |
| dim_inactive | Focus dimming | Comparable to macOS lowering brightness for inactive windows |

**Visual outcome**: Clean Gaussian blur; underlying wallpaper softly diffused. Panel body: Neutral gray translucency (no color shift). Shadow: Soft, slightly lifted from background. Behavior: Smooth, but visually "flat" — no tint or depth cues yet.

**Phase 3 Target: macOS Big Sur (11) / Monterey (12)**

Once scenefx/blur daemon adds linear-space tinting + vibrancy hints:
- Each panel can have its own tint overlay (tint_color, tint_alpha)
- Vibrancy adjusts foreground brightness based on blurred backdrop luminance
- Text/icons gain automatic contrast control (recommended_fg_style)
- Subtle per-material saturation lift brings the "glass depth" that later macOS versions show

**Visual shift**: From Yosemite-flat glass → Big Sur-depth glass: colored tint (e.g., #101214 @ 0.22 α), vivid edges, readable text over any wallpaper.

**Phase 4 Target: macOS Ventura (13) / Sonoma (14)**

Full Material System with semantic presets:
- Material recipes: dynamic adaptation
- System-wide consistency
- Preset materials (Hud, Sidebar, Popover, etc.)

### Quality Implementation Targets

**For scenefx/blur daemon implementation:**
- Dual kawase blur: 6-8 iterations for 40-60px radius
- Tint overlays: RGBA (242, 242, 247, 230) for light theme window backgrounds
- Saturation boost: 1.15x multiplier on backdrop content
- Vibrancy: 1.05-1.15x contrast enhancement
- Desktop tinting: Sample average hue from backdrop, apply 0.1-0.2 strength tint
- Performance: <2ms compositor overhead, <0.5ms with caching

Implementation pipeline: capture backdrop → downsample blur → upsample blur → saturation adjustment → tint overlay → composite with surface.

### Progressive Visual Parity Roadmap

| Stage | Implementation | Visual Outcome | Apple Comparison |
|-------|----------------|----------------|------------------|
| Phase 1 + 2 | ext-background-effect-v1; basic blur daemon | Smooth frosted glass, flat, neutral blur | macOS Yosemite / basic "transparency" |
| Phase 3 | Add tint + vibrancy + linear color; API + fg hint | Deep "glass HUD" look, adaptive readability, Raycast-like finish | macOS Big Sur / Monterey Spotlight |
| Phase 4 | Material presets (Hud, Sidebar, Popover) | Semantic materials, system-wide consistency | macOS Ventura / Sonoma materials system |

---

## Existing Compositor Blur Implementations

The current Wayland compositor ecosystem has robust blur implementations that provide the technical foundation for achieving Apple-level visual quality. Hyprland, SceneFX/SwayFX, and Wayfire each offer production-ready blur systems with different architectural approaches and capabilities.

### Hyprland — Kawase Blur with Vibrancy

Hyprland implements a **Dual Kawase blur**, an efficient multi-pass approximation of a Gaussian blur. A small blur kernel is applied repeatedly, downscaling and upscaling intermediate textures so that multiple passes yield a smooth, Gaussian-like effect.

Users can configure the blur **size (radius)** and **number of passes** to balance visual quality and performance — more passes produce smoother blur but increase GPU usage.

The blur is integrated directly into the compositor: when enabled, any transparent window (or one matching a blur rule) will blur the background behind it. Hyprland offers fine-tuning options:
- **Noise addition** to reduce color banding
- **Vibrancy controls** to boost saturation and make colors "spread" through the blur

Vibrancy works by increasing color saturation at each blur pass, causing vivid tones to bleed into neutral areas for a richer, frosted-glass effect.

Performance features include:
- **new_optimizations** (enabled by default) to skip redundant operations
- **xray mode**, which restricts blur to the wallpaper/background behind floating windows, ignoring other window layers to save GPU work

### Wlrfx / SceneFX — Effects Framework for wlroots

**SceneFX (wlrfx)** is a drop-in replacement for wlroots's scene graph, adding an FX renderer capable of **blur, shadows, rounded corners**, and more. It powers compositors such as **SwayFX** and others that focus on visual enhancements.

SceneFX's blur is conceptually similar to Hyprland's — it uses a **multi-pass GPU-based blur** emphasizing efficiency. Compositors like SwayFX expose configuration options:
- **blur_passes** and **blur_radius** (equivalent to Kawase blur iterations and kernel size)
- Additional image controls: **blur_noise**, **blur_brightness**, **blur_contrast**, and **blur_saturation** for precise tuning

It also supports an **xray blur mode** (`blur_xray`), which limits blur to the background layer only, omitting window contents behind the target surface.

SceneFX provides per-surface control, allowing compositors to apply blur selectively — for instance, enabling blur, shadows, and rounded corners on panels or pop-ups via **layer_effects** configuration.

### Wayfire — Multiple Blur Algorithms via Plugin

**Wayfire** handles blur effects through a dedicated **plugin** that supports multiple algorithms and fine-grained parameters. The available blur methods include:
- **Gaussian blur**
- **Box blur**
- **Dual Kawase blur**
- **Bokeh blur** (a depth-of-field-style, lens-like blur)

Each method has configurable options:
- **offset** — controls blur radius per pass
- **iterations** — sets the number of passes
- **degrade** — adjusts downscaling for performance vs quality

These parameters allow users to balance strength and speed — e.g., more iterations and larger offset yield stronger blur, while higher degrade values improve speed at the cost of fidelity.

Wayfire's blur can operate in two modes:
- **Normal mode:** automatically blurs backgrounds of transparent windows (by default, all toplevel windows)
- **Toggle mode:** blur can be enabled per-window via a keybinding

Unlike Hyprland or SwayFX, Wayfire's plugin focuses purely on blur quality and doesn't include built-in vibrancy or brightness enhancements (though a global saturation setting exists).

The **Bokeh blur** stands out as a unique feature, producing a camera-style softness distinct from other algorithms.

Wayfire's design emphasizes modularity — the blur plugin can be disabled, replaced, or reconfigured without patching the compositor. It uses **OpenGL** for rendering and fits seamlessly into Wayfire's plugin framework.

### Technical Foundation: All GLES-Based

All three compositors (Hyprland, SceneFX, and Wayfire) are **OpenGL ES—based** (GLES 2.0 or GLES 3.0):

| Compositor | Graphics API | Notes |
|------------|-------------|-------|
| **Hyprland** | OpenGL ES 3.0 (through GLX or EGL) | Uses custom shader pipeline; all blur passes are fragment shaders on FBOs |
| **SceneFX (SwayFX)** | OpenGL ES 2.0/3.0 | Replaces `wlr_renderer` with its own "fx_renderer"; still GLES-based |
| **Wayfire** | OpenGL ES 3.0 via `wf::simple-texture` wrappers | Uses `gl_program_t` abstraction inside plugins; compositor core handles context & state |

Fundamentally, they're all GPU-accelerated through the same primitives:
- framebuffer objects (FBOs)
- textures (`glTexImage2D`)
- GLSL fragment shaders
- render-to-texture downsample/upsample chains

### Wayfire's Unique Plugin Architecture

Wayfire's rendering system is built around a **"scene node" + plugin architecture**, where visual effects are modules that can hook into:
- `pre_render` (before the scene is drawn)
- `post_render` (after scene, for full-screen or background effects)
- `render_view()` (for per-surface effects)

Each plugin gets the compositor's OpenGL context through the core's `opengl_api_t`, so it can:
- compile its own shaders
- create FBOs or textures
- draw into the same shared GL pipeline

This is handled through the **Wayfire plugin API**, not via subclassing the renderer itself.

Because Wayfire's blur plugin already encapsulates its rendering in a self-contained GLES 3 pipeline, it's **the easiest one to extract wholesale** into a standalone process:
- It already manages its own FBOs and shaders
- It expects to receive a texture and output a blurred texture
- It doesn't depend on the compositor's scene graph logic

### Reuse Strategy for External Blur Daemon

If reusing an implementation for the external daemon:
- **Wayfire's plugin code** gives the cleanest modular example (algorithm selection + FBO management + shader setup)
- **Hyprland's** code shows how to add vibrancy, color boosts, and dynamic tuning
- **SceneFX** shows how to attach effects per-surface inside a wlroots compositor

---

## Understanding the Comprehensive Blur Ecosystem

The existing blur implementations (Hyprland, SceneFX, Wayfire) are quite robust and provide excellent technical foundations. However, achieving Apple-level visual completeness requires building a comprehensive "blur ecosystem" that extends beyond basic blur functionality.

### What "Comprehensive Ecosystem" Means

This ecosystem encompasses:

1. **Protocol standardization** via ext-background-effect-v1 for cross-compositor compatibility
2. **External blur service** architecture for parameter-rich effects independent of compositor internals
3. **Client library** (libvisualeffect) providing semantic material APIs to applications
4. **Progressive enhancement** from basic blur (Yosemite) → tint+vibrancy (Big Sur) → full materials (Ventura/Sonoma)
5. **IPC boundaries** that separate configuration from high-performance data paths
6. **Multi-compositor support** enabling blur across different Wayland compositors

### Why External Architecture Matters

While in-compositor blur works well, the external blur daemon architecture provides:
- **Protocol compliance**: full ext-background-effect-v1 support while extending beyond v1 limitations
- **Parameter richness**: tint, vibrancy, saturation, desktop color sampling not possible with current protocol
- **Crash isolation**: blur service failures don't take down compositor
- **Multi-renderer support**: same blur service can work with GLES2, GLES3, or Vulkan compositors
- **Extensibility**: new algorithms, effects, materials added without compositor changes

This matches Apple's approach: a system-level material service that apps request semantically, with the compositor/WindowServer handling implementation details.
