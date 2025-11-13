# Architecture Decision Records

This section documents all major architectural decisions made during wlblur development.

## Index

### Core Decisions
- [ADR-001: External Daemon Architecture](ADR-001-External-Daemon) - Why separate process
- [ADR-002: DMA-BUF for Zero-Copy](ADR-002-DMA-BUF) - GPU texture sharing
- [ADR-003: Kawase Algorithm Choice](ADR-003-Kawase-Algorithm) - Blur algorithm selection
- [ADR-004: IPC Protocol Design](ADR-004-IPC-Protocol) - Unix socket + SCM_RIGHTS
- [ADR-005: SceneFX Shader Extraction](ADR-005-SceneFX-Extraction) - Shader reuse strategy
- [ADR-006: Daemon Configuration](ADR-006-Daemon-Config) - Preset-based config

### Supporting Analysis
- [Why IPC Is Better](Why-IPC-Is-Better) - Detailed IPC advantages

## Reading Order

**For new contributors:**
1. Start with [ADR-001](ADR-001-External-Daemon) (why external daemon)
2. Then [ADR-002](ADR-002-DMA-BUF) (how zero-copy works)
3. Then [ADR-006](ADR-006-Daemon-Config) (configuration approach)

**For compositor developers:**
1. [ADR-001](ADR-001-External-Daemon) (architecture overview)
2. [ADR-004](ADR-004-IPC-Protocol) (IPC protocol)
3. [ADR-006](ADR-006-Daemon-Config) (preset system)

**For performance analysis:**
1. [ADR-002](ADR-002-DMA-BUF) (DMA-BUF benchmarks)
2. [ADR-003](ADR-003-Kawase-Algorithm) (algorithm performance)

## Decision Status

All ADRs are currently in "Proposed" status and will be validated during implementation phases.

## Contributing

When proposing new architectural decisions:
1. Create a new ADR file (ADR-XXX-Title.md)
2. Follow the existing template structure
3. Consider alternatives thoroughly
4. Document consequences (both positive and negative)
5. Link to related ADRs and investigation docs
