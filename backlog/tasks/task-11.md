---
id: task-11
title: "Implement TOML Configuration Parsing"
status: ✅ Complete
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-11-13
completed_date: 2025-11-13
labels: ["daemon", "config", "toml"]
milestone: "m-3"
dependencies: ["task-9"]
---

## Description

Implement TOML configuration file parsing for wlblurd using tomlc99 library. Enable daemon to read configuration from `~/.config/wlblur/config.toml` with support for daemon settings, default blur parameters, and named presets.

**Rationale**: Configuration system is required before compositor integration (m-4) because the `ext_background_effect_v1` protocol doesn't include blur parameters. See ADR-006 for complete architectural decision.

## Acceptance Criteria

- [x] TOML parsing using tomlc99 library
- [x] Config file location: `~/.config/wlblur/config.toml`
- [x] Support for `XDG_CONFIG_HOME` environment variable
- [x] Fallback to hardcoded defaults if config missing
- [x] Parse `[daemon]` section: socket_path, log_level, max_nodes_per_client
- [x] Parse `[defaults]` section: algorithm, radius, passes, post-processing params
- [x] Parse `[presets.*]` sections: Named presets with full parameter sets
- [x] Comprehensive parameter validation (ranges, algorithm support)
- [x] Command-line `--config` flag for custom config path
- [x] Graceful error handling (parse errors, invalid values)
- [x] Config parsing adds <5ms to daemon startup

## Implementation Details

**Files Created**:
- `wlblurd/src/config.c` (~405 lines)
- `wlblurd/include/config.h` (~211 lines, partial)

**Key Functions**:
```c
// Configuration lifecycle
struct daemon_config* config_load(const char *path);
void config_free(struct daemon_config *config);
bool config_validate(const struct daemon_config *config);

// Config file location priority:
// 1. Command-line --config flag
// 2. $XDG_CONFIG_HOME/wlblur/config.toml
// 3. ~/.config/wlblur/config.toml
// 4. /etc/wlblur/config.toml (system-wide)
// 5. Hardcoded defaults (no file)
```

**Parameter Validation**:
- Algorithm: Only "kawase" supported in m-3 (prepared for gaussian/box/bokeh in m-9)
- num_passes: 1-8
- radius: 1.0-20.0
- brightness: 0.0-2.0
- contrast: 0.0-2.0
- saturation: 0.0-2.0
- noise: 0.0-1.0
- vibrancy: 0.0-2.0

**Error Handling**:
- Missing config file: Log info, use hardcoded defaults
- TOML parse error: Log detailed error with line number, exit gracefully
- Invalid value: Log validation error, reject preset/config
- Out of range: Log range error, reject preset/config

**Build System Changes**:
- Added tomlc99 dependency to `wlblurd/meson.build`

## Testing Performed

**Manual Tests** (all passed ✅):
- [x] Daemon starts with no config file
- [x] Daemon loads config from default location
- [x] Daemon accepts --config flag
- [x] Invalid TOML syntax rejected
- [x] Out-of-range parameters rejected
- [x] Missing sections use defaults
- [x] Algorithm validation enforces KAWASE-only

**Performance**:
- Config parsing: ~2-3ms (within <5ms budget)

## Completion Notes

Implementation complete and tested. Handles all edge cases gracefully:
- Works out-of-box without config file (uses hardcoded defaults)
- Comprehensive validation prevents invalid configurations
- Clear error messages for debugging
- Efficient parsing (<5ms overhead)

**Integration**: Config system integrated into daemon main.c startup sequence.

**Documentation**: See `docs/architecture/04-configuration-system.md` for complete technical details.

## Related Tasks

- **Depends on**: task-9 (Blur Node Registry - daemon infrastructure)
- **Enables**: task-12 (Preset Management), task-13 (Hot Reload), task-14 (IPC Extension)
- **Milestone**: m-3 (Configuration System Implementation)

## References

- ADR-006: Daemon Configuration with Presets
- docs/architecture/04-configuration-system.md
- docs/configuration-guide.md
- docs/examples/wlblur-config.toml
