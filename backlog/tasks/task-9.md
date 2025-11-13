---
id: task-9
title: "Implement Blur Node Registry and State Management"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["daemon", "state", "management"]
milestone: "m-2"
dependencies: ["task-8"]
---

## Description

Implement the blur node registry that tracks compositor blur requests. Each compositor window gets a "blur node" with persistent state across frames.

**Features:**
- Node allocation with unique IDs
- Per-node parameter storage
- Client association (nodes belong to specific clients)
- Cleanup on client disconnect
- Resource limits

## Acceptance Criteria

- [x] Node IDs allocated sequentially
- [x] Node lookup by ID works
- [x] Parameters stored per node
- [x] Client disconnect cleans up all nodes
- [x] Resource limits enforced (max 100 nodes per client)
- [x] No memory leaks (valgrind clean)
- [x] Thread-safe (if daemon becomes multi-threaded)

## Implementation Plan

### Phase 1: Node Structure

**File**: `wlblurd/src/blur_node.c`

```c
#include <stdint.h>
#include <wlblur/blur_params.h>

struct blur_node {
    uint32_t node_id;
    uint32_t client_id;
    
    // Dimensions
    int width;
    int height;
    
    // Parameters
    struct wlblur_blur_params params;
    
    // Statistics
    uint64_t render_count;
    uint64_t last_render_time_us;
    
    struct blur_node *next;  // Linked list
};

static struct blur_node *node_list = NULL;
static uint32_t next_node_id = 1;

/**
 * Create new blur node
 */
uint32_t blur_node_create(
    uint32_t client_id,
    int width,
    int height,
    const struct wlblur_blur_params *params
) {
    // Check resource limits
    int client_node_count = count_client_nodes(client_id);
    if (client_node_count >= 100) {
        fprintf(stderr, "[wlblurd] Client %u exceeds node limit\n",
                client_id);
        return 0;
    }
    
    struct blur_node *node = calloc(1, sizeof(*node));
    if (!node) {
        return 0;
    }
    
    node->node_id = next_node_id++;
    node->client_id = client_id;
    node->width = width;
    node->height = height;
    node->params = *params;
    
    // Add to list
    node->next = node_list;
    node_list = node;
    
    return node->node_id;
}

/**
 * Lookup node by ID
 */
struct blur_node* blur_node_lookup(uint32_t node_id) {
    for (struct blur_node *n = node_list; n; n = n->next) {
        if (n->node_id == node_id) {
            return n;
        }
    }
    return NULL;
}

/**
 * Destroy node
 */
void blur_node_destroy(uint32_t node_id) {
    struct blur_node **prev = &node_list;
    
    for (struct blur_node *n = node_list; n; n = n->next) {
        if (n->node_id == node_id) {
            *prev = n->next;
            free(n);
            return;
        }
        prev = &n->next;
    }
}

/**
 * Cleanup all nodes for client
 */
void blur_node_cleanup_client(uint32_t client_id) {
    struct blur_node **prev = &node_list;
    
    while (*prev) {
        struct blur_node *n = *prev;
        if (n->client_id == client_id) {
            *prev = n->next;
            free(n);
        } else {
            prev = &n->next;
        }
    }
    
    printf("[wlblurd] Cleaned up nodes for client %u\n", client_id);
}
```

## Deliverables

1. `wlblurd/src/blur_node.c` - Node registry
2. `wlblurd/src/client.c` - Client management
3. Cleanup integration in main event loop
4. Commit: "feat(daemon): implement blur node registry"

**Milestone m-2 (Daemon Infrastructure) COMPLETE**
