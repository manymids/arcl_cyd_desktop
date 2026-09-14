# ARCL Common Specification (for Retro-Machine Emulators)

> **External specification:** this document belongs to a project separate from CYD Desktop
> Sandbox and is included, at v0.5, for reference. CYD Desktop Sandbox's MCP tools follow it
> (see [MCP tools](../mcp-tools_en.md) for the extent). This repository's MIT License does not
> apply to this document. The text below is unchanged.

[日本語版](arcl-common-spec_ja.md)

**Version:** v0.5  
**Purpose:** A standalone specification for adding an AI-oriented MCP experimentation platform, ARCL, to any retro-machine emulator. It is intended to be sufficient for design and implementation without another document.

Examples include MSX, PC-98, FM TOWNS, NES, Mega Drive, and X68000.

| Version | Change |
|---|---|
| v0.1 | Initial design: layers and execution model |
| v0.2 | Standardized verbs and primary arguments as a common contract |
| v0.3 | Split common `arcl_*` APIs from machine-specific `{machine}_*` APIs |
| v0.4 | Reorganized as a standalone specification |
| v0.5 | Specified observation resolution, turn efficiency, and audio observation; added cropping/scaling, input macro/state, audio recording, memory watchpoints, and `frames_used` semantics |

---

## 0. How to use this document

An implementation must preserve the following structure:

| Area | Requirement | Status |
|---|---|---|
| Design framework | Capability layers, the OTA loop, Observation / Action / Control, AI-controlled time | **Required** |
| Protocol framework | Frame and execution state in responses, layer filtering, stdio MCP | **Strongly recommended** |
| Common APIs | `arcl_xxx`, including verbs and primary arguments | **Required**; see section 7 |
| Machine-specific APIs | `{machine}_xxx`, such as `x68k_extmem` or `towns_cd` | Add only when required |
| Implementation details | Console recovery, key arrays, chip decoding, and so on | Designed per machine; keep the contract names aligned |

Agents should write procedures using `arcl_*` first, then `{machine}_*` only when needed. ARCL's value is not merely a tool list: it is an experimentation platform in which the machine does not advance while the AI is thinking.

---

## 1. Goal

Provide a common platform on which an AI can safely control, observe, and experiment with retro machines.

Typical uses are retro-software development, build and test automation, debugging and reverse engineering, emulator development, and AI-agent research or autonomous experiments. Game-playing AI is only one use case.

## 2. Core idea

Treat the emulator as a virtual laboratory controlled by AI.

| Property | Requirement |
|---|---|
| Safe | Operations remain inside the guest virtual machine. Any route to the host is explicitly limited. |
| Reproducible | The same restored state and input sequence produce the same result within a documented guarantee boundary. |
| Restorable | Save states and snapshots can return the machine to an earlier point. |

This distinguishes ARCL from systems that directly automate a modern host OS.

## 3. Mandatory design principles

### 3.1 Capability layers

Expose abilities to AI through layers. Numbers indicate abstraction level, not implementation dependencies.

| Layer | Name | Abstraction | Core scope |
|---|---|---|---|
| L0 | Human Interface | Human-equivalent | Display and input devices |
| L1 | OS / Shell Interface | OS interaction | Console text, commands, media changes |
| L2 | Debug Interface | Software internals | Memory, registers, disassembly, breakpoints |
| L3 | Emulator / Hardware Interface | Hardware internals | **Decoded** chip state |
| L4 | Laboratory Interface | AI-specific | Snapshots, rewind, speed, experiment branches |

- Every layer must be usable independently.
- Select exposed layers at launch; disabled-layer tools must not appear in `tools/list` (progressive disclosure).
- Do not change the enabled layer set while running.

### 3.2 Agent execution model (OTA)

```text
Observe → Think → Act → Observe → …
```

| Phase | Meaning |
|---|---|
| Observe | Read state through a layer. |
| Think | LLM reasoning, which may take seconds of real time. |
| Act | Send input, commands, or an experiment. |

Do not assume the machine advances while the agent thinks. The AI controls time; this is the highest-priority principle.

### 3.3 Observation, Action, and Control

