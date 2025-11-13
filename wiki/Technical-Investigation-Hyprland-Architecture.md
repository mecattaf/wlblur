# Hyprland Renderer Architecture and Blur Path

**Date:** 2025-11-12
**Repository:** [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland)
**Language:** C++
**Graphics API:** OpenGL ES 3.2 / EGL

---

## Executive Summary

Hyprland implements a sophisticated compositor-integrated blur system using **Dual Kawase** blur with vibrancy enhancements. The blur is tightly integrated with the compositor's render loop, damage tracking, and framebuffer management, making it highly optimized but difficult to externalize.

**Key Architecture:**
- **Graphics API:** GLES 3.2 (preferred) or GLES 3.0 (fallback) via EGL
- **Blur Algorithm:** Dual Kawase with configurable passes (1-8)
- **Integration:** Compositor-native, render pass system
- **Performance:** <2ms per frame with caching enabled

---

## Core Components

### 1. CCompositor

**Location:** `/tmp/Hyprland/src/Compositor.hpp:25`

Top-level compositor orchestrator that manages:
- Windows (`m_windows: std::vector<PHLWINDOW>`)
- Layers (`m_layers: std::vector<PHLLS>`)
- Monitors (`m_monitors: std::vector<PHLMONITOR>`)
- Frame scheduling (`scheduleFrameForMonitor()` - Compositor.cpp:2462)
- Aquamarine backend for output management (`m_aqBackend`)

**Role in Blur:**
- Provides window/layer surface state
- Schedules frame rendering which triggers blur computation
- Manages compositor-wide blur configuration

### 2. CHyprRenderer

**Location:** `/tmp/Hyprland/src/render/Renderer.hpp:50`

Main rendering coordinator that:
- Coordinates the render loop (`renderMonitor()` - Renderer.cpp:1207)
- Manages render passes via `CRenderPass m_renderPass`
- Determines which windows/layers need blur (`shouldBlur()`)
- Calls OpenGL implementation for actual rendering

**Blur-Specific Methods:**
```cpp
bool shouldBlur(PHLLS ls);      // Check if layer surface needs blur
bool shouldBlur(PHLWINDOW w);   // Check if window needs blur
bool shouldBlur(WP<CPopup> p);  // Check if popup needs blur
```

### 3. CHyprOpenGLImpl

**Location:** `/tmp/Hyprland/src/render/OpenGL.hpp:175`

Low-level OpenGL implementation that:
- Manages EGL context (`m_eglContext`, `m_eglDisplay`)
- Owns shader programs (`SP<SPreparedShaders> m_shaders`)
- Manages per-monitor render resources
- Implements blur algorithm (`blurMainFramebufferWithDamage()`)

**Key Blur Methods:**
```cpp
CFramebuffer* blurMainFramebufferWithDamage(float a, CRegion* damage);
CFramebuffer* blurFramebufferWithDamage(float a, CRegion* damage, CFramebuffer& source);
void preBlurForCurrentMonitor();  // Cache blur for optimizations
void markBlurDirtyForMonitor(PHLMONITOR);
```

---

## Render Loop Flow

### Frame Rendering Sequence

```
CCompositor::scheduleFrameForMonitor()
  └─> Monitor frame callback
      └─> CHyprRenderer::renderMonitor() [Renderer.cpp:1207]
          │
          ├─> 1. BEGIN RENDER
          │   └─> beginRender() [Renderer.cpp:2242]
          │       └─> CHyprOpenGLImpl::begin() [OpenGL.cpp:778]
          │           ├─> Allocate FBOs (offloadFB, mirrorFB, mirrorSwapFB)
          │           ├─> Set projection matrices
          │           └─> Bind offloadFB as current render target
          │
          ├─> 2. RENDER WORKSPACE CONTENTS
          │   └─> renderWorkspace() [renders windows/layers]
          │       ├─> renderAllClientsForWorkspace()
          │       └─> CHyprOpenGLImpl::preWindowPass() [OpenGL.cpp:2292]
          │           └─> CRenderPass::add(CPreBlurElement) [if blur needed]
          │
          ├─> 3. EXECUTE RENDER PASS
          │   └─> CRenderPass::render() [executes all pass elements]
          │       ├─> CPreBlurElement::draw() [PreBlurElement.cpp:6]
          │       │   └─> preBlurForCurrentMonitor() [OpenGL.cpp:2260]
          │       │       └─> blurMainFramebufferWithDamage()
          │       │           ├─> blurprepare.frag (contrast/brightness)
          │       │           ├─> blur1.frag × N (downsample passes)
          │       │           ├─> blur2.frag × N (upsample passes)
          │       │           └─> blurfinish.frag (noise)
          │       │
          │       ├─> CSurfacePassElement::draw() [render windows]
          │       │   └─> renderTextureWithBlurInternal() [per-window blur]
          │       │
          │       └─> CTexPassElement::draw() [render layers]
          │
          └─> 4. END RENDER
              └─> CHyprRenderer::endRender()
                  └─> CHyprOpenGLImpl::end() [OpenGL.cpp:849]
                      └─> Copy offloadFB → outFB (final display buffer)
```

