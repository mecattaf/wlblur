---
id: m-3
title: "Configuration System Implementation"
---

## Description

Implement daemon-side configuration system with preset support, enabling minimal compositor integration while supporting future feature evolution (tint, vibrancy, materials).

**Rationale:** Configuration system must exist before compositor integration because the `ext_background_effect_v1` protocol doesn't include blur parameters. Compositors need to reference presets by name when forwarding blur requests to the daemon.

**Timeline:** Weeks 6-8
**Status:** Next
**Depends on:** m-2 (wlblurd IPC Daemon)

## Objectives

1. Enable compositor integration with minimal parameter knowledge (~220 lines maintained)
2. Support future feature additions without compositor changes
3. Provide multi-compositor consistency through shared configuration
4. Enable hot-reload for rapid experimentation
5. Establish foundation for material system (m-8)

## Deliverables

### Core Implementation

- [ ] **TOML config file parsing** (`wlblurd/src/config.c`, ~300 lines)
  - Use tomlc99 library for parsing
  - Parse `~/.config/wlblur/config.toml`
  - Support XDG_CONFIG_HOME override
  - Graceful handling of missing config (use defaults)

- [ ] **Preset definition and resolution** (`wlblurd/src/presets.c`, ~200 lines)
  - Standard presets: "window", "panel", "hud", "tooltip"
  - User-defined custom presets
  - Preset lookup by name (hash table for O(1) access)
  - Fallback hierarchy: named preset → direct params → defaults

- [ ] **Configuration structures** (`wlblurd/include/config.h`, ~150 lines)
  - `struct daemon_config` - Global daemon settings
  - `struct preset` - Named parameter presets
  - Validation functions for all parameters