| Category | Role | Effect on machine |
|---|---|---|
| Observation | Read state | None |
| Action | Change state | Yes |
| Control | Time, save, restore | Operates on the timeline rather than guest contents |

Control is always exposed independently of capability layers.

#### Exception: interval observations

An observation that concerns an interval, rather than an instant, must advance time for that interval. Audio recording (`arcl_audio_record`) is the canonical example: audio has no single-frame equivalent.

Such a tool remains an Observation but must accept the amount to advance in `frames`, return the actual amount in `frames_used`, state prominently that it advances the machine, and use the same advancement semantics as `arcl_run`. No ordinary read-only observation may advance time.

---

## 4. Mandatory execution model

### 4.1 Execution states

| State | Meaning |
|---|---|
| Paused | No frame, or equivalent quantum, advances. |
| Step run | Consume the requested quanta at host-limited speed, then pause. |
| Continuous run | Keep running at real-time-equivalent speed. |

### 4.2 Unit of time

- The default quantum is one **frame** (one vertical refresh).
- Do not make seconds of wall-clock time an API control unit.
- If the machine has no frame concept, explicitly define a fixed clock tick, scanline, or other quantum.

Audio may report its content in samples and seconds, but control remains in frames. `arcl_audio_record`, for example, accepts `frames`, not seconds.

### 4.3 Required Control tools

| Role | Common name | Contract |
|---|---|---|
| Advance and pause | `arcl_run` | Run for requested frames or until a stop condition; always return paused and synchronously. |
| Pause | `arcl_pause` | Pause execution. |
| Continuous run | `arcl_resume` | Begin continuous execution. |
| Status | `arcl_status` | Return execution state, frame, machine ID, and configuration. |
| Reset | `arcl_reset` | Reset the machine. |
| Persistent state | `arcl_save_state` / `arcl_load_state` | Save to or restore from named slots. |

The `machine` value in `arcl_status` (for example, `"x68k"`, `"towns"`, or `"msx"`) lets an agent determine which `{machine}_*` tools are applicable.

### 4.4 Common response fields

Every `arcl_*` and `{machine}_*` response must include:

| Field | Meaning |
|---|---|
| `frame` | Position on the machine timeline at completion. |
| `running` or `state` | Current execution state. |

An agent must be able to compare frames from consecutive responses to determine whether the machine advanced. Restoring state must also restore `frame`; do not use a host-lifetime counter.

Every tool that advances the machine must return `frames_used`, not only `arcl_run`. This includes input taps, input macros, and interval observations. Do not require clients to infer it from a previous frame value.

### 4.5 Atomicity

- Serialize tool processing at frame boundaries, or at the defined quantum boundary.
- Never return a partially executed frame during continuous execution.
- A tool must not require the machine to be paused as a precondition; pausing is a choice available to the AI.

For a multi-step tool, each step must be atomic, but the entire sequence need not be. If `arcl_input_macro` fails partway through, stop there and report how far it got. Do not silently skip the remainder and do not roll the whole sequence back; timeline rollback belongs to L4, not to an implicit Action side effect.

### 4.6 Input representation

Inputs must support `press`, `release`, and `tap(frames)`. A tap advances the machine while held; a call made while paused advances N frames and pauses again.

#### Held input is state

A press remains active across `arcl_run` calls until released. It is therefore machine state and must be readable through `arcl_input_state`. Do not force an agent to infer held input from the display.

#### Batch timeline input

Require `arcl_input_macro`, a single-call tool that accepts a sequence of input and time steps. Each step uses the same arguments as the corresponding single tool; do not invent macro-only shorthand. The sequence must be able to contain `run` steps and intermediate observations, and must have limits on step count and total frames.

### 4.7 `arcl_run` stop conditions

At minimum, support:

| Condition | Meaning |
|---|---|
| `frames` | Consume the requested number of frames. |
| `text_match` | A string appears in the console, when L1 is available. |
| `until_break` | A breakpoint or watchpoint fires, when L2 is available. |

