# Design Schwung MIDI FX Module

## Purpose
Use this skill to convert a functional idea into a concrete Schwung MIDI FX module design.

This is the architecture and manifest design step. It should happen before implementation.

## Goal
Produce a module design that is:
- valid for Schwung
- consistent with this project's conventions (engine/wrapper split, parameter model)
- usable in the Move Signal Chain
- ergonomic on Ableton Move

## Design Rules
Assume a chainable MIDI FX component unless clearly stated otherwise.
Keep the first version intentionally small and stable.
Prefer:
- a compact parameter set (2–8 primary controls)
- one root UI level
- predictable knob assignment
- simple state persistence
- a portable engine core if timing or generation matters

## Design Process

### 1. Module Identity
Define:
- `id` — lowercase, filesystem-safe, unique
- `name` — display name
- `abbrev` — 2–3 characters for compact display
- `version` — start at `0.1.0`
- built-in vs external

### 2. Capability Shape
Confirm:
- `audio_in: false`, `audio_out: false` for MIDI FX
- `midi_in: true`, `midi_out: true`
- `chainable: true`
- `component_type: "midi_fx"`

### 3. Parameter Model
Define the `ui_hierarchy.levels.root` with:
- `knobs` — the 8 hardware knobs, ordered by importance
- `params` — full parameter list

Parameter rules:
- Prefer `float` (0.0–1.0), `int`, `enum`, or `toggle`
- Every parameter must include a sensible `default`
- Knob assignments must match the most important controls
- Chain editing params (`chain_params`) should include the live-editable subset

If the module is melodic, decide explicitly whether it needs a `scale` / `mode` parameter.

### Scale Design Process
If pitch output depends on a note palette, define scales before writing code:

1. Decide whether the module needs:
- one fixed palette
- a small `scale` enum
- or a reduced V1 with only 3–6 useful scales

2. Separate clearly:
- `root` = transposition
- `scale` = pitch vocabulary
- `range` = register shape
- `spread` = upward bias inside that register

3. Prefer scales that are immediately playable on Move.

Recommended starter palette for generative melodic modules:
- `ionian`
- `aeolian`
- `dorian`
- `mixolydian`
- `major_pent`
- `minor_pent`
- `suspended`
- `power`

Good expansion set when the module needs more color:
- `phrygian`
- `lydian`
- `harmonic_minor`
- `blues`

Advanced color set for more distinctive modules:
- `locrian`
- `melodic_minor`
- `whole_tone`
- `diminished`
- `phrygian_dominant`

4. Keep V1 selective.
Do not add scales just to look complete. Add them when each one creates a distinct musical behavior.

### 4. File Layout
Minimal files for a native MIDI FX:
```
module.json
dsp.so          ← compiled from:
  dsp/<module>_engine.h
  dsp/<module>_engine.c
  host/<module>_plugin.c
```

If a custom Move UI is needed:
```
ui.js           ← full-screen Move UI
ui_chain.js     ← compact chain-context UI
```

Default: use standard Schwung parameter UI, no custom `ui.js`.

### 5. Engine Architecture Decision
Choose the implementation model:

**Two-layer split (preferred for generation/timing):**
- Portable engine: `dsp/<module>_engine.h/.c` — no Schwung headers
- Host wrapper: `host/<module>_plugin.c` — Schwung API, MIDI, params

**Single file (acceptable for simple transforms):**
- `host/<module>_plugin.c` handles everything

Use the two-layer split when:
- the module generates timed events (requires `tick()`)
- the module needs independent testing without hardware
- the logic is complex enough to warrant isolation

### 6. State Strategy
Define whether the module needs:
- simple param recall only (most modules — just implement `save_state` / `load_state`)
- internal sequencer or loop state
- held-note state exclusion (never serialize active notes)
- transport-dependent reset rules

### 7. Clock Sync Strategy
Decide explicitly:
- `Sync = move` — uses `get_clock_status()` + `get_bpm()` (requires MIDI Clock Out enabled)
- `Sync = internal` — uses fixed or user-specified BPM constant
- Free-running — constant BPM, no clock API calls

Document which clock APIs are called. Free-running modules must NOT call `get_clock_status()` or `get_bpm()`.

### 8. Move Interaction Strategy
Define:
- knobs 1–8 mapped to parameters
- whether Shift modifiers are needed
- whether pads or step buttons add value
- LED behavior (informative, not decorative)
- menu/back structure if multiple screens

## Required Output Format

### Proposed File Tree
List every file that will be created.

### Proposed `module.json`
Write a complete draft manifest.

### Parameter Specification
For each parameter:
- key, name, type, options or min/max, default, knob assignment

### Engine Architecture Decision
State the chosen model and why.

### Clock Strategy
State the sync approach explicitly.

### State Strategy
Explain how state will be saved and restored.

### UX Plan
Describe exactly how the user interacts with the module on Move.

## Guardrails
- Do not invent a UI hierarchy more complex than needed.
- Do not expose parameters the engine cannot actually support.
- Do not use DAW metaphors that don't translate to Move hardware.
- Prefer an implementation testable incrementally.
- Keep V1 small. Avoid speculative parameters.
