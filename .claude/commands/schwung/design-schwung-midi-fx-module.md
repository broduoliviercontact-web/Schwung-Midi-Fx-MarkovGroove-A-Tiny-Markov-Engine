# Design Schwung MIDI FX Module

## Purpose
Use this skill to convert a functional idea or audited source project into a concrete Schwung MIDI FX module design.

This is the architecture and manifest design step.
It should happen before implementation.

## Goal
Produce a module design that is:
- valid for Schwung
- consistent with existing repo conventions
- usable in the Signal Chain
- ergonomic on Ableton Move

## Design Rules
The design must assume a chainable MIDI FX component.
The design should fit naturally into Schwung’s module structure and parameter model.

Prefer:
- a compact parameter set
- one clear root UI level
- predictable knob assignment
- simple state persistence
- a clean native core if timing matters

## Design Process

### 1. Module Identity
Define:
- `id`
- `name`
- `abbrev`
- `version`
- built-in vs external

Keep ids lowercase and filesystem-safe.

### 2. Capability Shape
Design the module as a Signal Chain MIDI FX component.

Assume the module should behave like existing chainable MIDI FX modules unless there is a strong reason not to.

**Standard MIDI FX capability block:**
```json
"capabilities": {
    "midi_in": true,
    "midi_out": true,
    "chainable": true,
    "component_type": "midi_fx"
}
```

**Optional capability flags to consider:**
| Flag | When to use |
|------|-------------|
| `"raw_midi": true` | Module needs unprocessed MIDI — skips velocity curve, aftertouch filter, and knob-touch note filter |
| `"raw_ui": true` | Module takes full UI control — host won't intercept Back; module must call `host_return_to_menu()` |
| `"claims_master_knob": true` | Module needs CC 79 (volume knob) for its own use |
| `"skip_led_clear": true` | Module overlays on existing pad colors and must not have LEDs cleared on load |

**Install path** for this `component_type`:
`/data/UserData/schwung/modules/midi_fx/<id>/`

**MIDI source modules** (modules that generate MIDI independently via `ui_chain.js`) use a patch entry like:
```json
"midi_source": { "module": "<id>" }
```
Chain will enter the source UI when a patch with this source loads. Back exits to chain view; Menu re-enters source UI.

### 3. Parameter Model
Define the `ui_hierarchy` root level with:
- `params`
- `knobs`

Prefer 2 to 8 primary controls.
Do not overload the first version with deep menu trees unless necessary.

### 4. File Layout
Choose the minimal file tree required.

Typical options:

#### Native MIDI FX
- `module.json`
- `dsp.so`
- `dsp/<sources>`

#### Native MIDI FX with custom UI
- `module.json`
- `ui.js`
- `ui_chain.js`
- `dsp.so`
- `dsp/<sources>`

#### External module package
- same structure, packaged for deployment

### 5. State Strategy
Define whether the module needs:
- simple param recall only
- custom JSON state
- internal sequencer state
- held-note state exclusion
- transport-dependent reset rules

### 6. Chain Editing Strategy
If chain editing matters, define:
- what is exposed through `chain_params`
- which controls are safe in chain context
- which controls stay in full UI only

### 7. Move Interaction Strategy
Define:
- knobs 1-8
- shift modifiers
- menu/back behavior
- whether pads are functional or just decorative
- whether steps act as shortcuts, toggles, or mode selectors
- LED behavior

## Required Output Format

### Proposed File Tree
Provide the file tree.

### Proposed `module.json`
Write a complete draft manifest.

### Parameter Specification
For each parameter provide:
- key
- name
- type
- options or min/max
- default
- knob assignment

### Native Engine Decision
State:
- why native is needed or not needed
- what timing-sensitive behavior exists
- whether `tick()` is required

### State Strategy
Explain how parameters and state will be restored.

### UX Plan
Explain exactly how the user interacts with the module on Move.

## Guardrails
- Do not invent a UI hierarchy more complex than needed.
- Do not expose parameters that the engine cannot actually support.
- Do not use DAW-oriented metaphors that do not make sense on Move.
- Prefer an implementation that can be tested incrementally.