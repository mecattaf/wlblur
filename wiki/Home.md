# wlblur Wiki

**Compositor-agnostic blur daemon for Wayland — because your compositor shouldn't maintain blur code.**

Welcome to the wlblur documentation! This wiki provides comprehensive information for users, compositor developers, and contributors.

---

## What is wlblur?

**wlblur** is an external blur daemon that provides Apple-level background blur effects to any Wayland compositor via a minimal IPC integration. Instead of forcing compositors to maintain thousands of lines of blur code, wlblur handles all blur processing in a separate process using DMA-BUF zero-copy transfers.

**Key Benefits**:
- **Minimal Integration**: ~220 lines of compositor code vs 50+ files for in-compositor solutions
- **Crash Isolation**: Daemon failures don't affect compositor
- **Independent Updates**: New blur algorithms ship without compositor rebuilds
- **Multi-Compositor**: Works with scroll, niri, sway, river, and more
- **Apple-Quality Blur**: Dual Kawase algorithm with vibrancy controls

---

## Quick Links

### For Users
- 📘 [What is wlblur?](Getting-Started-What-is-wlblur) - 2-minute introduction
- 🚀 [Quick Start](Getting-Started-Quick-Start) - Get started in 5 minutes
- 🔧 [Configuration Guide](User-Guide-Configuration) - Full configuration reference
- ❓ [FAQ](Getting-Started-FAQ) - Common questions answered
- 🐛 [Troubleshooting](User-Guide-Troubleshooting) - Common issues and solutions

### For Compositor Developers
- 👨‍💻 [Integration Overview](For-Compositor-Developers-Integration-Overview) - High-level integration approach
- ✅ [Integration Checklist](For-Compositor-Developers-Integration-Checklist) - Step-by-step guide
- 📚 [API Reference](For-Compositor-Developers-API-Reference) - IPC protocol documentation
- 💡 [Example Integration](For-Compositor-Developers-Example-Integration) - Code walkthrough

### Architecture & Design
- 🏗️ [System Overview](Architecture-System-Overview) - High-level architecture
- 📖 [Architecture Decisions](Architecture-Decisions-Overview) - ADRs explaining key design choices
- 🗺️ [Project Roadmap](Roadmap-Project-Roadmap) - Full roadmap m-0 through m-9

### For Contributors
- 🤝 [Contributing Guide](Development-Contributing) - How to contribute
- 🔨 [Building from Source](Development-Building-from-Source) - Developer build guide
- 📋 [Code Style](Development-Code-Style) - Coding conventions
- 🧪 [Testing](Development-Testing) - Test procedures

---

## Current Status

**Milestone m-3 Complete!** ✅

- ✅ **m-0**: Documentation & Setup
- ✅ **m-1**: libwlblur Core (~1,800 lines)
- ✅ **m-2**: wlblurd Daemon (~800 lines)
- ✅ **m-3**: Configuration System (~900 lines)
- 🔄 **m-4**: ScrollWM Integration (next)

**What's Working**:
- Core blur library with Dual Kawase algorithm
- IPC daemon with Unix socket protocol
- TOML-based configuration with preset support
- Hot reload via SIGUSR1
- Comprehensive documentation

**What's Next**:
- Compositor integration (ScrollWM)
- v1.0.0 public release
- Multi-compositor expansion

See [Current Status](Roadmap-Current-Status) for details.

---

## Why External Daemon?

