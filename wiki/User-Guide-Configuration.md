# Configuration Guide

> **Note**: This page will be populated with content from [docs/configuration-guide.md](https://github.com/mecattaf/wlblur/blob/main/docs/configuration-guide.md).

For now, see the complete configuration guide at:
- [docs/configuration-guide.md](https://github.com/mecattaf/wlblur/blob/main/docs/configuration-guide.md)
- [docs/examples/wlblur-config.toml](https://github.com/mecattaf/wlblur/blob/main/docs/examples/wlblur-config.toml)
- [docs/architecture/04-configuration-system.md](https://github.com/mecattaf/wlblur/blob/main/docs/architecture/04-configuration-system.md)

## Quick Reference

### Config File Location

`~/.config/wlblur/config.toml`

### Basic Example

```toml
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
```

### Hot Reload

```bash
killall -USR1 wlblurd
```

---

**Full Documentation**: See [docs/configuration-guide.md](https://github.com/mecattaf/wlblur/blob/main/docs/configuration-guide.md)