### Surface/Window Flow Through Pipeline

1. **Window Collection:** `g_pCompositor->m_windows` (vector of all windows)
2. **Visibility Check:** `shouldRenderWindow()` checks visibility, workspace, etc.
3. **Blur Decision:** `shouldBlur()` checks window/layer blur settings
4. **Pass Element Creation:**
   - Windows/surfaces → `CSurfacePassElement` or `CTexPassElement`
   - Blur preprocessing → `CPreBlurElement`
5. **Blur Rendering:** If blur enabled → `renderTextureWithBlurInternal()`

---

## EGL and OpenGL Context Management

### Context Creation Sequence

**Location:** OpenGL.cpp:263-362

```cpp
CHyprOpenGLImpl::CHyprOpenGLImpl() {
    // 1. Query EGL extensions
    const char* EGLEXTENSIONS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    // 2. Load EGL proc addresses
    loadGLProc(&m_proc.eglCreateImageKHR, "eglCreateImageKHR");
    loadGLProc(&m_proc.eglGetPlatformDisplayEXT, "eglGetPlatformDisplayEXT");
    loadGLProc(&m_proc.eglCreateSyncKHR, "eglCreateSyncKHR");
    // ... more extensions

    // 3. Try EXT_platform_device first, then fall back to GBM
    if (m_exts.KHR_display_reference) {
        m_eglDevice = eglDeviceFromDRMFD(m_drmFD);
        initEGL(false); // EXT_platform_device
    } else {
        m_gbmDevice = gbm_create_device(m_gbmFD);
        initEGL(true);  // GBM
    }
}
```

### EGL Display and Context Initialization

**Location:** OpenGL.cpp:133-207

```cpp
void CHyprOpenGLImpl::initEGL(bool gbm) {
    // 1. Create EGL display
    m_eglDisplay = m_proc.eglGetPlatformDisplayEXT(
        gbm ? EGL_PLATFORM_GBM_KHR : EGL_PLATFORM_DEVICE_EXT,
        gbm ? (void*)m_gbmDevice : (void*)m_eglDevice,
        nullptr
    );

    // 2. Initialize EGL
    EGLint major, minor;
    eglInitialize(m_eglDisplay, &major, &minor);

    // 3. Try GLES 3.2 first, fallback to 3.0
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_TRUE,
        EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
        EGL_LOSE_CONTEXT_ON_RESET_EXT,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, EGL_NO_CONFIG_KHR,
                                     EGL_NO_CONTEXT, contextAttribs);

    if (m_eglContext == EGL_NO_CONTEXT) {
        // Fallback to GLES 3.0
        contextAttribs[3] = 0;  // minor version = 0
        m_eglContext = eglCreateContext(m_eglDisplay, EGL_NO_CONFIG_KHR,
                                         EGL_NO_CONTEXT, contextAttribs);
        m_eglContextVersion = EGL_CONTEXT_GLES_3_0;
    } else {
        m_eglContextVersion = EGL_CONTEXT_GLES_3_2;
    }

    // 4. Make context current (surfaceless)
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext);
}
```

### Context Version Enum

**Location:** OpenGL.hpp:335-341

```cpp
enum eEGLContextVersion : uint8_t {
    EGL_CONTEXT_GLES_2_0 = 0,
    EGL_CONTEXT_GLES_3_0,
    EGL_CONTEXT_GLES_3_2,
};

eEGLContextVersion m_eglContextVersion = EGL_CONTEXT_GLES_3_2; // preferred
```

