# ADR-006: Daemon Configuration with Presets

> **Note**: This page contains content from [docs/decisions/006-daemon-configuration-with-presets.md](../../docs/decisions/006-daemon-configuration-with-presets.md).

For the complete ADR, see: [docs/decisions/006-daemon-configuration-with-presets.md](../../docs/decisions/006-daemon-configuration-with-presets.md)

## Summary

**Decision**: Implement daemon-side configuration with named presets instead of compositor-side configuration.

**Rationale**: The `ext_background_effect_v1` Wayland protocol doesn't include blur parameters. Compositors must decide parameters when forwarding blur requests. Daemon-side config solves this while maintaining minimal compositor integration (~220 lines).

## Benefits

1. **Minimal Compositor Integration**: Compositors just send preset names
2. **Multi-Compositor Consistency**: Single config works everywhere
3. **Future-Proof**: New features (tint, materials) work without compositor changes
4. **Hot Reload**: Edit config, send SIGUSR1, changes apply instantly

## Implementation

- Configuration file: `~/.config/wlblur/config.toml`
- Preset system: window, panel, hud, tooltip
- Hot reload via SIGUSR1
- Fallback hierarchy: preset → override → defaults → hardcoded

---

**Full ADR**: [docs/decisions/006-daemon-configuration-with-presets.md](../../docs/decisions/006-daemon-configuration-with-presets.md)
