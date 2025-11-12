Target repo organization:
```
wlblur/                               
├── LICENSE                           ✅ MIT
├── README.md                         🔄 To write
├── ROADMAP.md                        🔄 To write
├── meson.build                       🔄 Root build
├── meson_options.txt                 🔄 Build options
│
├── libwlblur/                        ← Core blur library
│   ├── meson.build
│   ├── include/
│   │   ├── wlblur/
│   │   │   ├── wlblur.h             ← Public API
│   │   │   ├── blur_params.h        ← Parameter structs
│   │   │   ├── blur_context.h       ← Context management
│   │   │   └── dmabuf.h             ← DMA-BUF helpers
│   │   └── wlblur_version.h         ← Version macros
│   │
│   ├── src/
│   │   ├── blur_kawase.c            ← Multi-pass algorithm
│   │   ├── blur_context.c           ← EGL context setup
│   │   ├── egl_helpers.c            ← EGL utilities
│   │   ├── dmabuf.c                 ← Import/export
│   │   ├── shaders.c                ← Shader compilation
│   │   ├── framebuffer.c            ← FBO management
│   │   └── utils.c                  ← Logging, etc.
│   │
│   ├── shaders/
│   │   ├── kawase_downsample.frag.glsl
│   │   ├── kawase_upsample.frag.glsl
│   │   ├── blur_prepare.frag.glsl
│   │   ├── blur_finish.frag.glsl
│   │   └── common.glsl              ← Shared functions
│   │
│   └── private/
│       └── internal.h               ← Private headers
│
├── wlblurd/                          ← Daemon
│   ├── meson.build
│   ├── src/
│   │   ├── main.c                   ← Entry point, socket server
│   │   ├── ipc.c                    ← Protocol handling
│   │   ├── ipc_protocol.c           ← Serialization
│   │   ├── client.c                 ← Per-client state
│   │   ├── blur_node.c              ← Virtual scene graph
│   │   ├── buffer_registry.c        ← Track DMA-BUFs
│   │   └── config.c                 ← Daemon config
│   │
│   ├── include/
│   │   └── protocol.h               ← IPC message definitions
│   │
│   └── systemd/
│       └── wlblur.service           ← systemd unit
│
├── examples/
│   ├── blur-png.c                   ← Test libwlblur (PNG → PNG)
│   ├── ipc-client-example.c         ← Reference IPC client
│   ├── protocol-demo.c              ← Show IPC messages
│   └── meson.build
│
├── integrations/                     ← Compositor patches
│   ├── scroll/
│   │   ├── README.md                ← Integration guide
│   │   ├── blur_integration.c       ← ~220 lines
│   │   ├── blur_integration.h
│   │   ├── scroll.patch             ← Git patch
│   │   └── meson.build
│   │
│   └── niri/                        ← Future Phase 4
│       └── README.md
│
├── tests/
│   ├── test_kawase.c                ← Algorithm tests
│   ├── test_dmabuf.c                ← DMA-BUF tests
│   ├── test_ipc.c                   ← Protocol tests
│   ├── test_params.c                ← Parameter validation
│   └── meson.build
│
├── docs/                             ✅ Already exists
│   ├── investigation/               ✅ Your research docs
│   ├── pre-investigation/           ✅ Planning docs
│   ├── post-investigation/          ✅ Conclusions
│   │
│   ├── architecture/                🔄 To create
│   │   ├── 00-overview.md
│   │   ├── 01-libwlblur.md
│   │   ├── 02-wlblurd.md
│   │   └── 03-integration.md
│   │
│   ├── decisions/                   🔄 ADRs
│   │   ├── 001-why-external-daemon.md
│   │   ├── 002-dma-buf-zero-copy.md
│   │   ├── 003-kawase-algorithm.md
│   │   ├── 004-ipc-protocol.md
│   │   └── 005-scenefx-extraction.md
│   │
│   ├── api/
│   │   ├── libwlblur-reference.md
│   │   ├── parameter-tuning.md
│   │   └── ipc-protocol.md
│   │
│   ├── guides/
│   │   ├── building.md
│   │   ├── compositor-integration.md
│   │   └── troubleshooting.md
│   │
│   └── consolidation/               🔄 Agent outputs
│       ├── shader-extraction.md
│       ├── parameter-comparison.md
│       └── algorithm-analysis.md
│
├── scripts/
│   ├── format.sh                    ← Code formatting
│   ├── generate-protocol.py         ← IPC code gen (optional)
│   └── run-tests.sh
│
└── assets/
    ├── architecture-diagram.svg
    ├── demo.gif
    └── test-images/
        ├── input.png
        └── expected-output.png
```
