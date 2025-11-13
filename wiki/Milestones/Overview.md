# Milestones

## Completed Milestones

### ✅ Milestone 0: Documentation & Setup
- [Plan](m-0-Documentation)
- Status: Complete
- Established project structure, investigation framework, and documentation standards

### ✅ Milestone 1: libwlblur Core
- [Plan](m-1-libwlblur)
- Status: Complete (~1,800 lines)
- Core blur rendering library with EGL context, shader management, and Kawase algorithm

### ✅ Milestone 2: wlblurd Daemon
- [Plan](m-2-Daemon)
- [Completion Report](m-2-Complete) ⭐
- Status: Complete (~800 lines)
- External daemon with IPC protocol, DMA-BUF handling, and blur node management

### ✅ Milestone 3: Configuration System
- [Plan](m-3-Configuration)
- [Completion Report](m-3-Complete) ⭐
- Status: Complete (~900 lines)
- TOML-based configuration with presets, hot reload via SIGUSR1, and validation

## Current Milestone

### 🔄 Milestone 4: ScrollWM Integration
- [Plan](m-4-ScrollWM-Integration)
- Status: Next up
- Goal: First compositor integration proof-of-concept
- Deliverables:
  - ScrollWM integration code (~220 lines)
  - DMA-BUF export/import working
  - Visible blur on transparent windows
  - Performance validation

## Future Milestones

### 📋 Milestone 5+
- [Future Plans](m-5-Future)
- Includes: Production hardening, advanced features, multi-compositor support
- Areas:
  - Additional compositors (niri, Sway, River)
  - Performance optimizations (caching, compute shaders)
  - Material system (vibrancy, tint, noise)
  - Wayland protocol extension

## Progress Summary

**Code Written:** ~3,500 lines
- libwlblur: ~1,800 lines
- wlblurd: ~800 lines
- Configuration: ~900 lines

**Current Phase:** m-4 (ScrollWM Integration)

**Next Phase:** Production hardening and multi-compositor support

## See Also

- [Current Status](../Roadmap/Current-Status) - Detailed status of all components
- [Project Roadmap](../Roadmap/Project-Roadmap) - Long-term vision
- [ADRs](../Architecture-Decisions/Overview) - Architectural decisions