Optional conditions include `screen_stable` for an unchanged screen over N frames and machine-specific conditions such as VBlank, I/O, or IRQ. A stop response must include `stop_reason` and `frames_used`. A breakpoint stop must additionally report the PC and a human-readable account of the access: address, byte length, value, and whether it was read or written.

### 4.8 Reproducibility boundary

- **Guaranteed:** restoring the same state and supplying the same input on the same step frame produces the same frame sequence.
- **Not guaranteed:** input arrival during continuous execution, RTC, host filesystem behavior, and real-device output.
- Each implementation's README must state the exact reproducibility boundary.

---

## 5. Recommended system architecture

### 5.1 Placement

```text
MCP client (AI)
       │ MCP (JSON-RPC)
       ▼
┌──────────────────────────────┐
│ Emulator executable           │
│   MCP server layer            │
│       │ existing frontend API │
│   Emulation core              │
└──────────────────────────────┘
```

Embed MCP in the emulator executable rather than controlling a separate process from outside; this makes time control and atomicity tractable. The MCP layer should use existing frontend/host APIs rather than manipulate the emulation core directly.

### 5.2 Transport

- Default: **stdio**, for example through `--mcp`, with stdout reserved for MCP.
- Optional: loopback TCP only; never bind to an external interface.
- Support one client at a time.

### 5.3 GUI coexistence

When MCP is active, it is recommended that a human can still observe and operate the same window. Human and AI input may merge into the same input state.

### 5.4 Security

- Use loopback only.
- Limit host-filesystem exposure to explicitly shared roots and reject path traversal.
- Do not expose arbitrary host-command execution through MCP.

---

## 6. Layer contracts and machine differences

### 6.1 L0 Human Interface

| Kind | Common capability | Machine-specific examples |
|---|---|---|
| Observation | Display image plus true resolution/aspect, with crop and scale | Multiple resolutions, interlace |
| Observation | Held input state | Key layout, button count |
| Observation | Interval audio capture and level | Sound configuration, sample rate |
| Action | Keyboard, pad, mouse equivalents | Key layout, light gun, button count |
| Action | Batch input timeline | None; common |

Common L0 tools are `arcl_screenshot`, `arcl_input_state`, `arcl_audio_record`, `arcl_key`, `arcl_type`, `arcl_joypad`, `arcl_mouse`, `arcl_clear_input`, and `arcl_input_macro`.

L0 is human-equivalent, not cheap. Images, sound, and input all belong here. Chip-register observation is L3; observing the sound actually produced is L0.

#### Mandatory observation resolution

A full-size screenshot alone is insufficient. `arcl_screenshot` must at least provide rectangle cropping (`x`, `y`, `w`, `h`) and nearest-neighbor integer scaling (`scale`). Linear interpolation must not be used because it invents colors absent from pixel art. `grid` for cell boundaries and `path` / `inline` for host saving are recommended.

Limit total scaled pixels and return an error instructing the client to crop or reduce scale. Responses must include both source dimensions and the crop rectangle, not merely the scaled dimensions.

#### Mandatory audio observation

`arcl_audio_record` records while advancing a requested number of frames and returns machine-verifiable values such as peak, RMS, silence status, and optionally the sample position at which sound begins. It should be able to save WAV or equivalent output to the host.

Its recording route must be independent of the playback ring buffer: emulation can outrun host time during `arcl_run`, which makes a playback buffer overflow. Enforce a recording-length limit. It is the interval-Observation exception described in section 3.3.

### 6.2 L1 OS / Shell Interface

| Kind | Common capability | Machine-specific implementation |
|---|---|---|
| Observation | Return console content as UTF-8 text | Text VRAM, terminal buffer, serial log, font-ROM matching, OCR only as last resort |
| Action | Send command | Key injection or a true shell channel |
| Action | Media and shared folders | Disk attach/eject, host-shared drive |
| Composite | `arcl_command` | Type + run until prompt + console read |

Prefer text to images when text is enough; it is more reliable and token-efficient. A machine without a shell may omit L1 and target L0 + L2 use cases.

### 6.3 L2 Debug Interface

