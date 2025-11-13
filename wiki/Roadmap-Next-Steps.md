# Next Steps

## Current Focus: Milestone 4

### ScrollWM Integration (m-4)

**Goal:** First compositor integration proof-of-concept

**Tasks:**
- [ ] Implement ScrollWM integration code (~220 lines)
- [ ] Test DMA-BUF export/import
- [ ] Verify blur visible on transparent windows
- [ ] Performance validation (<1.5ms)
- [ ] Documentation of integration process

**Timeline:** 4-6 weeks

**Status:** In planning

See: [m-4 Plan](Milestones-m-4-ScrollWM-Integration)

## Short-Term (Next 3 Months)

### Production Hardening

**Stability:**
- Error recovery and handling
- Resource limit enforcement
- Graceful degradation
- Logging improvements

**Performance:**
- Blur caching for static content
- Damage tracking optimization
- Memory usage profiling
- GPU hang detection

**Testing:**
- Integration test suite
- Stress testing
- Multi-compositor testing
- Performance benchmarking

### Documentation

- Integration guides for other compositors
- Performance tuning guide
- Troubleshooting FAQ
- Video tutorials

## Medium-Term (3-6 Months)

### Multi-Compositor Support

**Target Compositors:**
- **niri** (Rust, scrolling tiling)
- **Sway** (i3-compatible, most popular)
- **River** (Zig, dynamic tiling)
- **Labwc** (Openbox-like, stacking)

**Deliverables:**
- Rust bindings for niri
- Integration examples for each
- Cross-compositor compatibility testing
- Preset sharing between compositors

### Additional Features

**Blur Algorithms:**
- Gaussian blur (higher quality)
- Box blur (faster)
- Bokeh blur (artistic)

**Material System:**
- Vibrancy enhancement (Hyprland-style)
- Tint/color overlay
- Noise texture
- Dynamic adjustment

## Long-Term (6-12 Months)

### Advanced Features

**Wayland Protocol Extension:**
- Define `ext-background-effect-v1` protocol
- Client-driven blur requests
- Standardization with Wayland community
- Protocol implementation in compositors

**Performance Optimizations:**
- Compute shader path (OpenGL 4.3+)
- Vulkan backend
- Half-float textures
- Shader compilation caching

**macOS Parity:**
- Light/dark material variants
- Behind-window blur
- In-window blur regions
- Frosted glass aesthetic

See: [macOS Parity Vision](macOS-Parity-Vision)

### Hyprland Feature Parity

- xray mode (exclude certain windows)
- Blur caching with dirty tracking
- Per-monitor FBO optimization
- Ignore opacity support

See: [Hyprland Parity Plan](Hyprland-Parity-Plan)

## Future Possibilities

### Experimental Features

**Advanced Effects:**
- Real-time reflection
- Glass refraction simulation
- Depth-of-field blur
- Motion blur

**Platform Support:**
- FreeBSD/OpenBSD support
- Vulkan-only path
- Wayland-less testing mode

**Ecosystem Integration:**
- Desktop environment integration (GNOME, KDE)
- Window manager plugins
- Theme engine integration

## Community Requests

Track feature requests: [GitHub Issues](https://github.com/mecattaf/wlblur/issues)

**Most requested:**
- Different blur algorithms
- Per-window blur settings
- Blur animations
- Theme system integration

## How to Help

**For Users:**
- Test on different hardware
- Report bugs and issues
- Suggest compositor integrations
- Share configuration presets

**For Developers:**
- Implement compositor integrations
- Contribute performance optimizations
- Write tests
- Improve documentation

**For Compositor Maintainers:**
- Integrate wlblur into your compositor
- Provide feedback on API design
- Help with protocol standardization

See: [Contributing Guide](Development-Contributing)

## See Also

- [Project Roadmap](Project-Roadmap) - Long-term vision
- [Current Status](Current-Status) - Project status
- [Milestones Overview](Milestones-Overview) - All milestones
