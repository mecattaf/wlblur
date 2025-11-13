# Current Status

**Last Updated**: 2025-11-13

## Milestone m-3: Configuration System ✅ COMPLETE

The configuration system implementation is complete, marking a major milestone for the wlblur project!

### What's Complete

- ✅ **m-0**: Project Setup & Documentation
  - 6 Architecture Decision Records (ADRs)
  - Complete architecture documentation
  - Repository structure and build system

- ✅ **m-1**: libwlblur Core Implementation
  - Shader extraction complete
  - Unified parameter schema
  - EGL/DMA-BUF infrastructure
  - Kawase blur algorithm (~1,800 lines)

- ✅ **m-2**: wlblurd IPC Daemon
  - Unix socket server
  - IPC protocol handler
  - Blur node registry (~800 lines)

- ✅ **m-3**: Configuration System
  - TOML parsing with tomlc99 (~900 lines)
  - Preset management system
  - Hot reload via SIGUSR1
  - IPC protocol extension
  - Complete documentation

**Total Implementation**: ~3,500 lines of code
**Total Documentation**: ~10,000+ lines

### Key Achievements

1. **Configuration System**: Daemon-side config with preset support solves the `ext_background_effect_v1` protocol constraint
2. **Hot Reload**: Users can edit config and apply changes without any restarts
3. **Future-Proof**: Architecture supports future features (tint, vibrancy, materials) without compositor changes
4. **Minimal Integration**: Compositors can reference presets by name (~220 lines of integration code)

### Technical Highlights

**Configuration Parsing** (task-11):
- TOML config from `~/.config/wlblur/config.toml`
- Comprehensive validation
- Graceful fallbacks
- ~405 lines (config.c)

**Preset Management** (task-12):
- Hash table-based registry
- Standard presets: window, panel, hud, tooltip
- Custom user presets
- ~243 lines (presets.c)

**Hot Reload** (task-13):
- SIGUSR1 signal handling
- Safe atomic config swap
- ~89 lines (reload.c)

**IPC Protocol Extension** (task-14):
- Preset support in protocol
- Backward compatible
- ~80 lines (ipc_protocol.c)

---

## What's Next: Milestone m-4

**ScrollWM Integration** (Weeks 9-10)

Now that the configuration system is complete, we can proceed with real-world compositor integration:

### Deliverables
- [ ] ScrollWM integration module (~220 lines)
- [ ] IPC client wrapper library
- [ ] DMA-BUF export integration
- [ ] Preset-based blur requests
- [ ] Demo video

### Success Criteria
- Integration footprint <250 lines
- Visual quality matches Hyprland
- Performance <2ms overhead
- No crashes in 1-hour demo

### Why m-3 Was Critical

Compositor integration requires the preset system:

```c
// Compositor can now do this:
const char *preset = is_panel(surface) ? "panel" : "window";
wlblur_request_blur(preset);  // Just send preset name!
```

Without m-3, compositors would need to provide full parameter sets, violating our minimal integration goal.

---

## Roadmap Progress

| Milestone | Status | Lines of Code |
|-----------|--------|---------------|
| m-0: Documentation & Setup | ✅ Complete | N/A |
| m-1: libwlblur Core | ✅ Complete | ~1,800 |
| m-2: wlblurd Daemon | ✅ Complete | ~800 |
| m-3: Configuration System | ✅ Complete | ~900 |
| **m-4: ScrollWM Integration** | **🔄 Next** | **~220** |
| m-5: Production Hardening | 📋 Planned | TBD |
| m-6: Tint & Vibrancy | 📋 Planned | TBD |
| m-7: Material System | 📋 Planned | TBD |
| m-8: Multi-Compositor (niri) | 📋 Planned | ~300 |
| m-9: Community Features | 📋 Planned | TBD |

**Timeline**: On track for v1.0.0 release at m-5 (Weeks 11-12)

---

## Recent Milestones

### Milestone m-3 Highlights

See [milestone3-report.md](https://github.com/mecattaf/wlblur/blob/main/docs/post-milestone3-discussion/milestone3-report.md) for complete details.

**Key Stats**:
- ~975 lines of implementation code
- ~2,741 lines of documentation
- 4 new tasks completed (task-11 through task-14)
- 0 deviations from specifications
- All manual tests passing

**Impact**:
- Compositors can integrate with preset system
- Users get hot reload capability
- Multi-compositor consistency enabled
- Future-proof for m-6/m-7/m-8 features

---

## Get Involved

**For Users**: Try the configuration system once compositor integration is complete (m-4)

**For Compositor Developers**: Review [Integration Overview](For-Compositor-Developers-Integration-Overview) to prepare for integration

**For Contributors**: See [Contributing Guide](Development-Contributing) for how to help

---

**See Also**:
- [Project Roadmap](Project-Roadmap) — Full timeline
- [Next Steps](Next-Steps) — Immediate priorities
- [Future Vision](Future-Vision) — Long-term goals