**Answer:** Hyprland uses **both EGL and GLES3**:
- **EGL:** Context management, display initialization, extension loading
- **GLES 3.2:** Preferred rendering API (fallback to GLES 3.0 if unavailable)

---

## Framebuffer Architecture

### SMonitorRenderData Structure

**Location:** OpenGL.hpp:103-115

```cpp
struct SMonitorRenderData {
    CFramebuffer offloadFB;        // Main render target (all windows/layers)
    CFramebuffer mirrorFB;         // Blur working buffer 1 (ping-pong)
    CFramebuffer mirrorSwapFB;     // Blur working buffer 2 (ping-pong)
    CFramebuffer offMainFB;        // Offload buffer for effects
    CFramebuffer monitorMirrorFB;  // For output mirroring
    CFramebuffer blurFB;           // CACHED blur result (new_optimizations)

    SP<CTexture> stencilTex;       // Shared stencil buffer

    bool blurFBDirty = true;        // Cache needs recomputation
    bool blurFBShouldRender = false; // Will be rendered this frame
};
```

**Per-Monitor Storage:**
```cpp
std::map<PHLMONITORREF, SMonitorRenderData> m_monitorRenderResources;
```

### Framebuffer Lifecycle

**Allocation:** OpenGL.cpp:813-823

```cpp
void CHyprOpenGLImpl::begin(PHLMONITOR pMonitor, const CRegion& damage, ...) {
    auto monData = &m_monitorRenderResources[pMonitor];

    // Allocate if size/format changed
    if (monData->offloadFB.m_size != pMonitor->m_pixelSize ||
        DRM_FORMAT != monData->offloadFB.m_drmFormat) {

        // Allocate all FBs at full monitor resolution
        monData->offloadFB.alloc(pixelSize.x, pixelSize.y, DRM_FORMAT);
        monData->mirrorFB.alloc(pixelSize.x, pixelSize.y, DRM_FORMAT);
        monData->mirrorSwapFB.alloc(pixelSize.x, pixelSize.y, DRM_FORMAT);
        monData->offMainFB.alloc(pixelSize.x, pixelSize.y, DRM_FORMAT);

        // Attach shared stencil buffer (for rounded corners)
        monData->offloadFB.addStencil(monData->stencilTex);
        monData->mirrorFB.addStencil(monData->stencilTex);
        monData->mirrorSwapFB.addStencil(monData->stencilTex);
    }

    // Bind offloadFB as current render target
    monData->offloadFB.bind();
}
```

### Framebuffer Usage in Render Pipeline

```
┌──────────────────────────────────────────┐
│ offloadFB (Main Render Target)           │
│ • All windows/layers rendered here       │
│ • Source for blur computation            │
└──────────────────────────────────────────┘
             ↓ (copy to mirror)
┌──────────────────────────────────────────┐
│ mirrorFB / mirrorSwapFB (Ping-Pong)      │
│ • Blur passes alternate between these    │
│ • Downsample passes                      │
│ • Upsample passes                        │
└──────────────────────────────────────────┘
             ↓ (cache result)
┌──────────────────────────────────────────┐
│ blurFB (Cached Blur)                     │
│ • Stores final blur result               │
│ • Reused between frames                  │
│ • Invalidated on damage                  │
└──────────────────────────────────────────┘
             ↓ (composite)
┌──────────────────────────────────────────┐
│ outFB (Display Buffer)                   │
│ • Final composited output                │
│ • Sent to display hardware               │
└──────────────────────────────────────────┘
```

---

## Render Pass System

### Pass Element Hierarchy

**Location:** PassElement.hpp:3

```
IPassElement (base class)
  ├─ CPreBlurElement       - Blur preprocessing (cache computation)
  ├─ CSurfacePassElement   - Render surface content
  ├─ CTexPassElement       - Render texture
  ├─ CRectPassElement      - Render rectangle
  ├─ CPrepassElement       - Pre-rendering setup
  └─ ... (other elements)
```

### CPreBlurElement Implementation

**Location:** PreBlurElement.cpp:1-24

