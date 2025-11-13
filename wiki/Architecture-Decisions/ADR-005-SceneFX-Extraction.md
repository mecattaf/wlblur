# ADR-005: SceneFX as Primary Code Extraction Source

**Status**: Proposed
**Date**: 2025-01-15

## Context

Our blur daemon needs to implement production-quality blur with minimal development time. Three major Wayland compositors have production blur implementations we can learn from:

1. **Hyprland**: Custom renderer, deeply integrated, ~2000 lines blur code
2. **Wayfire**: Plugin architecture, 4 algorithms, ~800 lines blur code
3. **SceneFX**: wlroots scene graph replacement, Dual Kawase, ~1500 lines blur code

We need to decide which implementation(s) to extract code from, considering:
- Code quality and documentation
- Ease of extraction (coupling to compositor)
- Feature completeness (damage tracking, optimization)
- License compatibility
- Maintenance status

## Decision

**We will primarily extract code from SceneFX for core infrastructure (DMA-BUF, damage tracking, FBO management) and Wayfire for algorithm implementations (Kawase, Gaussian, Box, Bokeh), with Hyprland shaders for vibrancy enhancement.**

### Rationale for Hybrid Approach

**SceneFX provides best foundation because:**
1. Designed for wlroots replacement → compositor-agnostic patterns
2. Complete DMA-BUF import/export infrastructure
3. Sophisticated 3-level damage tracking with artifact prevention
4. Per-output FBO management (exactly what daemon needs)
5. Well-documented investigation findings

**Wayfire provides best algorithms because:**
1. Cleanest plugin architecture → minimal coupling
2. Four production algorithms (Kawase, Gaussian, Box, Bokeh)
3. Pure OpenGL ES 2.0 shaders (most portable)
4. Algorithm abstraction base class (wf_blur_base)
5. ~85% code reusable in daemon

**Hyprland provides best enhancements because:**
1. Vibrancy system (HSL saturation boost)
2. Optimized shader variants
3. Color management functions

## Alternatives Considered

### Alternative 1: Extract Only From Wayfire

**Approach:** Use Wayfire as sole source for all blur code.

**Pros:**
- Single source → simpler licensing
- Cleanest architecture (plugin system)
- Multiple algorithms ready to go
- Proven algorithm quality

**Cons:**
- **Missing DMA-BUF patterns**: Wayfire doesn't use DMA-BUF (in-process)
- **Simpler damage tracking**: No artifact prevention (SceneFX is better)
- **No vibrancy**: Missing Apple-level color enhancement
- **FBO management is plugin-specific**: Needs adaptation

**Why Insufficient:** Wayfire has best algorithms but lacks cross-process patterns we need.

### Alternative 2: Extract Only From Hyprland

**Approach:** Port Hyprland's blur implementation to standalone daemon.

**Pros:**
- Most feature-complete (caching, vibrancy, xray mode)
- Best performance optimizations
- Best Apple aesthetic match
- Active development

**Cons:**
- **Tightly coupled**: Deeply integrated with CHyprRenderer
- **Complex extraction**: ~2000 lines tightly coupled to compositor state
- **Per-monitor assumptions**: Assumes CHyprMonitor* everywhere
- **License concerns**: Hyprland is BSD-3-Clause (compatible but different from SceneFX/Wayfire MIT)

**Why Insufficient:** Too much extraction work, tight coupling makes daemon port difficult.

### Alternative 3: Extract Only From SceneFX

**Approach:** Use SceneFX as sole source.

**Pros:**
- Best DMA-BUF patterns
- Best damage tracking
- Designed for multi-compositor use
- Good documentation from investigation

**Cons:**
- **Single algorithm**: Only Dual Kawase (no Gaussian, Box, Bokeh)
- **No vibrancy**: Missing color enhancement
- **wlroots-specific patterns**: Some wlroots types need translation
- **Newer codebase**: Less battle-tested than Wayfire/Hyprland

**Why Insufficient:** Need algorithm variety. Wayfire has 4 algorithms vs SceneFX's 1.

### Alternative 4: Write From Scratch

**Approach:** Implement blur algorithms from academic papers.