| Kind | Common capability | Machine-specific aspect |
|---|---|---|
| Observation | CPU registers, memory, disassembly, stack window | Register set, banks, mirrors, bus width, ISA, ABI |
| Action | Memory writes, breakpoints, stepping | Whether the core can stop per instruction |
| Action | Read/write memory watchpoints | Memory-access path layout |

Document cases where reported breakpoint values are exact but the actual stopping granularity is one frame.

#### Mandatory memory watchpoints

An exec breakpoint cannot answer “who overwrote this address?” efficiently. Extend `arcl_breakpoint` with `type` (`exec`, `read`, `write`, `access`) and `length`, rather than inventing a separate tool. Share `until_break` as its stop condition.

Place hooks at a point that every guest access traverses, including CPU, DMA, and I/O space. A disabled watchpoint must cost no more than one load and an untaken branch in the emulator's inner loop. Report the executing-instruction PC, read/write direction, size, value, and address. Document if instruction fetches can trigger a read watchpoint.

### 6.4 L3 Hardware Interface

L3's value is decoded, named state: display mode, transformed sprite coordinates, or named IRQ causes. It is not a second spelling of raw memory access, which belongs in L2. The observed devices differ per machine (VDP, PPU, CRTC, sprites, DMA, sound registers, and so on); L3-specific Actions are optional because L2 memory writes are often sufficient.

L0 audio recording answers what was actually produced. Sound-chip registers answer what the program intended to produce. Both are required to distinguish bad music data from a faulty voice or mixer. Expose machine-specific decoded tools such as `x68k_opm`, `nes_apu`, or `md_ym2612`; do not merely return an undecoded register byte array.

Many chips have write-only registers and the emulator may not retain a register file. In that case, shadow writes for observation. Because the shadow is not included in save states, document that it can become stale after a state load.

### 6.5 L4 Laboratory Interface

| Capability | Requirement |
|---|---|
| Snapshot | Named, in-memory, non-slot-consuming experiment branch |
| Rewind | Automatically captured ring buffer; report memory consumption |
| Speed | Real-time ratio during continuous execution |
| Clone | Sequential branching is acceptable when parallel instances are impractical; document it |

For large states, depth × state size is the real limit. Return the consumption in tool responses.

---

## 7. Naming: `arcl_*` and `{machine}_*`

### 7.1 Two prefixes

| Family | Form | Meaning | Example |
|---|---|---|---|
| Common | `arcl_{verb}` | Same contract across machines | `arcl_run`, `arcl_screenshot` |
| Machine-specific | `{machine}_{verb}` | Machine-only capability or detail outside the common contract | `x68k_extmem`, `towns_cd` |

Use `arcl_*` for procedures portable across machines, and `{machine}_*` only for machine, hardware, OS, or experiment-specific needs.

- Never prefix a common ability with a machine name: use `arcl_run`, not `x68k_run`.
- Never prefix a machine-only ability with `arcl_`.
- Do not reinvent common verbs: no `arcl_advance` when `arcl_run` exists.
- One server represents one machine; expose only its `arcl_*` and its own `{machine}_*` tools.

### 7.2 Multiple servers

If a client connects to multiple machine servers, distinguish their same-named common tools by: (1) the MCP server identity, (2) `arcl_status.machine`, and (3) the visible machine-specific tools. Write skills around `arcl_*`, branching only on `machine` and `{machine}_*`.

### 7.3 Common API list

Do not expose unimplemented verbs, but do not supply aliases for them either.