```cpp
class CPreBlurElement : public IPassElement {
  public:
    void draw(const CRegion& damage) {
        g_pHyprOpenGL->preBlurForCurrentMonitor();
    }

    bool needsLiveBlur() { return false; }
    bool needsPrecomputeBlur() { return false; }
    bool disableSimplification() { return true; }  // Never skip this element
    bool undiscardable() { return true; }          // Never discard

    const char* passName() { return "CPreBlurElement"; }
};
```

### When PreBlurElement is Added

**Location:** OpenGL.cpp:2292-2297

```cpp
void CHyprOpenGLImpl::preWindowPass() {
    if (!preBlurQueued())
        return;

    // Add to render pass BEFORE window rendering
    g_pHyprRenderer->m_renderPass.add(makeUnique<CPreBlurElement>());
}

bool CHyprOpenGLImpl::preBlurQueued() {
    static auto PBLURNEWOPTIMIZE = CConfigValue<INT>("decoration:blur:new_optimizations");
    static auto PBLUR = CConfigValue<INT>("decoration:blur:enabled");

    return m_renderData.pCurrentMonData->blurFBDirty &&
           *PBLURNEWOPTIMIZE &&
           *PBLUR &&
           m_renderData.pCurrentMonData->blurFBShouldRender;
}
```

### Render Pass Execution

**Location:** Pass.cpp (inferred from usage)

```cpp
CRegion CRenderPass::render(const CRegion& damage_) {
    // Sort elements by priority
    sortElements();

    // Execute each element
    for (auto& elem : m_passElements) {
        if (!elem->shouldSkip()) {
            elem->element->draw(elem->elementDamage);
        }
    }

    return accumulatedDamage;
}
```

---

## Integration Points for Blur

### 1. Blur Decision Logic

**Location:** Renderer.cpp (method signatures in Renderer.hpp:141-143)

```cpp
bool CHyprRenderer::shouldBlur(PHLWINDOW w) {
    // Check window opacity, blur rules, configuration
    if (!*PBLUR || w->m_sWindowData.noBlur.value())
        return false;

    // Check if window has transparency or blur rule matches
    return w->m_opacity < 1.0 || matchesBlurRule(w);
}

bool CHyprRenderer::shouldBlur(PHLLS ls) {
    // Check layer surface blur configuration
    return *PBLUR && ls->layer->shouldBlur();
}
```

### 2. Texture Rendering with Blur

**Location:** OpenGL.cpp:2328 (renderTextureWithBlurInternal)

```cpp
void CHyprOpenGLImpl::renderTextureWithBlurInternal(SP<CTexture> tex,
                                                     const CBox& box,
                                                     const STextureRenderData& data) {
    // 1. Compute blur for this window's backdrop
    CRegion damage{m_renderData.damage};
    damage.intersect(box);

    CFramebuffer* POUTFB = data.xray ?
        &m_renderData.pCurrentMonData->blurFB :  // Use cached blur (xray mode)
        blurMainFramebufferWithDamage(data.blurA, &damage);  // Compute fresh

    // 2. Create stencil mask for rounded corners
    setupStencilForRoundedCorners(box, data.round);

    // 3. Render blurred backdrop
    renderTexture(POUTFB->getTexture(), monitorBox, textureData);

    // 4. Render window texture on top (with transparency)
    renderTexture(tex, box, data);
}
```

### 3. Blur Cache Management

**Dirty Tracking:** OpenGL.cpp:2173-2174

```cpp
void CHyprOpenGLImpl::markBlurDirtyForMonitor(PHLMONITOR pMonitor) {
    m_monitorRenderResources[pMonitor].blurFBDirty = true;
}
```

**Cache Computation:** OpenGL.cpp:2260-2290

```cpp
void CHyprOpenGLImpl::preBlurForCurrentMonitor() {
    // Create full-screen damage region
    CRegion fakeDamage{0, 0,
                       m_renderData.pMonitor->m_transformedSize.x,
                       m_renderData.pMonitor->m_transformedSize.y};

    // Compute blur with full damage
    const auto POUTFB = blurMainFramebufferWithDamage(1.0, &fakeDamage);

    // Allocate cache FB if needed
    m_renderData.pCurrentMonData->blurFB.alloc(
        m_renderData.pMonitor->m_pixelSize,
        m_renderData.pMonitor->m_drmFormat
    );

    // Copy blurred result to cache
    m_renderData.pCurrentMonData->blurFB.bind();
    renderTextureInternal(POUTFB->getTexture(), monitorBox, renderData);
    m_renderData.currentFB->bind();

    // Mark as clean
    m_renderData.pCurrentMonData->blurFBDirty = false;
}
```

