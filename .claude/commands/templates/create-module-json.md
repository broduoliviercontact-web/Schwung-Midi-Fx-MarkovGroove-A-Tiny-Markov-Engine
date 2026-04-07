---
description: Create or update a Schwung module.json manifest
argument-hint: [module-name-and-feature-summary]
---

Create a production-ready `module.json` for a Schwung MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions. Read `repo-bootstrap` if not already done.

Before drafting, inspect:
- `src/module.json` — the reference manifest for this project
- `src/host/midi_fx_api_v1.h` — capabilities supported by the host API

## Requirements

- Match conventions used in this project's existing `module.json`
- Default to a Signal Chain MIDI FX design unless clearly stated otherwise
- Prefer a compact, Move-friendly parameter surface
- Use a clean `capabilities` structure consistent with the existing module
- If chain editing matters, include `chain_params` matching real editable controls
- Use full parameter objects inside `chain_params`, not just key strings

## Module Identity Rules

```json
{
  "id": "lowercase-filesystem-safe",
  "name": "Display Name",
  "abbrev": "AB",
  "version": "0.1.0",
  "description": "One sentence.",
  "dsp": "dsp.so",
  "api_version": 1
}
```

## Capabilities Block

Standard MIDI FX:
```json
"capabilities": {
  "audio_out": false,
  "audio_in": false,
  "midi_in": true,
  "midi_out": true,
  "chainable": true,
  "component_type": "midi_fx",
  "ui_hierarchy": {
    "levels": {
      "root": {
        "name": "Module Name",
        "knobs": ["param1", "param2", "param3", "param4", "param5", "param6", "param7", "param8"],
        "params": ["param1", "param2", ...]
      }
    }
  }
}
```

## Parameter Rules

- Prefer 2–8 primary parameters for V1
- Use realistic types: `float`, `int`, `enum`, `toggle`
- Every parameter must have a sensible `default`
- Knob assignments must match the most important controls (ordered by musical priority)
- `chain_params` should be the live-editable subset — omit parameters that are unsafe to edit in chain context
- If an `int` parameter is musically signed, declare the real negative range explicitly
- Keep enum option order stable and intentional — normalized Move knob values depend on enum index
- For generative modules, prefer paired controls with distinct jobs instead of several overlapping probability params

## Scale / Mode Parameters

If the module exposes a melodic palette, prefer an explicit enum:

```json
{
  "key": "scale",
  "name": "Scale",
  "type": "enum",
  "options": [
    "ionian",
    "aeolian",
    "dorian",
    "mixolydian",
    "major_pent",
    "minor_pent",
    "suspended",
    "power"
  ],
  "default": "ionian"
}
```

Good expansion options when the design justifies them:
- `phrygian`
- `lydian`
- `harmonic_minor`
- `blues`

More advanced options for strongly flavored modules:
- `locrian`
- `melodic_minor`
- `whole_tone`
- `diminished`
- `phrygian_dominant`

Rules:
- keep the option order stable once released
- use the same names in `module.json`, wrapper parsing, save/load, and docs
- do not rename `mode` to `scale` or vice versa mid-project without handling compatibility

## Supported Types

```json
{ "key": "foo", "name": "Foo", "type": "float", "min": 0.0, "max": 1.0, "default": 0.5, "step": 0.01 }
{ "key": "bar", "name": "Bar", "type": "int", "min": 1, "max": 32, "default": 16, "step": 1 }
{ "key": "mode", "name": "Mode", "type": "enum", "options": ["a", "b", "c"] }
{ "key": "active", "name": "Active", "type": "toggle", "default": false }
```

`chain_params` should mirror the same schema shape:

```json
[
  { "key": "density", "name": "Density", "type": "float", "min": 0.0, "max": 1.0, "default": 0.7, "step": 0.01 }
]
```

## Decision Rules

- If the request is underspecified, infer a minimal usable V1 design and state your assumptions
- Do not invent unsupported manifest fields
- Do not add parameters that the engine will not actually support
- Keep V1 small and stable

## Return Format

Return exactly:
1. A short design summary
2. One fenced `json` block containing the full `module.json`
3. A short assumptions section listing any decisions made where the spec was ambiguous

Do not generate native C or UI files in this step.
