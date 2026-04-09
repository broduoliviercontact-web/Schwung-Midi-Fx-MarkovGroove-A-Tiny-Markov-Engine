---
description: Create a Schwung module.json manifest for a MIDI FX module
argument-hint: [module-name-and-feature-summary]
---

Create a production-ready `module.json` for a Schwung MIDI FX module based on this request:

$ARGUMENTS

Follow project memory in @.claude/CLAUDE.md.

Before drafting, inspect:
- `docs/MODULES.md`
- `docs/API.md`
- at least one existing module in `src/modules/midi_fx/`

Requirements:
- Match real Schwung repo conventions.
- Default to a Signal Chain MIDI FX design unless the request clearly says otherwise.
- Prefer a compact, Move-friendly parameter surface.
- Use a clean `capabilities` structure that matches existing repo patterns.
- If chain editing is relevant, include `ui_hierarchy.levels.root` with:
  - `name`
  - `params`
  - `knobs`
- Choose sensible values for:
  - `id`
  - `name`
  - `abbrev`
  - `version`
  - `builtin`
- Do not invent unsupported manifest fields.
- Do not add parameters that the engine will not really support.
- Keep the first version intentionally small and stable.

Parameter rules:
- Prefer 2 to 8 primary parameters.
- Use realistic types such as:
  - `enum`
  - `int`
  - `float`
  - `toggle`
- Every parameter must include a sensible default.
- Knob assignments must match the most important controls.

Decision rules:
- If this should be an external module, do not mark it as built-in unless explicitly requested.
- If the request is underspecified, infer a minimal usable V1 design and state your assumptions.

Return exactly:
1. A short design summary
2. One fenced `json` block containing the full `module.json`
3. A short assumptions section

Do not generate any other files in this step.