| Layer | Tool | Category | Contract |
|---|---|---|---|
| Control | `arcl_run` | Control | Run requested frames or to a stop condition; return paused. |
| Control | `arcl_pause`, `arcl_resume`, `arcl_status`, `arcl_reset` | Control | Pause, continuous run, status, and reset. |
| Control | `arcl_save_state`, `arcl_load_state` | Control | Save to and restore from persistent slots. |
| L0 | `arcl_screenshot` | Observation | Image with crop, nearest-neighbor scale, grid, path, and inline controls. |
| L0 | `arcl_input_state`, `arcl_audio_record` | Observation | Held input; interval audio with peak/RMS/silence and optional WAV. |
| L0 | `arcl_key`, `arcl_type`, `arcl_mouse`, `arcl_joypad`, `arcl_clear_input`, `arcl_input_macro` | Action | Input operations and a one-call input/run timeline. |
| L1 | `arcl_console_read`, `arcl_host_dir` | Observation | UTF-8 console and optional shared-host directory list. |
| L1 | `arcl_mount`, `arcl_command` | Action / Composite | Media attachment and command/type/wait/read. |
| L2 | `arcl_registers`, `arcl_read_mem`, `arcl_disasm`, `arcl_stack` | Observation | Debug observations. |
| L2 | `arcl_write_mem`, `arcl_write_registers`, `arcl_breakpoint`, `arcl_step` | Action | Debug modifications; stepping only where supported. |
| L3 | `arcl_video`, `arcl_palette`, `arcl_sprites`, `arcl_dma`, `arcl_irq`, `arcl_vram` | Observation | Decoded video/hardware state, except raw VRAM. |
| L4 | `arcl_snapshot`, `arcl_rewind`, `arcl_speed` | Control | Named snapshots, rewind history, and continuous-run speed. |

### 7.4 Standard arguments and responses

| Subject | Required common form |
|---|---|
| Input | `action`: `tap` / `press` / `release`; `frames` for hold duration |
| `arcl_run` | `frames` and stop conditions such as `text_match` and `until_break` |
| Memory | `address` as integer or hexadecimal string; `length` / `count` |
| Breakpoints | `action`: `add` / `remove` / `clear` / `list`; `type`; `length` |
| Display crop | Source-frame `x`, `y`, `w`, `h`; integer `scale`; `grid` |
| Host saving | `path`; `inline`, defaulting to true |
| Time-advancing tool | Accept `frames`, return `frames_used` |
| Every response | `frame` and `state` / `running` |
| Stops | `stop_reason`; breakpoint site and reason for `until_break` |
| `arcl_status` | Required `machine` |

Machine-specific arguments may be added, but common arguments must not be renamed: for example, do not replace `frames` with `duration` or `x`/`y`/`w`/`h` with `rect` or `crop`.

### 7.5 Tool descriptions

Each description should state, in order: the family (`Common (arcl)` or `Machine-specific (x68k)`), category and layer, one sentence on the OTA use, and any machine-specific pitfall. `serverDescription` should include an OTA summary and the `machine` ID.

### 7.6 Machine-specific APIs

Use a machine-specific API when it has no cross-machine equivalent, common wording would mislead, or it only matters for a particular OS, board, or extension. The form is always lowercase `{machine}_{verb}`; examples of machine IDs are `x68k`, `towns`, `msx`, `pc98`, `nes`, and `md`.

Even if an L3 response contains hardware register names, retain a common name such as `arcl_video` when its role is cross-machine. The agent can follow the shared procedure “inspect decoded display state” and apply machine knowledge only to fields.

Example: `x68k_extmem` is a Control/L4 tool for the X68030 extended-RAM window (`$01000000`–`$10FFFFFF`), with `status`, `on`, `off`, and `reset_trace`. It is machine-specific because the address window and 24/32-bit masking transition are X68030-specific. Its description should explain that enabling before and after boot are distinct experiments, that changes apply at a frame boundary, and that traces distinguish in-window accesses from above-window bus errors.

Comparable examples are `towns_os` / `towns_cd`, `msx_slot` / `msx_mapper`, `pc98_gdc` / `pc98_egc`, `nes_ppu` / `nes_apu`, and `md_vdp` / `md_z80`.

### 7.7 Composite tools

A tool spanning Action, Control, and Observation is acceptable only if the same outcome is decomposable into common tools.

| Tool | Decomposition |
|---|---|
| `arcl_command` | `arcl_type` + `arcl_run` + `arcl_console_read` |
| `arcl_input_macro` | A sequence of corresponding single-tool steps |
| `arcl_audio_record` | `arcl_run` plus an inseparable interval recording |

Composite tools exist solely to reduce round trips. Do not hide a unique capability inside one; conversely, turn an operation that actually consumes dozens of round trips into a composite tool.

---

## 8. Adoption checklist