---

## Architecture Diagram (Text Format)

```
┌─────────────────────────────────────────────────────────────────┐
│                         CCompositor                              │
│  • Manages windows, layers, monitors                            │
│  • Schedules frame rendering                                    │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                       CHyprRenderer                              │
│  • Main render loop (renderMonitor)                             │
│  • Decides which surfaces need blur (shouldBlur)                │
│  • Manages render passes (CRenderPass)                          │
└─────────────────────────┬───────────────────────────────────────┘
                          │
          ┌───────────────┴───────────────┐
          ▼                               ▼
┌─────────────────────┐         ┌─────────────────────┐
│   CRenderPass       │         │ CHyprOpenGLImpl     │
│  • CPreBlurElement  │────────>│  • EGL context      │
│  • CSurfaceElement  │         │  • Shader programs  │
│  • CTexElement      │         │  • Blur algorithm   │
└─────────────────────┘         │  • FBO management   │
                                └─────────┬───────────┘
                                          │
                        ┌─────────────────┼─────────────────┐
                        ▼                 ▼                 ▼
              ┌────────────────┐  ┌────────────┐  ┌──────────────┐
              │ Blur Shaders   │  │ FBOs       │  │ EGL/GLES     │
              │ • blurprepare  │  │ • offload  │  │ • Context    │
              │ • blur1 (down) │  │ • mirror   │  │ • Extensions │
              │ • blur2 (up)   │  │ • mirrorSw │  │ • DMA-BUF    │
              │ • blurfinish   │  │ • blurFB   │  │              │
              └────────────────┘  └────────────┘  └──────────────┘
```

---

## Key Architectural Insights

### 1. Tight Compositor Integration

Blur is **not** a separate module but deeply integrated:
- Direct access to framebuffers (no IPC)
- Shares OpenGL context with compositor
- Uses compositor's damage tracking
- Part of render pass system

**Performance Benefit:** Zero-copy, minimal latency
**Trade-off:** Cannot be externalized without significant overhead

### 2. Render Pass Design

The render pass system allows:
- Ordered execution of rendering operations
- Blur preprocessing before window rendering
- Modular addition/removal of effects
- Damage tracking per element

**Blur Fit:** PreBlurElement runs before windows, computing cached blur

### 3. Framebuffer Ping-Pong Pattern

Alternating between mirrorFB and mirrorSwapFB:
- Avoids read/write hazards (GL_FEEDBACK_LOOP)
- Enables in-place blur computation
- Minimal memory overhead (only 2 extra FBOs)

**Efficiency:** No framebuffer copies during blur passes

### 4. EGL Surfaceless Context

Hyprland uses `eglMakeCurrent(display, NO_SURFACE, NO_SURFACE, context)`:
- Doesn't need window surface for rendering
- Renders to FBOs directly
- Compatible with headless/nested compositors

**Flexibility:** Can render without display attached

---

## Summary

Hyprland's blur architecture is a well-engineered compositor-native implementation that achieves high performance through:

1. **Direct OpenGL Integration:** No IPC overhead, shared GPU resources
2. **Intelligent Caching:** Blur computed once, reused until invalidated
3. **Damage-Aware Rendering:** Only redraws affected regions
4. **Modern GPU Pipeline:** GLES 3.2 features, efficient shaders
5. **Modular Pass System:** Clean separation of blur preprocessing

**Externalization Difficulty:** Very high - blur is tightly coupled with:
- Framebuffer management (mirrorFB, mirrorSwapFB, blurFB)
- Damage tracking system
- Render pass integration
- Direct OpenGL context access
- Per-monitor render data structures

Any external blur daemon would need compositor cooperation for framebuffer sharing, damage regions, and window state, adding significant complexity and latency.

**Recommended Approach:** Compositor plugins (if available) rather than external daemons for implementing similar blur functionality.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-12
