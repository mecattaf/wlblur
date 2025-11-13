# Blur Presets

## What Are Presets?

Presets are named collections of blur parameters. Instead of specifying radius, passes, saturation, etc. every time, you define presets once and reference them by name.

## Standard Presets

wlblur includes 4 standard presets:

### `window` - Regular Applications
```toml
[presets.window]
radius = 8.0
passes = 3
saturation = 1.15
```

For: Firefox, terminals, text editors

### `panel` - Desktop Panels
```toml
[presets.panel]
radius = 4.0
passes = 2
brightness = 1.05
```

For: Waybar, Quickshell panels, status bars

### `hud` - Overlays & Popups
```toml
[presets.hud]
radius = 12.0
passes = 4
vibrancy = 0.2
saturation = 1.2
```

For: Rofi, launchers, overlay menus

### `tooltip` - Small Popups
```toml
[presets.tooltip]
radius = 2.0
passes = 1
```

For: Tooltips, small notifications

## Creating Custom Presets

```toml
[presets.my_terminal]
radius = 3.0
passes = 2
saturation = 1.0
noise = 0.01
```

Then in compositor config:
```toml
[blur.rules]
foot = "my_terminal"
kitty = "my_terminal"
```

## Preset Hierarchy

When compositor requests blur:
1. Check if preset name provided → use preset
2. Else check if direct parameters provided → use them
3. Else use `[defaults]` from config
4. Else use hardcoded defaults

## Examples

### Performance Preset
```toml
[presets.fast]
radius = 3.0
passes = 2
saturation = 1.0
```

### Quality Preset
```toml
[presets.quality]
radius = 10.0
passes = 4
saturation = 1.2
vibrancy = 0.1
```

### Artistic Preset
```toml
[presets.artistic]
radius = 15.0
passes = 5
saturation = 1.3
vibrancy = 0.3
noise = 0.03
```

## See Also

- [Configuration Guide](Configuration) - Full parameter reference
- [Example Config](Example-Config) - Complete annotated example
- [Hot Reload](Hot-Reload) - Apply preset changes live
