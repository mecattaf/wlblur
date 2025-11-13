---
id: task-10
title: "Implement ScrollWM Blur Integration Module"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["integration", "scroll", "compositor"]
milestone: "m-3"
dependencies: ["task-9"]
---

## Description

Create the ~220-line blur integration module for ScrollWM that connects to wlblurd. This proves the external daemon approach works with real compositor constraints.

**Features:**
- Detect blur-eligible windows
- Export backdrop as DMA-BUF
- Send IPC requests to daemon
- Import blurred results
- Composite into scene graph

## Acceptance Criteria

- [x] blur_integration.c created (~220 lines)
- [x] Connects to wlblurd socket
- [x] Detects windows needing blur
- [x] Exports backdrop using wlr_buffer APIs
- [x] Sends IPC requests correctly
- [x] Imports blurred DMA-BUF
- [x] Composites result into scene graph
- [x] Handles daemon unavailable gracefully
- [x] No crashes or leaks
- [x] Compile-time optional (meson flag)

## Implementation Plan

[Complete ScrollWM integration implementation with all compositor-specific details]

## Deliverables

1. `integrations/scroll/blur_integration.c`
2. `integrations/scroll/blur_integration.h`
3. `integrations/scroll/scroll.patch` - Git patch for ScrollWM
4. `integrations/scroll/README.md` - Integration guide
5. Commit: "feat(scroll): implement ScrollWM blur integration"
