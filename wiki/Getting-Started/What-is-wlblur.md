# What is wlblur?

**wlblur** is an external blur daemon that brings Apple-level background blur effects to any Wayland compositor through a minimal IPC integration.

---

## The Problem

Modern Wayland compositors face a dilemma:

**Users want blur** — it looks beautiful, enhances UI hierarchy, and matches the visual quality of macOS and Windows.

**Maintainers don't want to maintain blur** — it's thousands of lines of shader code, complex damage tracking, GPU optimization, and constant bug fixes.

### The Maintenance Burden

In-compositor blur implementations like Hyprland's require:
- **50+ source files** with blur-specific code
- **~3,000 lines** of implementation
- **Complex shader management** (downsample, upsample, post-processing)
- **Sophisticated damage tracking** to prevent artifacts
- **GPU performance optimization** for different hardware
- **Constant bug fixes** and performance tuning

When we approached scroll's maintainer about adding blur, the response was clear: [**"I don't want to maintain it"**](https://github.com/mecattaf/wlblur/blob/main/docs/pre-investigation/scrollwm-maintainer-discussion.md).

---

## The Solution

**wlblur** takes all that blur complexity and moves it to a separate daemon process:

```
┌─────────────────────────────────────┐
│  Compositor (scroll/niri/sway)     │
│                                     │
│  Integration: ~220 lines            │
│  • Detect blur windows              │
│  • Send texture to daemon           │
│  • Composite blurred result         │
│  That's it!                         │
└─────────────────────────────────────┘
              │
              │ Unix Socket + DMA-BUF
              ↓
┌─────────────────────────────────────┐
│  wlblur Daemon (separate process)   │
│                                     │
│  Implementation: ~3,500 lines       │
│  • All the blur complexity          │
│  • Shaders, optimization, damage    │
│  • GPU acceleration                 │
│  • Maintained by wlblur team        │
└─────────────────────────────────────┘
```

**Result**: Compositors get blur with minimal code. Blur complexity is maintained by a dedicated team.

---

## How It Works

### 1. Minimal Compositor Integration

The compositor adds ~220 lines of code:

```c
// Detect blur-eligible windows
if (window_should_blur(window)) {
    // Export backdrop as DMA-BUF
    int dmabuf_fd = export_backdrop_texture(window);

    // Send blur request (just a preset name!)
    const char *preset = is_panel(window) ? "panel" : "window";
    wlblur_request_blur(dmabuf_fd, preset);

    // Import blurred result and composite
    int blurred_fd = wlblur_receive_result();
    composite_blurred_texture(blurred_fd, window);
}
```

### 2. Daemon Does the Heavy Lifting

The daemon (separate process) handles:
- **Shader compilation and management**
- **Multi-pass blur algorithm** (Dual Kawase, 6-8 passes)
- **GPU acceleration** (GLES3)
- **Damage tracking** (prevent artifacts)
- **Performance optimization** (FBO pooling, caching)
- **Configuration** (TOML-based with hot reload)
- **Preset system** (window, panel, hud, tooltip)

### 3. Zero-Copy Communication

**DMA-BUF** (Direct Memory Access Buffer) enables GPU-to-GPU texture sharing with zero CPU copies:

1. Compositor exports backdrop texture as DMA-BUF (file descriptor)
2. Daemon imports DMA-BUF as GL texture (no copy!)
3. Daemon applies blur shaders on GPU
4. Daemon exports result as DMA-BUF
5. Compositor imports blurred texture (no copy!)

**Total overhead**: ~0.2ms for IPC, ~1.2ms for blur = **1.4ms total**

---

## Key Benefits

### ✅ Minimal Compositor Maintenance

**In-compositor blur** (Hyprland):
- 50+ files to maintain
- ~3,000 lines of blur-specific code
- Shader debugging on user GPUs
- Performance tuning for every release

**wlblur external daemon**:
- 3-5 files in compositor (~220 lines)
- Zero blur-specific code to maintain
- Daemon handles all complexity
- Updates shipped independently

**Winner**: Compositor maintainers save hundreds of hours

---

### ✅ Independent Updates

**In-compositor**: Want Gaussian blur? Needs compositor rebuild, testing, release cycle.