- [ ] **Hot reload via SIGUSR1** (`wlblurd/src/reload.c`, ~100 lines)
  - Signal handler for SIGUSR1
  - Safe config reload (validate before applying)
  - Atomic config swap (don't break running daemon)
  - Logging for reload success/failure

### IPC Protocol Extension

- [ ] **Extended request structure** (`wlblurd/include/protocol.h`, ~50 lines)
  - Add `char preset_name[32]` field to `wlblur_request`
  - Add `bool use_preset` flag
  - Maintain backward compatibility

- [ ] **Preset resolution in IPC handler** (`wlblurd/src/ipc_protocol.c`, ~100 lines)
  - Resolve preset name to parameters
  - Handle missing presets (log warning, use default)
  - Support direct parameter override

### Algorithm Selection

- [ ] **Algorithm enum** (`libwlblur/include/wlblur/blur_params.h`)
  - Add `enum wlblur_algorithm { WLBLUR_ALGO_KAWASE, ... }`
  - Add `algorithm` field to `struct wlblur_blur_params`
  - Document "kawase" as only supported value in m-3

- [ ] **Algorithm validation** (`wlblurd/src/config.c`)
  - Validate `algorithm == "kawase"` in m-3
  - Reject other values with clear error message
  - Prepare for m-9 extension (gaussian, box, bokeh)

### Default Configuration

- [ ] **Hardcoded default presets** (`wlblurd/src/presets.c`)
  - Daemon works without config file
  - Sensible defaults for window/panel/hud/tooltip
  - Document default values

- [ ] **Example configuration file** (`docs/examples/wlblur-config.toml`)
  - Complete example with all presets
  - Commented sections explaining each parameter
  - Algorithm selection examples

### Documentation

- [ ] **Configuration architecture** (`docs/architecture/04-configuration-system.md`)
  - System overview and rationale
  - Preset resolution algorithm
  - Hot reload mechanism
  - IPC protocol extension details

- [ ] **User configuration guide** (`docs/configuration-guide.md`)
  - Config file location and format
  - Creating custom presets
  - Algorithm selection guide
  - Hot reload instructions
  - Troubleshooting common errors

- [ ] **ADR 006** (`docs/decisions/006-daemon-configuration-with-presets.md`)
  - Architecture decision rationale
  - Alternatives considered
  - ext_background_effect_v1 protocol constraint
  - Future-proofing for tint/vibrancy/materials

### Testing

- [ ] **Config parsing tests**
  - Valid TOML parsing
  - Invalid TOML handling
  - Missing file handling
  - Malformed presets

- [ ] **Preset resolution tests**
  - Named preset lookup
  - Fallback to defaults
  - Direct parameter override
  - Missing preset handling

- [ ] **Hot reload tests**
  - Successful reload
  - Failed reload (keep old config)
  - Invalid config rejection
  - Signal handling

- [ ] **Integration tests**
  - End-to-end: config file → preset resolution → blur render
  - Multiple presets in single session
  - Config reload during active blurs

## Success Criteria

- [ ] Daemon starts successfully with and without config file
- [ ] Standard presets (window, panel, hud, tooltip) resolve correctly
- [ ] Custom user presets work
- [ ] Hot reload (SIGUSR1) applies new config without daemon restart
- [ ] Invalid configs are rejected gracefully (don't crash daemon)
- [ ] Config parsing adds <5ms to daemon startup time
- [ ] Preset lookup adds <0.001ms to IPC request handling
- [ ] All tests pass
- [ ] Documentation is complete and clear

## Implementation Notes

### File Structure

```
wlblurd/
├── src/
│   ├── config.c          (+300 lines)
│   ├── presets.c         (+200 lines)
│   ├── reload.c          (+100 lines)
│   ├── main.c            (+50 lines, signal setup)
│   └── ipc_protocol.c    (+100 lines, preset resolution)
├── include/
│   ├── config.h          (+150 lines)
│   └── protocol.h        (+20 lines, extended request)
└── tests/
    ├── test_config.c     (NEW)
    └── test_presets.c    (NEW)

libwlblur/
└── include/wlblur/
    └── blur_params.h     (+30 lines, algorithm enum)

docs/
├── architecture/
│   └── 04-configuration-system.md  (NEW)
├── decisions/
│   └── 006-daemon-configuration-with-presets.md  (NEW)
├── examples/
│   └── wlblur-config.toml  (NEW)
└── configuration-guide.md  (NEW)
```

**Total new code:** ~900 lines
**Total new documentation:** ~8,000 words

### Dependencies

**Add to meson.build:**
```meson
tomlc99_dep = dependency('tomlc99', required: true)

executable('wlblurd',
  sources: [...],
  dependencies: [existing_deps..., tomlc99_dep],
)
```

**tomlc99 installation:**
- Arch: `pacman -S tomlc99`
- Debian/Ubuntu: `apt install libtoml-dev`
- Fedora: `dnf install tomlc99-devel`

### Config File Location Priority

1. `$XDG_CONFIG_HOME/wlblur/config.toml`
2. `~/.config/wlblur/config.toml`
3. `/etc/wlblur/config.toml` (system-wide)
4. Hardcoded defaults (no file)

### Standard Preset Definitions

```c
// wlblurd/src/presets.c
static const struct preset builtin_presets[] = {
    {
        .name = "window",
        .params = {
            .algorithm = WLBLUR_ALGO_KAWASE,
            .num_passes = 3,
            .radius = 8.0,
            .saturation = 1.15,
            // ... other defaults
        }
    },
    {
        .name = "panel",
        .params = {
            .algorithm = WLBLUR_ALGO_KAWASE,
            .num_passes = 2,
            .radius = 4.0,
            .brightness = 1.05,
            .saturation = 1.1,
        }
    },
    {
        .name = "hud",
        .params = {
            .algorithm = WLBLUR_ALGO_KAWASE,
            .num_passes = 4,
            .radius = 12.0,
            .saturation = 1.2,
            .vibrancy = 0.2,
        }
    },
    {
        .name = "tooltip",
        .params = {
            .algorithm = WLBLUR_ALGO_KAWASE,
            .num_passes = 1,
            .radius = 2.0,
            .saturation = 1.0,
        }
    },
};
```

## Related Milestones

**Depends on:**
- m-2: wlblurd IPC Daemon (provides IPC infrastructure to extend)

**Enables:**
- m-4: ScrollWM Integration (requires preset system)
- m-6: Tint Support (config adds tint_color field, no compositor changes)
- m-7: Vibrancy Support (config adds vibrancy_strength field, no compositor changes)
- m-8: Material System (presets map to material definitions)
- m-9: Additional Algorithms (algorithm field ready for gaussian/box/bokeh)

## References

- **ADR-006:** Daemon Configuration with Presets (architecture decision)
- **ext_background_effect_v1:** Protocol spec (explains parameter omission)
- **tomlc99:** https://github.com/cktan/tomlc99 (parsing library)
- **kanshi:** https://sr.ht/~emersion/kanshi/ (similar config pattern)
- **mako:** https://github.com/emersion/mako (similar hot reload pattern)
