id: task-1
title: "Create Complete Repository Structure with Build System"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["infrastructure", "setup", "meson"]
milestone: "m-0"
dependencies: []
---

## Description

Create the complete directory structure for wlblur with all subdirectories, skeleton C files with proper headers, and a working meson build system. This establishes the foundation for all subsequent development.

**Critical**: This task must complete before any code implementation tasks can begin.

## Acceptance Criteria

- [x] All directories created per target-repomap.md structure
- [x] All skeleton .c and .h files have MIT license headers
- [x] All meson.build files created in subdirectories
- [x] Root meson.build defines project metadata correctly
- [x] .gitignore covers build artifacts (build/, *.o, *.so, etc.)
- [x] Existing /docs directory preserved untouched
- [x] `meson setup build` runs without errors
- [x] `meson compile -C build` fails gracefully (expected - no implementations yet)
- [x] All paths use forward slashes, no spaces in filenames

## Implementation Plan

### Phase 1: Create Directory Tree

Create all directories from target-repomap.md:

```bash
mkdir -p libwlblur/{include/wlblur,src,shaders,private}
mkdir -p wlblurd/{src,include,systemd}
mkdir -p examples
mkdir -p integrations/{scroll,niri}
mkdir -p tests
mkdir -p docs/{architecture,decisions,api,guides,consolidation}
mkdir -p scripts
mkdir -p assets/test-images
mkdir -p .backlog/{tasks,milestones}
```

### Phase 2: Create Skeleton Files with Headers

For EVERY .c and .h file in target-repomap.md, create with this header:

```c
/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * [filename] - [brief description based on filename]
 */

#include "[corresponding header]"  // For .c files

// TODO: Implementation
```

**Files to create** (refer to target-repomap.md for complete list):

