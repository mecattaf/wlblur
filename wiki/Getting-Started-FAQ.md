# Frequently Asked Questions

## General

### What is wlblur?

wlblur is an external blur daemon for Wayland compositors that provides Apple-quality blur effects through minimal integration.

See: [What is wlblur?](What-is-wlblur)

### Why not just use Hyprland?

Hyprland's blur is excellent! But:
- wlblur works with compositors that don't have built-in blur
- Enables consistent blur across different compositors
- Reduces maintenance burden for compositor developers

See: [ADR-001: External Daemon](Architecture-Decisions-ADR-001-External-Daemon)

### Which compositors are supported?

**Currently:** None (in integration phase)
**Next:** ScrollWM (milestone m-4)
**Planned:** niri, Sway, River, and others

See: [Roadmap](Roadmap-Project-Roadmap)

### How does performance compare to Hyprland?

Target: <1.5ms @ 1920×1080 (3 passes)
- Hyprland: ~0.8-1.5ms
- wlblur: ~1.4ms (1.2ms blur + 0.2ms IPC)

See: [ADR-002: DMA-BUF](Architecture-Decisions-ADR-002-DMA-BUF) for benchmarks

## Configuration

### Where is the config file?

Primary: `~/.config/wlblur/config.toml`
Custom: Use `wlblurd --config /path/to/config.toml`

See: [Configuration Guide](User-Guide-Configuration)

### Can I use different blur settings per application?

Yes! Use presets:

```toml
[presets.terminal]
radius = 3.0
passes = 2

[presets.browser]
radius = 8.0
passes = 3
```

See: [Presets Guide](User-Guide-Presets)

### How do I reload config without restarting?

```bash
killall -USR1 wlblurd
```

See: [Hot Reload Guide](User-Guide-Hot-Reload)

## Development

### How can I contribute?

See: [Contributing Guide](Development-Contributing)

### How do I integrate with my compositor?

See: [Integration Guide](For-Compositor-Developers-Integration-Overview)

### Why external daemon instead of library?

Crash isolation, independent updates, minimal compositor code.

See: [ADR-001](Architecture-Decisions-ADR-001-External-Daemon) and [Why IPC Is Better](Architecture-Decisions-Why-IPC-Is-Better)

## Troubleshooting

### Daemon won't start

Check logs: `wlblurd --log-level debug`

See: [Troubleshooting Guide](User-Guide-Troubleshooting)

### Config errors on startup

Verify config syntax:
```bash
tomlc99-verify ~/.config/wlblur/config.toml
```

### Performance is slow

Check GPU usage, try fewer passes or smaller radius.

See: [Performance Considerations](For-Compositor-Developers-Performance-Considerations)

## See Also

- [Home](Home) - Wiki home
- [Current Status](Roadmap-Current-Status) - Project status
- [Getting Started](What-is-wlblur) - Introduction
