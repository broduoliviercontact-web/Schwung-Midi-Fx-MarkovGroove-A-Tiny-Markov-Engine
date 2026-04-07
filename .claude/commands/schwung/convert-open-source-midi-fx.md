# Convert Open-Source MIDI FX to Schwung Module

## Purpose
Use this skill to perform the full end-to-end conversion of an open-source MIDI FX project into a Schwung module.

This is the orchestration skill and the coherence reviewer. Use it to either drive a full conversion OR to review alignment between existing files.

## Inputs
You may receive:
- a GitHub repo URL or source files
- a README or plugin bundle
- an earlier audit
- a user request describing desired behavior
- existing partial implementation files to review

## Goal
Deliver a clean Schwung-native module, not a shallow wrapper.

The result should feel like it was designed for Move, not ported from a desktop plugin. Prefer a solid V1 over an ambitious unstable clone.

---

## Workflow

Run these phases in order. Do not skip.

### Phase 1 — Audit
Use `audit-open-source-midi-fx`.

Produce: feasibility rating, source summary, conversion strategy (keep/rewrite/discard/simplify), portability class, proposed architecture, Move UX sketch, risk list.

If feasibility is `Poor fit`, stop and explain why. Do not proceed.

### Phase 2 — Design
Use `design-module`.

Produce: module identity, `module.json` draft, parameter spec, engine architecture decision, clock strategy, state strategy, Move UX plan.

Verify: the design covers the full conversion scope from the audit. No parameter invented without engine support.

### Phase 3 — Engine
Use the `create-dsp-c` template.

Produce: `dsp/<module>_engine.h` and `dsp/<module>_engine.c`.

Requirements:
- Portable — no Schwung or Move headers
- Testable without hardware (`make test`)
- Separated strictly from the host wrapper
- Note lifecycle explicit and safe

### Phase 4 — Host Wrapper
Use the `create-host-wrapper` template.

Produce: `host/<module>_plugin.c`.

Requirements:
- All parameters from `module.json` implemented in `set_param` and `get_param`
- `get_param` returns `snprintf(...)` — never `return 0`
- `create_instance` initializes all defaults, `running = 0`
- Note lifecycle flushed on transport stop and mode changes

### Phase 5 — State and Chain Integration
Verify and finalize:
- `save_state` / `load_state` covers all playback-affecting parameters
- Transient state (active notes, scheduled events) is NOT serialized
- `chain_params` in `module.json` matches what the engine actually supports
- `sync_warn` virtual param implemented if transport sync requires user action

### Phase 6 — Validation (Coherence Review)
Before building for hardware, check cross-file alignment:

**manifest vs engine:**
- [ ] Every parameter in `module.json` has a `set_param` handler
- [ ] Every parameter in `module.json` has a `get_param` handler with correct return
- [ ] All `chain_params` keys match actual engine support
- [ ] `chain_params` uses full parameter objects when chain editing is expected
- [ ] Signed int ranges in `module.json` match engine clamp behavior
- [ ] Enum option order matches the wrapper's normalized parsing logic

**Note lifecycle:**
- [ ] Note-on always paired with a future note-off
- [ ] Active notes flushed on transport stop (`0xFC`)
- [ ] Active notes flushed on mode change while notes are held
- [ ] Active notes flushed on state reset

**Timing:**
- [ ] `tick()` handles `frames = 0` gracefully
- [ ] Step accumulator doesn't overflow on long sessions
- [ ] Clock strategy is consistent (no mixed `get_clock_status()` + constant BPM)

**MIDI pass-through:**
- [ ] Unrecognized messages are forwarded, not silently dropped
- [ ] `max_out` limit is respected — never exceed it

**State restore:**
- [ ] All params saved in `save_state` are loaded in `load_state`
- [ ] Missing keys use defaults — no crash on incomplete state
- [ ] Restoring state does not leave active notes
- [ ] Parameters survive both raw and normalized-value recall paths

**Move UX:**
- [ ] Knob keys match actual parameter keys
- [ ] No parameter key referenced in `ui.js` or `ui_chain.js` that doesn't exist in the engine

### Phase 7 — Manifest Finalization
Use `create-module-json` to finalize `module.json` against the implemented parameter surface.

Confirm: no parameter listed that the engine cannot support. V1 is intentionally small.

### Phase 8 — UI (if justified)
Use `build-move-ui-and-controls`.

Produce: knob map, shift plan, pad/step plan, LED plan, decision on `ui.js` and `ui_chain.js`.

Default: standard Schwung UI only. `OMIT_UI_JS` is a valid and often correct result.

### Phase 9 — Build and Hardware Validation
Use `build-and-install`.

```bash
make test
./scripts/build.sh
./scripts/install.sh
```

Follow `docs/HARDWARE_TESTING.md`.

Confirm:
- Module appears in MIDI FX slot
- All parameters display and edit correctly
- Int and enum params edit correctly from both normal UI and chain context
- MIDI output starts and stops cleanly with transport
- No stuck notes after Stop, mode change, or module removal
- State recalls correctly after session reload

---

## Using This Skill as a Coherence Reviewer

When files already exist, use this skill to check alignment rather than regenerate everything:

> Review the coherence of the current module design.
> Check alignment between module.json, native engine, and UI strategy.
> Focus on mismatches, stuck note risk, and Move UX clarity.

What to detect:
- Parameter in `module.json` absent from `set_param` or `get_param`
- Engine state that cannot be edited from the UI
- UI key that doesn't match any engine parameter
- `chain_params` referencing unsupported functionality
- UX complexity that doesn't serve the music

---

## Required Output Format

When used as orchestrator:
1. Source summary and port scope
2. Schwung design (file tree, manifest, parameters, UI decision)
3. Implementation plan with phase checklist
4. Risk list
5. Code (when explicitly requested per phase)

When used as reviewer:
1. Coherence report — pass / warn / fail per section
2. Specific mismatches found
3. Recommended fixes in priority order

---

## Guardrails
- Do not skip the audit — licensing and feasibility must be checked first.
- Do not implement audio output in a `midi_fx` module.
- Do not preserve features that don't make sense on Move hardware.
- Do not generate placeholder pseudocode — produce compilable C.
- Do not chase feature parity at the expense of usability.
- Prefer a solid V1 over an ambitious unstable clone.
- Test natively before touching hardware.