Compositors like scroll's maintainer explicitly [don't want blur](https://github.com/mecattaf/wlblur/blob/main/docs/pre-investigation/scrollwm-maintainer-discussion.md) — but their users do. The external daemon solves this by removing the maintenance burden.

**Comparison**:

| Aspect | In-Compositor (Hyprland) | wlblur (Daemon) |
|--------|--------------------------|-----------------|
| Integration | 50+ files, 1000s of lines | 3-5 files, ~220 lines |
| Maintenance | Compositor maintainer burden | Independent daemon team |
| Updates | Requires compositor rebuild | Daemon updates independently |
| Crash impact | Can crash compositor | Daemon restarts, compositor unaffected |
| Multi-compositor | Single compositor only | Works everywhere |

See [Why External Daemon?](Architecture-Decisions-ADR-001-External-Daemon) for complete rationale.

---

## Configuration Example

wlblur uses daemon-side configuration with named presets:

```toml
# ~/.config/wlblur/config.toml

[defaults]
algorithm = "kawase"
num_passes = 3
radius = 5.0
saturation = 1.1

[presets.window]
radius = 8.0
num_passes = 3

[presets.panel]
radius = 4.0
num_passes = 2
brightness = 1.05
```

**Benefits**:
- **Multi-compositor consistency**: Same blur across all compositors
- **Hot reload**: Edit config, send `SIGUSR1`, changes apply instantly
- **Future-proof**: New features (tint, materials) work without compositor updates

See [Configuration Guide](User-Guide-Configuration) for complete documentation.

---

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Compositor Process                     │
│  (scroll/niri/sway)                     │
│                                         │
│  Minimal blur client (~220 lines):     │
│  • Detect blur-eligible windows         │
│  • Export backdrop texture as DMA-BUF   │
│  • Send blur request via Unix socket    │
│  • Import blurred result                │
│  • Composite into scene graph           │
└─────────────────────────────────────────┘
              │
              │ Unix Socket + DMA-BUF (zero-copy)
              ↓
┌─────────────────────────────────────────┐
│  wlblur Daemon (separate process)       │
│                                         │
│  • Dual Kawase blur algorithm           │
│  • GPU-accelerated GLES3 shaders        │
│  • Configuration system with presets    │
│  • Hot reload support                   │
│  • Independent updates                  │
│  • Crash isolation                      │
└─────────────────────────────────────────┘
```

See [System Overview](Architecture-System-Overview) for detailed architecture.

---

## Feature Highlights

### 🎨 Apple-Quality Blur
Dual Kawase algorithm with vibrancy controls matching macOS blur quality.

### 🔌 Minimal Integration
~220 lines of IPC client code vs thousands for in-compositor solutions.

### 🚀 Zero-Copy Performance
DMA-BUF texture sharing eliminates CPU-side copies (~1.4ms total latency).

### 🛡️ Crash Isolation
Daemon crashes don't affect compositor — blur silently disables until restart.

### 🔄 Independent Updates
New blur algorithms ship without compositor rebuilds.

### 🌐 Multi-Compositor
Works with any wlroots compositor (scroll, sway, river) and Smithay compositors (niri).

### ⚡ Hot Reload
Edit config, send SIGUSR1, changes apply instantly without any restarts.

### 🎛️ Preset System
Standard presets (window, panel, hud, tooltip) + custom user presets.

---

## Documentation Structure

### 📘 Getting Started
Learn about wlblur and get set up quickly.

- [What is wlblur?](Getting-Started-What-is-wlblur)
- [Installation](Getting-Started-Installation)
- [Quick Start](Getting-Started-Quick-Start)
- [FAQ](Getting-Started-FAQ)

### 🔧 User Guide
Configure and use wlblur on your system.

- [Configuration](User-Guide-Configuration)
- [Presets](User-Guide-Presets)
- [Hot Reload](User-Guide-Hot-Reload)
- [Troubleshooting](User-Guide-Troubleshooting)

### 👨‍💻 For Compositor Developers
Integrate wlblur into your compositor.

- [Integration Overview](For-Compositor-Developers-Integration-Overview)
- [Integration Checklist](For-Compositor-Developers-Integration-Checklist)
- [API Reference](For-Compositor-Developers-API-Reference)
- [Example Integration](For-Compositor-Developers-Example-Integration)

### 🏗️ Architecture
Understand how wlblur works internally.

- [System Overview](Architecture-System-Overview)
- [libwlblur Internals](Architecture-libwlblur-Internals)
- [Daemon Architecture](Architecture-Daemon-Architecture)
- [IPC Protocol](Architecture-IPC-Protocol)
- [Configuration System](Architecture-Configuration-System)

### 📖 Architecture Decisions
Learn why key design decisions were made.

- [ADR-001: External Daemon](Architecture-Decisions-ADR-001-External-Daemon)
- [ADR-002: DMA-BUF](Architecture-Decisions-ADR-002-DMA-BUF)
- [ADR-003: Kawase Algorithm](Architecture-Decisions-ADR-003-Kawase-Algorithm)
- [ADR-004: IPC Protocol](Architecture-Decisions-ADR-004-IPC-Protocol)
- [ADR-005: SceneFX Extraction](Architecture-Decisions-ADR-005-SceneFX-Extraction)
- [ADR-006: Daemon Configuration](Architecture-Decisions-ADR-006-Daemon-Config)

### 🗺️ Roadmap & Milestones
See where wlblur is headed.

- [Project Roadmap](Roadmap-Project-Roadmap)
- [Current Status](Roadmap-Current-Status)
- [Next Steps](Roadmap-Next-Steps)
- [Future Vision](Roadmap-Future-Vision)

### 🏁 Milestones
Detailed milestone documentation.

- [m-0: Documentation](Milestones-m-0-Documentation)
- [m-1: libwlblur](Milestones-m-1-libwlblur)
- [m-2: Daemon](Milestones-m-2-Daemon)
- [m-3: Configuration](Milestones-m-3-Configuration)
- [m-4: ScrollWM Integration](Milestones-m-4-ScrollWM-Integration)
- [Future Milestones](Milestones-Future-Milestones)

### 🤝 Development
Contribute to wlblur.

- [Contributing](Development-Contributing)
- [Building from Source](Development-Building-from-Source)
- [Code Style](Development-Code-Style)
- [Testing](Development-Testing)

---

## Community

**Repository**: [github.com/mecattaf/wlblur](https://github.com/mecattaf/wlblur)

**Issues**: [GitHub Issues](https://github.com/mecattaf/wlblur/issues) for bug reports and feature requests

**Discussions**: [GitHub Discussions](https://github.com/mecattaf/wlblur/discussions) for questions and ideas

---

## License

MIT License — see [LICENSE](https://github.com/mecattaf/wlblur/blob/main/LICENSE) for details.

---

## Acknowledgments

- **Hyprland** — Inspiration for performance optimizations and vibrancy system
- **Wayfire** — Algorithm variety and clean plugin abstraction
- **SceneFX** — Scene graph integration patterns and damage tracking
- **wlroots** — Foundation for Wayland compositor ecosystem
- **[`dawsers`](https://github.com/dawsers)** (scroll maintainer) — For validating the need for external daemon approach

---

**Built by [@mecattaf](https://github.com/mecattaf) and contributors**

**Last Updated**: 2025-11-13