**Pros:**
- No licensing concerns
- Exactly tailored to our needs
- Learning opportunity

**Cons:**
- **High risk**: Might not match production quality
- **Long timeline**: 6-8 weeks vs 2-3 weeks for extraction
- **Unknown bugs**: Production code is battle-tested
- **No shortcuts**: Must implement all optimizations ourselves

**Why Rejected:** Unnecessary risk and delay. Production code is proven and available.

### Alternative 5: Fork Entire Compositor

**Approach:** Fork SceneFX or Wayfire, rip out compositor parts.

**Pros:**
- Get everything (all code)
- Known working system

**Cons:**
- **Massive codebase**: 50,000+ lines to understand
- **Tight coupling**: Hard to separate blur from scene graph
- **Maintenance burden**: Must track upstream changes
- **Wrong license**: Fork implies GPL/MIT obligations

**Why Rejected:** We only need ~2000 lines, not 50,000. Extraction is cleaner.

## Consequences

### Positive

1. **Fast MVP Timeline**: Reuse ~1400 lines of proven code
   - SceneFX DMA-BUF: ~300 lines
   - SceneFX damage tracking: ~200 lines
   - Wayfire algorithms: ~600 lines
   - Wayfire base classes: ~200 lines
   - Hyprland vibrancy: ~100 lines

2. **Production Quality Immediately**: Battle-tested code
   - SceneFX used in SwayFX (thousands of users)
   - Wayfire blur used for years (stable)
   - Hyprland vibrancy loved by community

3. **Algorithm Variety**: 4+ algorithms from day one
   - Kawase (default, fast)
   - Gaussian (high quality)
   - Box (very fast)
   - Bokeh (artistic)

4. **Best-in-Class Components**: Cherry-pick best parts
   - SceneFX damage tracking (most sophisticated)
   - Wayfire algorithm abstraction (cleanest)
   - Hyprland vibrancy (best Apple match)

5. **Clear Code Lineage**: Document what came from where
   - Easier maintenance (know where to check for upstream fixes)
   - Proper attribution in comments
   - License compliance

6. **License Compatibility**: All MIT or BSD-3-Clause
   - SceneFX: MIT
   - Wayfire: MIT
   - Hyprland: BSD-3-Clause
   - All compatible with our MIT license

7. **Upstream Benefit**: Bug fixes flow back
   - If we find bugs, can report/patch upstream
   - Community benefits from our work
   - Good open source citizenship

### Negative

1. **Three Codebases to Understand**: More complexity
   - Must understand SceneFX patterns
   - Must understand Wayfire patterns
   - Must understand Hyprland shaders
   - Mitigation: Investigation docs already done this

2. **Integration Work**: Glue code needed
   - SceneFX DMA-BUF + Wayfire algorithms don't automatically fit
   - Must write adapter layer (~200 lines)
   - Mitigation: Clear interfaces make this straightforward

3. **Maintenance Tracking**: Watch three upstreams
   - SceneFX might add optimizations
   - Wayfire might add algorithms
   - Hyprland might improve vibrancy
   - Mitigation: Subscribe to release notifications

4. **License Attribution**: Must credit all three
   - Copyright notices in extracted files
   - License files in repo
   - Documentation of origins
   - Mitigation: Standard open source practice

5. **Code Style Variance**: Three different styles
   - SceneFX: wlroots C style
   - Wayfire: C++ with modern features
   - Hyprland: C++ with custom patterns
   - Mitigation: Normalize to consistent style during extraction

## Extraction Plan

### Phase 1: DMA-BUF Infrastructure (from SceneFX)

**Files to Extract:**
- `render/fx_renderer/fx_texture.c:353-404` → `daemon/dmabuf_import.c`
- `render/egl.c:751-841` → `daemon/egl_context.c`
- `render/fx_renderer/fx_framebuffer.c:117-158` → `daemon/fbo_pool.c`

**Adaptations:**
- Remove wlroots types (wlr_buffer → int fd + attributes)
- Remove wlr_addon pattern (not needed in daemon)
- Add daemon-specific buffer registry