**wlblur**: Just update daemon:
```bash
# Update daemon, no compositor changes
pacman -S wlblurd  # New algorithms, tint, materials, etc.

# Compositor doesn't care, it just sends preset names
```

**Winner**: Users get new features faster

---

### ✅ Multi-Compositor Support

**In-compositor**: Each compositor reimplements blur (Hyprland, Wayfire, KWin, etc.)

**wlblur**: Write once, use everywhere:
- scroll (wlroots)
- niri (Smithay/Rust)
- sway (wlroots)
- river (wlroots)
- Any future Wayland compositor

**Winner**: Consistent blur across the ecosystem

---

### ✅ Crash Isolation

**In-compositor**: Shader crash = compositor crash = lose all your windows

**wlblur**: Daemon crash = blur temporarily disabled, compositor keeps running

**Winner**: Stability

---

### ✅ Configuration Flexibility

**In-compositor**: Each compositor has different config formats:
```bash
# Hyprland
~/.config/hypr/hyprland.conf

# Wayfire
~/.config/wayfire.ini

# scroll
~/.config/scroll/config.toml
```

**wlblur**: Single config for all compositors:
```bash
# Configure once, works everywhere
~/.config/wlblur/config.toml
```

Hot reload with `SIGUSR1` — no compositor restart!

**Winner**: User experience

---

## What Makes wlblur Different?

### vs In-Compositor Blur (Hyprland, Wayfire, KWin)

**In-compositor**:
- ❌ Thousands of lines per compositor
- ❌ Reimplemented in each compositor
- ❌ Tied to compositor release cycles
- ❌ Crashes can bring down compositor
- ❌ Maintainer burden

**wlblur**:
- ✅ ~220 lines per compositor
- ✅ Write once, use everywhere
- ✅ Independent updates
- ✅ Crash isolated
- ✅ Maintained by dedicated team

### vs Client-Side Blur (Waybar, etc.)

**Client-side**:
- Only blurs client's own content
- Doesn't blur desktop backdrop
- Limited to specific applications

**wlblur**:
- Blurs any window's backdrop
- Works system-wide
- Compositor-level integration

---

## Use Cases

### Regular Users

**Want**: Beautiful blur effects like macOS/Windows
**Get**: Professional blur without compositor limitations

### Compositor Developers

**Want**: Minimal code to maintain
**Get**: ~220 lines vs thousands, all complexity externalized

### Multi-Compositor Users

**Want**: Consistent appearance when switching compositors
**Get**: Same `~/.config/wlblur/config.toml` works everywhere

### Power Users

**Want**: Fine-grained control and experimentation
**Get**: TOML config with hot reload, custom presets

---

## Technical Highlights

### 🎨 Apple-Quality Blur

Dual Kawase algorithm (same as Hyprland) with:
- Vibrancy system (HSL saturation boost)
- Post-processing (brightness, contrast, saturation, noise)
- Future: Tint overlays, material system (macOS Ventura parity)

### 🚀 Performance

- **DMA-BUF zero-copy**: No CPU texture transfers
- **~1.4ms total latency**: 0.2ms IPC + 1.2ms blur
- **Sophisticated damage tracking**: Minimize GPU work
- **FBO pooling**: Reduce allocation overhead

### 🔧 Configuration

- **TOML format**: Human-friendly, well-typed
- **Preset system**: window, panel, hud, tooltip
- **Hot reload**: `killall -USR1 wlblurd` applies changes instantly
- **Fallback hierarchy**: preset → override → defaults → hardcoded

---

## Project Status

**Current**: Milestone m-3 Complete ✅

- ✅ Core library implementation (~1,800 lines)
- ✅ IPC daemon (~800 lines)
- ✅ Configuration system (~900 lines)
- ✅ Comprehensive documentation
- 🔄 ScrollWM integration (next)

**Timeline**: v1.0.0 public release planned for m-5

See [Current Status](../Roadmap/Current-Status.md) for details.

---

## Quick Start

Ready to try wlblur? See the [Quick Start Guide](Quick-Start.md).

Compositor developer? See [Integration Overview](../For-Compositor-Developers/Integration-Overview.md).

---

**Next**: [Installation →](Installation.md)
