# wlblur

**Compositor-agnostic blur daemon for Wayland — because your compositor shouldn't maintain blur code.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Status: Planning](https://img.shields.io/badge/Status-Planning-yellow.svg)](#project-status)

---

## What is wlblur?

**wlblur** is an external blur daemon that provides Apple-level background blur effects to any Wayland compositor via a minimal IPC integration. Instead of forcing compositors to maintain thousands of lines of blur code, shaders, and optimization logic, wlblur handles all blur processing in a separate process using DMA-BUF zero-copy transfers.

**Integration cost:** ~220 lines of compositor code vs 50+ files for in-compositor solutions.

```
┌─────────────────────────────────────────────────────────────┐
│                    Compositor Process                       │
│                     (scroll/niri/sway)                      │
│                                                             │
│  Existing compositor code (untouched):                      │
│  ├─ Scene graph                                             │
│  ├─ Rendering pipeline                                      │
│  └─ Window management                                       │
│                                                             │
│  NEW: Minimal blur client (~220 lines):                    │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  • Detect blur-eligible windows                      │  │
│  │  • Export backdrop texture as DMA-BUF                │  │
│  │  • Send blur request via Unix socket                 │  │
│  │  • Import blurred result                             │  │
│  │  • Composite into scene graph                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ Unix Socket + DMA-BUF (zero-copy)
                            ↓
┌─────────────────────────────────────────────────────────────┐
│              wlblur Daemon (separate process)               │
│                                                             │
│  • Dual Kawase blur (Hyprland/macOS quality)               │
│  • Gaussian/Box/Bokeh algorithms (Wayfire variety)         │
│  • Vibrancy/saturation controls                            │
│  • Sophisticated damage tracking                           │
│  • GPU-accelerated GLES3 shaders                           │
│  • Independent updates (no compositor rebuild)             │
│  • Multi-compositor support                                │
│                                                             │
│  Crash isolation: daemon failures don't affect compositor  │
└─────────────────────────────────────────────────────────────┘
```

---

## Why External Daemon?

### vs In-Compositor Blur

| Aspect | In-Compositor (e.g., Hyprland) | wlblur (External Daemon) |
|--------|-------------------------------|--------------------------|
| **Integration** | 50+ files, thousands of lines | 3-5 files, ~220 lines |
| **Maintenance** | Compositor maintainer burden | Independent daemon team |
| **Shaders** | Embedded in compositor | In daemon only |
| **Updates** | Requires compositor rebuild | Daemon updates independently |
| **Crash impact** | Can crash compositor | Daemon restarts, compositor unaffected |
| **Multi-compositor** | Single compositor only | Works with scroll, niri, sway, river, etc. |
| **Performance** | 0.8-1.5ms | ~1.4ms (1.2ms blur + 0.2ms IPC) |

**Key insight:** Compositors like scroll's maintainer explicitly [don't want blur](docs/pre-investigation/scrollwm-maintainer-discussion.md) — but their users do. External daemon solves this by removing the maintenance burden.

---

## Features

- **🎨 Apple-Quality Blur**
  Dual Kawase algorithm with vibrancy controls matching macOS blur quality

- **🔌 Minimal Integration**
  ~220 lines of IPC client code vs thousands for in-compositor solutions

- **🚀 Zero-Copy Performance**
  DMA-BUF texture sharing eliminates CPU-side copies (~1.4ms total latency)

- **🛡️ Crash Isolation**
  Daemon crashes don't affect compositor — blur silently disables until restart

- **🔄 Independent Updates**
  New blur algorithms ship without compositor rebuilds

- **🌐 Multi-Compositor**
  Works with any wlroots compositor (scroll, sway, river) and Smithay compositors (niri)

- **⚡ Sophisticated Damage Tracking**
  Three-level damage expansion prevents artifacts while minimizing GPU work

- **🎛️ Algorithm Variety**
  Dual Kawase, Gaussian, Box, and Bokeh blur — choose quality vs performance

---

## Quick Start

> **⚠️ Project Status:** Currently in planning/investigation phase. Implementation has not started yet.

Once implemented, usage will be:

```bash
# Start the blur daemon
wlblur-daemon --socket /run/user/1000/wlblur.sock

# Configure your compositor (example for scroll)
# Add to scroll config:
blur_daemon_socket /run/user/1000/wlblur.sock
blur_enabled yes
blur_radius 40
```

See [INSTALL.md](docs/INSTALL.md) *(coming soon)* for compositor-specific integration guides.

---

## Project Status

**Current Phase:** Investigation & Planning (Milestone m-0)