1. Define the time quantum, normally a frame.
2. Implement all `arcl_*` Control tools and include `machine` in status.
3. Include `frame` and execution state in every response; return `frames_used` from time-advancing tools.
4. Confirm the OTA loop using L0 screenshot and primary input.
5. Make L0 observations decision-quality: crop/scale, held-input readback, and audio recording/level.
6. Add `arcl_input_macro` to collapse input round trips.
7. Decide whether L1 is needed; if it is, provide string observation and use `arcl_console_read` / `arcl_command`.
8. Re-expose available debugging capabilities as L2 common tools, document step granularity, and include watchpoints.
9. Use common L3 names where the role is common; use `{machine}_*` for details that do not fit.
10. Add L4 snapshots and machine-only experiment switches where appropriate.
11. Add layer filtering and loopback-only transport.
12. Document the reproducibility guarantee boundary in the implementation README.

### 8.1 Observation self-check

| Question | Missing capability if the answer is no |
|---|---|
| Can an agent visually judge whether a 16×16 sprite is correct? | Screenshot crop and scale |
| Can it determine whether a supposedly held input is actually held? | Input-state readback |
| Can it determine whether BGM is silent or merely wrong in pitch? | Audio capture plus sound-register observation |
| Can it identify the code that writes a given address? | Memory watchpoint |
| Can it walk a character across a screen without dozens of turns? | Input macro |

## 9. Minimum MVP

| Priority | Contents |
|---|---|
| P0 | stdio MCP, `--mcp-layers`, Control tools, and L0 screenshot with crop and integer scale, primary input, and clear input |
| P1 | Remaining L0 input/state/macro tools; L1 console and command where a shell exists; `text_match` stop condition |
| P2 | L2 registers, memory, disassembly, breakpoints/watchpoints; L4 snapshots; audio recording where audio matters |
| P3 | L3 common tools, rewind, speed, and machine-specific APIs including sound registers |

The acceptance baseline is: `tools/list` works under `--mcp`; repeated `arcl_run` reaches boot or a title without real-time waiting; save → action → load → the same action yields the same display while stepping; continuous execution never produces a torn screenshot; GUI interaction remains possible where practical; cropped/scaled dimensions are correct; and held input is visible after press/run and absent after release.

## 10. Layer combinations by use

| Use | Layers |
|---|---|
| Game AI | L0 + Control |
| Development AI with shell | L0 + L1 + Control |
| Graphics work | L0 with crop/scale + L1 + Control |
| Sound work | L0 audio capture + L1 + Control + L3 machine-specific sound registers |
| Debugging AI | L0 + L1 + L2 with watchpoints + Control |
| Emulator development | Add L3 |
| AI research / autonomous experiments | All layers |

For graphics and sound work, L0 observation resolution matters more than layer depth: the agent must be able to verify its own output.

## 11. Design philosophy

- The purpose is not to automate human input faithfully.
- Provide both a human-equivalent evaluation environment (L0) and AI-optimized capabilities (L1–L4) in one architecture.
- Do not require AI to keep pace with real time; allow pause and resume for thinking.
- ARCL provides an experimentation platform for observation, reasoning, and experiments, not merely a collection of MCP tools.

## Appendix: terms

| Term | Definition |
|---|---|
| ARCL | AI Retro Computer Laboratory; the overall system and origin of the `arcl_` prefix. |
| `arcl_*` | Machine-independent common MCP tools. |
| `{machine}_*` | Machine-specific MCP tools, such as `x68k_` or `towns_`. |
| OTA | Observe–Think–Act. |
| Quantum | Smallest unit of timeline advancement, normally a frame. |
| Snapshot | Named in-memory state for an experiment branch. |
| Save State | State stored in a persistent slot. |
| Progressive Disclosure | Filtering the tool list by selected layers at startup. |
| watchpoint | A breakpoint on reads or writes to a memory range, configured with `arcl_breakpoint.type`. |
| Input macro | A composite Action that executes a sequence of input and time steps in one call. |
| Interval observation | An observation over a duration that necessarily advances the machine, for example `arcl_audio_record`. |