**Timeline:** Week 3 (DMA-BUF integration phase)

**Reusability:** ~90% (very clean separation)

### Phase 2: Damage Tracking (from SceneFX)

**Concepts to Extract:**
- Three-level expansion pattern (visibility, render pass, artifact prevention)
- Blur size formula: `2^(passes + 1) × radius`
- Padding save/restore logic

**Files to Study:**
- `types/scene/wlr_scene.c:680-684` (visibility expansion)
- `render/fx_renderer/fx_pass.c:889` (render pass expansion)
- `types/scene/wlr_scene.c:2963-3010` (artifact prevention)

**Adaptations:**
- Virtual scene graph in daemon (not wlroots scene graph)
- Compositor sends pre-expanded damage regions
- Daemon does artifact prevention locally

**Timeline:** Week 6 (damage tracking phase)

**Reusability:** ~60% (concepts, not direct code)

### Phase 3: Kawase Blur (from Wayfire)

**Files to Extract:**
- `plugins/blur/kawase.cpp:25-120` → `daemon/kawase_blur.c`
- `plugins/blur/blur.hpp:91-155` → `daemon/blur_algorithm.h`
- `plugins/blur/blur-base.cpp:150-280` → `daemon/blur_base.c`

**Shaders to Copy:**
- Kawase downsample/upsample shaders (GLSL ES 2.0)

**Adaptations:**
- C++ → C translation (simple, mostly struct → class)
- Remove Wayfire plugin API dependencies
- Remove wf::render_target_t (use GLuint FBO directly)

**Timeline:** Week 4-5 (blur renderer phase)

**Reusability:** ~85% (very clean)

### Phase 4: Additional Algorithms (from Wayfire)

**Files to Extract:**
- `plugins/blur/gaussian.cpp` → `daemon/gaussian_blur.c`
- `plugins/blur/box.cpp` → `daemon/box_blur.c`
- `plugins/blur/bokeh.cpp` → `daemon/bokeh_blur.c`

**Timeline:** Week 10 (additional algorithms phase)

**Reusability:** ~80%

### Phase 5: Vibrancy Enhancement (from Hyprland)

**Shaders to Extract:**
- `src/render/shaders/glsl/blur1.frag:80-120` (saturation boost)
- `src/render/shaders/glsl/CM.glsl:150-180` (HSL color space)
- `src/render/shaders/glsl/blurfinish.frag:50-80` (vibrancy finalization)

**Concepts to Extract:**
- HSL color space conversion
- Saturation multiplier calculation
- Vibrancy strength parameter

**Timeline:** Week 7 (vibrancy phase)

**Reusability:** ~70% (shaders need GLSL ES 2.0 compatibility check)

## Code Attribution

### Copyright Headers

All extracted files will include attribution:

```c
/*
 * Portions derived from SceneFX (MIT License)
 * Copyright (c) 2023 Erik Reider, Scott Moreau
 * https://github.com/wlrfx/scenefx
 *
 * Portions derived from Wayfire (MIT License)
 * Copyright (c) 2018-2023 Wayfire contributors
 * https://github.com/WayfireWM/wayfire
 *
 * Portions derived from Hyprland (BSD-3-Clause License)
 * Copyright (c) 2022-2023 Vaxry
 * https://github.com/hyprwm/Hyprland
 *
 * Modifications for wlblur daemon:
 * Copyright (c) 2025 wlblur contributors
 *
 * SPDX-License-Identifier: MIT
 */
```

### License Files

Include all upstream licenses in repo:
- `LICENSE.SceneFX` (MIT)
- `LICENSE.Wayfire` (MIT)
- `LICENSE.Hyprland` (BSD-3-Clause)

### Documentation

`docs/code-origins.md` will document:
- Which files came from where
- What adaptations were made
- Why we chose that source
- Links to upstream commits

## Validation Strategy

### Correctness Validation

**Visual Comparison:**
- Render same scene in Hyprland, Wayfire, SceneFX, and wlblur daemon
- Compare screenshots pixel-by-pixel
- Acceptable difference: <1% RMSE (lighting/driver variations)

