---
description: Create a compact ui_chain.js for a Schwung Signal Chain module
argument-hint: [module-name-and-chain-editing-behavior]
---

Create a production-ready `ui_chain.js` for a Schwung module based on this request:

$ARGUMENTS

Follow project memory in @.claude/CLAUDE.md.

Before writing code, inspect:
- `docs/MODULES.md`
- `docs/API.md`
- at least one existing chain-aware module or chain UI pattern in the repo
- the target module’s `module.json`
- the target module’s editable parameter surface

`ui_chain.js` must set `globalThis.chain_ui` and must NOT override `globalThis.init` or `globalThis.tick`:

```javascript
globalThis.chain_ui = { init, tick, onMidiMessageInternal, onMidiMessageExternal };
```

Key facts:
- Chain UI is entered when a patch loads this module as its MIDI source. Back exits to chain view; Menu re-enters.
- Available host calls: `host_module_get_param(key)`, `host_module_set_param(key, val)`
- Knobs 1–8: CC 71–78 (relative). Jog: CC 14. Jog click: CC 3. Shift: CC 49. Back: CC 51.
- Display: 128×64, 1-bit. `print()`, `fill_rect()`, `clear_screen()`.
- LED buffer: max ~60 commands/frame. Use progressive init if setting many LEDs at once.

Goal:
Create a compact chain-editing UI for Signal Chain use.

Design principles:
- Chain UI must be smaller and simpler than full module UI.
- Expose only the controls that matter during chain editing.
- Prefer a minimal, high-confidence editing surface.
- Avoid interactions that depend on deep full-screen context unless clearly supported.
- Keep parameter names and behavior aligned with manifest keys and engine support.
- Prefer direct editing over visual complexity.

Behavior requirements:
- Focus on the module’s most important live-edit parameters.
- Respect the real parameter model.
- Use a limited control surface suitable for chain context.
- Keep navigation shallow.
- If the module exposes `chain_params`, the UI must match them.
- If chain editing does not need a custom JS layer, say so clearly instead of inventing unnecessary UI complexity.

Implementation rules:
- Match repo conventions.
- Keep the code compact and readable.
- Do not duplicate the full-screen UI unless that is clearly the established repo pattern.
- Do not expose controls that the engine cannot edit safely in chain context.
- If no custom chain UI is justified, return `OMIT_UI_CHAIN_JS`.

When a custom chain UI is justified, include:
- chain-context initialization
- parameter display/edit logic
- minimal navigation behavior
- clear control mapping
- cleanup/reset behavior if needed

Return exactly one of these two outcomes:

Outcome A:
1. A short chain UI summary
2. One fenced `javascript` block containing the full `ui_chain.js`
3. A short explanation of what is exposed in chain mode and why

Outcome B:
1. A short explanation of why a custom `ui_chain.js` is not justified
2. The exact line: `OMIT_UI_CHAIN_JS`

Do not generate `module.json`, native C, or full `ui.js` in this step.