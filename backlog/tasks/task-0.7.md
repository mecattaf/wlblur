---
id: task-0.7
title: "Write IPC Protocol Specification Early (Pre-Code)"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["protocol", "design", "documentation"]
milestone: "m-0"
dependencies: []
---

## Description

Write the complete IPC protocol specification now (normally task-9), since it's pure design work with no code dependencies. This can be done in parallel with everything else.

## Acceptance Criteria

- [x] `docs/api/ipc-protocol.md` complete
- [x] Message structures defined
- [x] Operation codes enumerated
- [x] SCM_RIGHTS mechanism explained
- [x] Error codes defined
- [x] Versioning strategy
- [x] Example message flows
- [x] Security considerations

## Structure

```markdown
# wlblur IPC Protocol Specification

Version: 1.0

## Overview
- Unix domain socket
- Binary structs
- SCM_RIGHTS for FD passing

## Socket Path
`/run/user/$UID/wlblur.sock`

## Message Format

### Request Structure
```c
struct blur_request {
    uint32_t protocol_version;  // 1
    uint32_t op;                // Operation code
    uint32_t node_id;           // Blur node ID
    uint32_t width, height;     // Texture dimensions
    // Parameters follow
    // DMA-BUF FD sent via SCM_RIGHTS
};
```

### Response Structure
...

## Operations

### BLUR_OP_CREATE_NODE (1)
**Purpose**: Allocate blur node
...

### BLUR_OP_RENDER_BLUR (3)
**Purpose**: Perform blur operation
...

## Error Codes
- 0: SUCCESS
- 1: INVALID_NODE
- 2: DMABUF_IMPORT_FAILED
...

## Message Flow Examples

### First-time blur
[Diagram]

### Cached blur
[Diagram]

## Security
- Socket permissions: 0700
- Parameter validation: All ranges checked
- Resource limits: Max 100 nodes per client
...
```

## References

- `docs/investigation/scenefx-investigation/daemon-translation.md`
- `docs/post-investigation/blur-daemon-approach.md`

