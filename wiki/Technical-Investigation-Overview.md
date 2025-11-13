# Technical Investigation

This section documents our comprehensive investigation of existing blur implementations in Wayland compositors.

## Purpose

Before designing wlblur, we thoroughly analyzed three major blur implementations:
- **Hyprland** - High-performance in-compositor blur
- **SceneFX** - wlroots scene graph extension
- **Wayfire** - Plugin-based blur system

This investigation informed our architectural decisions and validated the external daemon approach.

## Investigation Reports

### Comparative Analysis
- [Comprehensive Synthesis](Comparative-Analysis) ⭐ - Complete comparison of all three
- [Parameter Comparison](Parameter-Comparison) - Parameter mapping across compositors
- [Shader Extraction Report](Shader-Extraction-Report) - How we reused shaders

### Hyprland Investigation
- [Overview](Hyprland-Overview) - Introduction to Hyprland's approach
- [Architecture](Hyprland-Architecture) - Implementation structure
- [Blur Algorithm](Hyprland-Blur-Algorithm) - Dual Kawase details
- [Daemon Translation](Hyprland-Daemon-Translation) - How to externalize
- [Damage Tracking](Hyprland-Damage-Tracking) - Optimization techniques
- [Performance](Hyprland-Performance) - Benchmarks and analysis

### SceneFX Investigation
- [API Compatibility](SceneFX-API-Compatibility) - wlroots integration
- [Architecture](SceneFX-Architecture) - Scene graph replacement
- [Blur Implementation](SceneFX-Blur-Implementation) - How blur works
- [Daemon Translation](SceneFX-Daemon-Translation) - Externalization strategy
- [Damage Tracking](SceneFX-Damage-Tracking) - Scene graph damage
- [Summary](SceneFX-Summary) ⭐ - Key findings

### Wayfire Investigation
- [Overview](Wayfire-Overview) - Plugin architecture intro
- [Architecture](Wayfire-Architecture) - Plugin system details
- [Algorithm Analysis](Wayfire-Algorithm-Analysis) - Multiple blur algorithms
- [Damage Tracking](Wayfire-Damage-Tracking) - Plugin damage handling
- [IPC Feasibility](Wayfire-IPC-Feasibility) - External daemon possibility

## Key Findings

**What we learned:**
1. All three use OpenGL ES for blur rendering
2. Dual Kawase is most efficient algorithm
3. Damage tracking is critical for performance
4. Plugin/external architecture is feasible
5. DMA-BUF enables zero-copy texture sharing

**How this informed wlblur:**
- Chose Dual Kawase algorithm ([ADR-003](Architecture-Decisions-ADR-003-Kawase-Algorithm))
- Designed external daemon ([ADR-001](Architecture-Decisions-ADR-001-External-Daemon))
- Used DMA-BUF for performance ([ADR-002](Architecture-Decisions-ADR-002-DMA-BUF))
- Extracted shaders from SceneFX ([ADR-005](Architecture-Decisions-ADR-005-SceneFX-Extraction))

## Investigation Timeline

The investigation was conducted in three phases:

1. **Phase 1** (Week 1-2): Hyprland investigation
   - Analyzed custom renderer architecture
   - Documented blur algorithm and optimizations
   - Identified performance characteristics

2. **Phase 2** (Week 3-4): SceneFX investigation
   - Studied wlroots scene graph integration
   - Analyzed DMA-BUF patterns
   - Documented damage tracking approach

3. **Phase 3** (Week 5-6): Wayfire investigation
   - Examined plugin architecture
   - Compared multiple blur algorithms
   - Validated IPC daemon feasibility

## Methodology

For each compositor, we:
1. Read the source code thoroughly
2. Built and tested locally
3. Profiled performance
4. Documented architecture patterns
5. Identified extractable components
6. Evaluated daemon translation feasibility

## See Also

- [ADR Overview](Architecture-Decisions-Overview) - How investigations influenced decisions
- [SceneFX Extraction](Architecture-Decisions-ADR-005-SceneFX-Extraction) - Code reuse strategy
- [Background Context](Background-Blur-in-Compositors) - Pre-investigation research
