# Build Move UI and Controls for Schwung Module

## Purpose
Use this skill to design and implement the Move-facing control surface for a Schwung module.

This includes:
- parameter-to-knob mapping
- shift behaviors
- pad behaviors
- step button behaviors
- LED feedback
- optional `ui.js`
- optional `ui_chain.js`

## Goal
Make the module feel native on Move. The user should understand and control the module quickly with minimal cognitive load.

---

## UI Design Principles

**Keep the first screen simple.** Put the most important parameters on direct knobs. Shift is for secondary adjustments, not for hiding critical features.

**Prefer standard UI when possible.** A well-designed `module.json` with good knob assignment often eliminates the need for custom `ui.js`. Only build a custom UI when Move's native parameter browser genuinely cannot express the interaction.

**LEDs should convey state, not create noise.** One clear signal is worth more than ten blinking patterns.

**Pads and step buttons only if they add musical value.** Don't use them because they're there — use them when the interaction is immediate and obvious.

**Prefer clear macro/fine pairs over long flat parameter lists.** Generative modules often become easier to learn when paired controls each own one job:
- `range` + `spread`
- `density` + `rest`
- `chaos` + `resolve`

---

## Typical Knob Candidates (ordered by priority)

Direct knob access (no shift) — most important live controls:
- Primary amount, rate, or intensity
- Mode or character selector (enum)
- Musical timing (rate, division, tempo)
- Output target (note, channel, range)

For generative note modules, a strong default ordering is often:
- harmonic identity first (`root`, `scale`)
- register second (`range`, `spread`)
- phrase amount third (`density`)
- behavior next (`chaos`, `resolve`, `rest`)
- setup params (`steps`, `gate`, `vel`) in the full param list unless they are central to performance

Shift layer — secondary adjustments:
- Fine-tuning of primary controls
- Reset or randomize shortcuts
- Alternate mode or sub-parameter

---

## Design Process

### Step 1 — Audit the Parameter Surface
Read `src/module.json` — knobs list, params list, chain_params. Identify:
- Which parameters are musically critical (direct knob)
- Which are setup parameters (menu or shift)
- Which are too dangerous to expose in chain context
- Which parameter pairs are likely to be tweaked together in live use

### Step 2 — Define the Knob Map

| Knob | Parameter | Shift (if any) |
|---|---|---|
| 1 | most important | — |
| 2 | second most important | — |
| … | … | … |
| 8 | least frequent | — |

Maximum 8 direct knobs. Be ruthless — fewer is better for V1.

If there are more than 8 meaningful params:
- keep exploratory controls on the live knobs
- move setup controls to the full param list
- do not hide a constantly-used musical control behind shift

### Step 3 — Define Shift Behavior
Only add shift behavior when it genuinely improves usability. Each shift+knob pair should be immediately intuitive. Do not use shift to compensate for a parameter surface that is too large.

### Step 4 — Define Pad and Step Button Behavior (if any)
Pads work well for: mode toggles, mute/solo per lane, pattern selection.
Step buttons work well for: step toggles, value presets, shortcut navigation.

If pads and step buttons have no clear role, leave them unused. Do not invent interactions.

### Step 5 — Define LED Behavior
LEDs should reflect: active state, current mode, transport running/stopped, active lane.

Avoid: rapid flickering, decorative patterns, information overload.

### Step 6 — Decide: Standard UI or Custom `ui.js`

Use standard Schwung parameter UI (`module.json` only) when:
- The parameter surface is compact and well-ordered
- No custom visual feedback is needed
- No pad or step button interaction is required

Build custom `ui.js` only when:
- Move's native browser cannot express the required interaction
- Step-based editing is central to the module's use
- Pad interaction has clear musical value
- A visual display (active step, loop position, etc.) adds real usability

If custom `ui.js` is not justified, return `OMIT_UI_JS`.

### Step 7 — Decide: Chain UI or Not

Build `ui_chain.js` when:
- Chain-context editing needs a tighter, more focused surface than the full UI
- Only 2–4 params matter during chain navigation

If no custom chain UI is justified, return `OMIT_UI_CHAIN_JS`.

---

## Engine Warning Display Pattern

If the module depends on a user setting (e.g. "MIDI Clock Out" for transport sync), expose feedback via a virtual `get_param` key:

```c
if (strcmp(key, "sync_warn") == 0) {
    if (inst->sync_mode == SYNC_MOVE && g_host && g_host->get_clock_status) {
        int status = g_host->get_clock_status();
        if (status == MOVE_CLOCK_STATUS_UNAVAILABLE)
            return snprintf(buf, buf_len, "Enable MIDI Clock Out");
        if (status == MOVE_CLOCK_STATUS_STOPPED)
            return snprintf(buf, buf_len, "stopped");
    }
    return snprintf(buf, buf_len, "");
}
```

The UI can poll `sync_warn` and display it with a `!` prefix when non-empty. This surfaces configuration problems without hidden failures.

---

## Required Output Format

### Interaction Summary
One paragraph: how the user controls the module on Move.

### Knob Map (1–8)
Table: knob number / parameter / shift behavior.

### Shift Layer
List of shift+knob combinations, or "none".

### Pad Map
List of pad assignments, or "unused".

### Step Button Map
List of step button assignments, or "unused".

### LED Plan
What each LED or LED group communicates.

### UI File Decision
- Standard UI only (`OMIT_UI_JS`, `OMIT_UI_CHAIN_JS`) — with rationale
- OR: which files to create and why each is justified

### Implementation Notes
Any non-obvious patterns, warning displays, or edge cases in the interaction model.

---

## Guardrails
- Do not build `ui.js` just to have one. Standard UI is the right default.
- Do not use pads or step buttons without a clear musical reason.
- Do not add shift behaviors to hide an oversized parameter surface — reduce the surface instead.
- Do not let LED behavior distract from the music.
- Always align control keys with the exact keys in `module.json` and the engine.
- Keep V1 UI deliberately simple. Complexity can be added later.
- For generative modules, do not expose too many overlapping "probability" knobs — users should hear what each knob owns.
