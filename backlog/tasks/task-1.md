---
id: task-1
title: "Create Complete Repository Structure"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["infrastructure", "setup"]
milestone: "m-0"
dependencies: []
---

## Description

Create the complete directory structure for the wlblur project, including all subdirectories, skeleton files with copyright headers, and initial meson build configuration. This establishes the file organization for all subsequent development.

The structure includes:
- `libwlblur/` - Core blur library
- `wlblurd/` - IPC daemon
- `examples/` - Test programs
- `integrations/` - Compositor patches
- `tests/` - Unit tests
- `docs/` - Documentation (already exists, preserve)

## Acceptance Criteria

- [x] All directories created with proper structure
- [x] Skeleton `.c` and `.h` files have MIT license headers
- [x] `meson.build` files exist in each subdirectory
- [x] Root `meson.build` defines project metadata
- [x] `.gitignore` covers C/Meson build artifacts
- [x] Existing `/docs` directory preserved
- [x] All skeleton files parse (no syntax errors)
- [x] `meson setup build` runs without errors

## Implementation Plan

### Phase 1: Directory Creation
Create directory tree:
```
wlblur/
├── libwlblur/
│   ├── include/wlblur/
│   ├── src/
│   ├── shaders/
│   └── private/
├── wlblurd/
│   ├── src/
│   ├── include/
│   └── systemd/
├── examples/
├── integrations/scroll/
├── tests/
├── scripts/
└── assets/test-images/
```

### Phase 2: Skeleton Files
For each `.c` file, add header:
```c
/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2024 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * [filename] - [brief purpose]
 */
```

### Phase 3: Meson Configuration
- Root `meson.build`: project('wlblur', 'c', version: '0.1.0', license: 'MIT')
- Subdirectory build files with TODO comments
- `meson_options.txt` for build options

### Phase 4: Git Configuration
- `.gitignore` for: build/, *.o, *.so, *.a, compile_commands.json
- Preserve existing `.git` history

## Notes & Comments

**Reference Structure**: See complete structure in orchestration conversation.

**Existing Content**: Repository already has:
- `/docs/investigation/` - Research docs (preserve)
- `/docs/pre-investigation/` - Planning (preserve)
- `/docs/post-investigation/` - Conclusions (preserve)
- `LICENSE` - MIT license (preserve)

**Validation**: After creation, run:
```bash
meson setup build
meson compile -C build  # Should fail on missing implementations, but parse
```

**Deliverables**:
1. Commit with all structure
2. Tree output showing created files
3. Any deviations from plan documented