**libwlblur/include/wlblur/**:
- wlblur.h - "Public API for blur operations"
- blur_params.h - "Blur parameter structures and presets"
- blur_context.h - "Context management for blur operations"
- dmabuf.h - "DMA-BUF import/export helpers"

**libwlblur/src/**:
- blur_kawase.c - "Dual Kawase blur algorithm implementation"
- blur_context.c - "Context lifecycle management"
- egl_helpers.c - "EGL utility functions"
- dmabuf.c - "DMA-BUF import/export implementation"
- shaders.c - "Shader compilation and management"
- framebuffer.c - "Framebuffer object pooling"
- utils.c - "Logging and utilities"

**libwlblur/private/**:
- internal.h - "Private API and data structures"

**wlblurd/src/**:
- main.c - "Daemon entry point and event loop"
- ipc.c - "IPC protocol handler"
- ipc_protocol.c - "Protocol message serialization"
- client.c - "Per-client state management"
- blur_node.c - "Blur node registry"
- buffer_registry.c - "DMA-BUF buffer tracking"
- config.c - "Daemon configuration"

**wlblurd/include/**:
- protocol.h - "IPC protocol definitions"

**examples/**:
- blur-png.c - "Test libwlblur with PNG files"
- ipc-client-example.c - "Reference IPC client implementation"
- protocol-demo.c - "IPC protocol demonstration"

**tests/**:
- test_kawase.c - "Kawase algorithm unit tests"
- test_dmabuf.c - "DMA-BUF import/export tests"
- test_ipc.c - "IPC protocol tests"
- test_params.c - "Parameter validation tests"

### Phase 3: Meson Build System

**Root meson.build:**
```meson
project('wlblur', 'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=c99',
    'warning_level=2',
    'werror=false',
  ],
)

# Project information
project_description = 'Compositor-agnostic blur daemon for Wayland'
project_url = 'https://github.com/mecattaf/wlblur'

# Dependencies (check availability only, don't fail)
egl_dep = dependency('egl', required: false)
glesv2_dep = dependency('glesv2', required: false)
libdrm_dep = dependency('libdrm', required: false)

if egl_dep.found() and glesv2_dep.found() and libdrm_dep.found()
  message('All dependencies found - build will succeed')
else
  message('Some dependencies missing - build will fail (expected for skeleton)')
endif

# Subdirectories
subdir('libwlblur')
subdir('wlblurd')
subdir('examples')
subdir('tests')

# Summary
summary({
  'prefix': get_option('prefix'),
  'libdir': get_option('libdir'),
  'bindir': get_option('bindir'),
}, section: 'Directories')

summary({
  'EGL': egl_dep.found(),
  'GLESv2': glesv2_dep.found(),
  'libdrm': libdrm_dep.found(),
}, section: 'Dependencies')
```

**libwlblur/meson.build:**
```meson
libwlblur_sources = files(
  'src/blur_kawase.c',
  'src/blur_context.c',
  'src/egl_helpers.c',
  'src/dmabuf.c',
  'src/shaders.c',
  'src/framebuffer.c',
  'src/utils.c',
)

libwlblur_deps = [
  dependency('egl'),
  dependency('glesv2'),
  dependency('libdrm'),
]

libwlblur_includes = include_directories('include')

libwlblur = library(
  'wlblur',
  libwlblur_sources,
  dependencies: libwlblur_deps,
  include_directories: libwlblur_includes,
  version: meson.project_version(),
  install: true,
)

# Install headers
install_headers(
  'include/wlblur/wlblur.h',
  'include/wlblur/blur_context.h',
  'include/wlblur/blur_params.h',
  'include/wlblur/dmabuf.h',
  subdir: 'wlblur',
)

# Declare dependency for internal use
libwlblur_dep = declare_dependency(
  link_with: libwlblur,
  include_directories: libwlblur_includes,
)
```

**wlblurd/meson.build:**
```meson
wlblurd_sources = files(
  'src/main.c',
  'src/ipc.c',
  'src/ipc_protocol.c',
  'src/client.c',
  'src/blur_node.c',
  'src/buffer_registry.c',
  'src/config.c',
)

wlblurd_deps = [
  libwlblur_dep,
]

wlblurd = executable(
  'wlblurd',
  wlblurd_sources,
  dependencies: wlblurd_deps,
  install: true,
)
```

**examples/meson.build:**
```meson
# Examples are optional
if get_option('examples').enabled()
  blur_png = executable('blur-png',
    'blur-png.c',
    dependencies: [libwlblur_dep],
  )
  
  ipc_example = executable('ipc-client-example',
    'ipc-client-example.c',
  )
endif
```

**tests/meson.build:**
```meson
# Tests
if get_option('tests').enabled()
  test_kawase = executable('test_kawase',
    'test_kawase.c',
    dependencies: [libwlblur_dep],
  )
  test('kawase algorithm', test_kawase)
  
  test_dmabuf = executable('test_dmabuf',
    'test_dmabuf.c',
    dependencies: [libwlblur_dep],
  )
  test('dmabuf operations', test_dmabuf)
endif
```

**meson_options.txt:**
```meson
option('examples', type: 'feature', value: 'auto',
  description: 'Build example programs')

option('tests', type: 'feature', value: 'auto',
  description: 'Build test suite')

option('blur-algorithms', type: 'array',
  choices: ['kawase', 'gaussian', 'box', 'bokeh'],
  value: ['kawase'],
  description: 'Blur algorithms to include')
```

### Phase 4: Git Configuration

**.gitignore:**
```
# Build artifacts
build/
builddir/
*.o
*.so
*.so.*
*.a
*.dylib
*.dll
*.exe

# Meson
meson-logs/
meson-private/
compile_commands.json

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Testing
*.log
core
vgcore.*

# Don't ignore docs
!docs/
```

### Phase 5: Integration Files

**integrations/scroll/README.md:**
```markdown
# ScrollWM Integration

Integration for wlblur daemon with ScrollWM compositor.

## Status
Not yet implemented. This directory will contain:
- blur_integration.c (~220 lines)
- blur_integration.h
- scroll.patch (git patch for ScrollWM)
- Integration documentation

## Prerequisites
- ScrollWM installed
- wlblurd running
- libwlblur.so available

See parent README for details.
```

**integrations/niri/README.md:**
```markdown
# niri Integration

Integration for wlblur daemon with niri compositor (Rust/Smithay).

## Status
Planned for Phase 4. This directory will contain:
- Rust IPC client module
- niri integration guide
- Example configuration

See parent README for details.
```

### Phase 6: Shader Directory Setup

**libwlblur/shaders/README.md:**
```markdown
# wlblur Shaders

GLSL shaders for blur algorithms. These will be extracted from:
- SceneFX (Dual Kawase base implementation)
- Hyprland (vibrancy enhancements)
- Wayfire (additional algorithms)

## Expected Files
- kawase_downsample.frag.glsl - Downsample pass (5-tap)
- kawase_upsample.frag.glsl - Upsample pass (8-tap)
- blur_prepare.frag.glsl - Pre-processing
- blur_finish.frag.glsl - Post-processing effects
- vibrancy.frag.glsl - HSL color boost
- common.glsl - Shared functions

## Status
Awaiting extraction in task-2.
```

### Phase 7: Systemd Service Template

**wlblurd/systemd/wlblur.service:**
```ini
[Unit]
Description=wlblur - Wayland blur daemon
Documentation=https://github.com/mecattaf/wlblur
PartOf=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/bin/wlblurd
Restart=on-failure
RestartSec=1

# Security hardening
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
NoNewPrivileges=yes

[Install]
WantedBy=graphical-session.target
```

## Validation Commands

After completing all phases, validate:

```bash
# Check directory structure
tree -L 3 wlblur/

# Validate meson setup
cd wlblur
meson setup build
# Should output: "Project configured" (even if dependencies missing)

# Check for syntax errors (will fail on missing implementations - expected)
meson compile -C build 2>&1 | grep -i "error" | wc -l
# Should show errors from empty implementations, not syntax errors

# Verify all skeleton files have headers
grep -r "SPDX-License-Identifier: MIT" libwlblur/ wlblurd/ | wc -l
# Should match number of .c and .h files created
```

## Notes & Comments

**Skeleton Files**: All .c files contain only headers and TODO comments. No implementations. This allows:
- Meson build system to be validated
- Subsequent tasks to fill in implementations
- Clear separation between structure and logic

**Meson Error Expected**: `meson compile -C build` WILL fail because implementations are empty. This is correct. We're only validating structure.

**Preserving Docs**: The existing docs/ directory MUST be preserved. Do not delete or modify:
- docs/investigation/
- docs/pre-investigation/  
- docs/post-investigation/

**Dependencies**: Meson checks for egl, glesv2, libdrm but doesn't fail if missing. This allows validation on systems without GPU libraries installed.

## Deliverables

After task completion, commit with message:
```
feat: initial repository structure and build system

- Create complete directory structure per target-repomap.md
- Add skeleton .c and .h files with MIT license headers
- Implement meson build system with all subdirectories
- Add .gitignore for build artifacts
- Create integration directory placeholders
- Add systemd service template

Meson validates but compilation fails (expected - no implementations).
Ready for shader extraction (task-2) and parameter schema (task-3).
```

Provide:
1. Tree output of created structure
2. Output of `meson setup build`
3. List of all created files with line counts
4. Any deviations from target-repomap.md (with justification)