- ✅ Investigation complete: Analyzed Hyprland, Wayfire, and SceneFX blur implementations
- ✅ Architecture validated: External daemon approach via DMA-BUF + IPC
- ✅ Performance validated: ~1.4ms total latency (1.2ms blur + 0.2ms IPC)
- 🔄 **Currently:** Writing comprehensive documentation and roadmap
- ⏳ **Next:** Core daemon implementation (Milestone m-1)

See [backlog/tasks/](backlog/tasks/) for detailed task breakdown.

---

## Architecture Deep Dive

### Why DMA-BUF?

**DMA-BUF** (Direct Memory Access Buffer) is a Linux kernel framework for zero-copy buffer sharing between GPU and different processes:

- ✅ No CPU-side texture downloads/uploads
- ✅ GPU-to-GPU transfers only (~0.2ms overhead)
- ✅ Standard wlroots API (`wlr_buffer_get_dmabuf`)
- ✅ No custom kernel modules needed

### Integration Workflow

1. **Compositor** renders backdrop (everything behind blur window) to texture
2. **Compositor** exports texture as DMA-BUF file descriptor
3. **Compositor** sends blur request + DMA-BUF FD over Unix socket
4. **Daemon** imports DMA-BUF as GL texture (zero-copy)
5. **Daemon** applies blur shaders (Dual Kawase: 6 passes)
6. **Daemon** exports blurred result as DMA-BUF
7. **Daemon** sends DMA-BUF FD back to compositor
8. **Compositor** imports blurred texture and composites into scene graph

**Total roundtrip:** ~1.4ms (measured in SceneFX investigation)

See [docs/post-investigation/blur-daemon-approach.md](docs/post-investigation/blur-daemon-approach.md) for detailed architectural rationale.

---

## Comparison to Existing Solutions

### Hyprland

**Approach:** Monolithic in-compositor blur with heavy optimizations
**Strengths:** Excellent performance (0.8-1.5ms), vibrancy system, aggressive caching
**Limitations:** Tightly coupled to Hyprland, requires forking entire compositor for other projects

**wlblur advantage:** Extract Hyprland's optimization techniques into reusable daemon

### SceneFX

**Approach:** wlroots scene graph replacement with blur support
**Strengths:** Multi-compositor potential (wlroots-based), clean scene graph integration
**Limitations:** Requires replacing core wlroots scene graph, still in-process

**wlblur advantage:** Works with *stock* wlroots, no scene graph replacement needed

### Wayfire Blur Plugin

**Approach:** Plugin architecture with multiple blur algorithms
**Strengths:** Clean abstraction, algorithm variety (Kawase/Gaussian/Box/Bokeh)
**Limitations:** Still in-process, Wayfire-specific plugin API

**wlblur advantage:** Compositor-agnostic, works across different compositor frameworks

See [docs/post-investigation/comprehensive-synthesis1.md](docs/post-investigation/comprehensive-synthesis1.md) for full comparative analysis.

---

## Documentation

- **[Investigation Summary](docs/post-investigation/comprehensive-synthesis1.md)** — Analysis of Hyprland, Wayfire, and SceneFX blur implementations
- **[Daemon Approach Rationale](docs/post-investigation/blur-daemon-approach.md)** — Why external daemon beats in-compositor integration
- **[Hyprland Parity Plan](docs/post-investigation/hyprland-parity-explanation.md)** — Roadmap to match Hyprland blur quality
- **[macOS Parity Explained](docs/post-investigation/macos-parity-explained.md)** — How wlblur will match Apple's blur quality
- **[scroll Maintainer Discussion](docs/pre-investigation/scrollwm-maintainer-discussion.md)** — Why some compositors reject built-in blur

### Investigation Docs

- [Hyprland Investigation](docs/investigation/hyprland-investigation/)
- [SceneFX Investigation](docs/investigation/scenefx-investigation/)
- [Wayfire Investigation](docs/investigation/wayfire-investigation/)

---

## Contributing

Contributions are welcome! This project is in early planning stages.

**Areas where help is needed:**
- Core daemon implementation (GLES3 rendering, IPC server)
- Compositor integration libraries (wlroots, Smithay)
- Performance benchmarking tools
- Documentation and examples

See [CONTRIBUTING.md](CONTRIBUTING.md) *(coming soon)* for guidelines.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Acknowledgments

- **Hyprland** — Inspiration for performance optimizations and vibrancy system
- **Wayfire** — Algorithm variety and clean plugin abstraction
- **SceneFX** — Scene graph integration patterns and damage tracking
- **wlroots** — Foundation for Wayland compositor ecosystem
- **scroll maintainer** — For validating the need for external daemon approach

---

**Built by [@mecattaf](https://github.com/mecattaf) and contributors**