**Performance Comparison:**
- Benchmark blur times for each algorithm
- Compare to in-compositor measurements
- Acceptable overhead: +0.2ms (IPC latency)

### License Compliance

**Check:**
- All copyright notices preserved
- License files included
- Attribution in documentation
- No GPL contamination (all MIT/BSD)

**Tools:**
- `reuse lint` (REUSE compliance)
- `licensee` (license detection)
- Manual review by maintainers

## Extraction Anti-Patterns to Avoid

**❌ Don't blindly copy-paste:**
- Understand code before extracting
- Adapt to daemon context
- Remove compositor-specific assumptions

**❌ Don't mix incompatible licenses:**
- GPL code cannot be mixed with MIT
- Check every file's license header

**❌ Don't remove attribution:**
- Keep original copyright notices
- Add, don't replace, copyright lines

**❌ Don't ignore upstream bugs:**
- If we find bugs in extracted code, report upstream
- Don't just fix locally and move on

## Upstream Relationship

### Bug Reports

If we find issues in extracted code:
1. Create minimal reproducer
2. Report to upstream (SceneFX/Wayfire/Hyprland)
3. Offer patch if we've fixed it
4. Credit upstream in our fix

### Feature Contributions

If we add features that could benefit upstream:
1. Implement in our daemon first (prove it works)
2. Offer to upstream as compositor-agnostic version
3. Help with integration if they want it

### Acknowledgment

Public acknowledgment of upstream:
- README credits section
- Release notes mention upstream sources
- Social media shoutouts when appropriate

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **Upstream code has bugs** | Medium | Medium | Extensive testing, report upstream |
| **License violation** | Low | Critical | Careful review, REUSE compliance |
| **Extraction breaks something** | Medium | Medium | Unit tests for each extracted component |
| **Code doesn't adapt to daemon** | Low | High | Investigation phase already validated patterns |
| **Upstream adds improvements we miss** | High | Low | Subscribe to release notifications |

## Success Criteria

**Extraction is successful if:**
1. ✅ Kawase blur produces visually identical results to Wayfire/SceneFX
2. ✅ DMA-BUF import/export works with zero-copy (validated via GPU memory profiling)
3. ✅ Damage tracking prevents artifacts (validated via visual inspection)
4. ✅ Performance within 20% of in-compositor (1.2ms → 1.4ms acceptable)
5. ✅ All licenses properly attributed (validated via REUSE lint)
6. ✅ Code compiles with zero warnings (-Wall -Wextra -Wpedantic)
7. ✅ Unit tests pass for all extracted components

## References

- Investigation docs:
  - [SceneFX Investigation Summary](../Technical-Investigation/SceneFX-Summary) - Complete SceneFX analysis
  - [Wayfire Algorithm Analysis](../Technical-Investigation/Wayfire-Algorithm-Analysis) - Wayfire algorithms
  - [Hyprland Blur Algorithm](../Technical-Investigation/Hyprland-Blur-Algorithm) - Hyprland blur + vibrancy
  - [Comparative Analysis](../Technical-Investigation/Comparative-Analysis) - Three-compositor comparison

- Upstream repositories:
  - [SceneFX](https://github.com/wlrfx/scenefx)
  - [Wayfire](https://github.com/WayfireWM/wayfire)
  - [Hyprland](https://github.com/hyprwm/Hyprland)

- License resources:
  - [REUSE specification](https://reuse.software/spec/)
  - [MIT License](https://opensource.org/licenses/MIT)
  - [BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)

- Related ADRs:
  - [ADR-003: Kawase algorithm choice](ADR-003-Kawase-Algorithm) (justifies Wayfire extraction)
  - [ADR-002: DMA-BUF choice](ADR-002-DMA-BUF) (justifies SceneFX extraction)

## Community Feedback

We invite feedback on this decision:

- **Upstream maintainers (SceneFX/Wayfire/Hyprland)**: Are you comfortable with this extraction approach?
- **License experts**: Have we properly addressed license compliance?
- **Implementers**: Is the extraction plan clear and reasonable?
- **Community**: Should we prioritize different code sources?

Please open issues at [project repository] or discuss in [community forum].
