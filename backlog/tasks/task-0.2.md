---
id: task-0.2
title: "Write All Five Architecture Decision Records (ADRs)"
status: Done
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["documentation", "decisions"]
milestone: "m-0"
dependencies: []
---

## Description

Write complete ADRs for the five key architectural decisions. These explain WHY we chose our approach and invite feedback.

ADRs to write:
1. 001-why-external-daemon.md
2. 002-dma-buf-vs-alternatives.md
3. 003-kawase-algorithm-choice.md
4. 004-ipc-protocol-design.md
5. 005-scenefx-extraction.md

## Acceptance Criteria

- [x] All 5 ADRs in `docs/decisions/`
- [x] Each follows ADR template (Context, Decision, Consequences)
- [x] References investigation docs
- [x] Explains tradeoffs
- [x] Invites community feedback

## ADR Template

```markdown
# ADR-XXX: Title

**Status**: Proposed  
**Date**: 2025-01-15

## Context
[What problem are we solving?]

## Decision
[What did we decide?]

## Alternatives Considered
[What else could we have done?]

## Consequences
**Positive:**
- ...

**Negative:**
- ...

## References
- Investigation docs
- Related discussions
```

## References

- `docs/post-investigation/` - All rationale documented here
- `docs/investigation/` - Technical evidence

