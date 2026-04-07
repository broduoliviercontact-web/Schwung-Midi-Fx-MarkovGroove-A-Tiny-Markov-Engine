# Schwung Module — Repo Bootstrap

## Purpose
Use this skill before designing, modifying, porting, or debugging any Schwung module in this project.

Establishes the minimum required reading, architectural assumptions, and API conventions before any implementation work begins.

## Required Reading

Before writing or changing code, read:
- `CLAUDE.md` — architecture rules, API gotchas, critical conventions
- `BUILDING.md` — how to build, test, and deploy
- `docs/MODULES.md` — full module API reference (manifest, plugin interface, host API)
- `docs/PROJECT_PLAN.md` — scope, delivery stages, explicit non-goals
- `docs/PORTING_NOTES.md` — per-module design decisions and Schwung/Move principles
- `src/module.json` — parameter surface and manifest conventions for the current module

Then inspect the implementation split:
- `src/dsp/<module>_engine.h` — portable engine API (no Schwung/Move headers)
- `src/dsp/<module>_engine.c` — portable engine implementation
- `src/host/<module>_plugin.c` — Schwung host wrapper: MIDI dispatch, param I/O, state

Also read the host API contracts:
- `src/host/midi_fx_api_v1.h` — Schwung MIDI FX plugin interface
- `src/host/plugin_api_v1.h` — host API struct (`host_api_v1_t`, `get_clock_status`, `get_bpm`)

Finally, look at the tests:
- `tests/<module>_engine_test.c` — portable engine correctness
- `tests/<module>_midi_fx_test.c` — host wrapper and MIDI dispatch behavior

## Architecture

This project follows a strict two-layer split:

```
src/dsp/<module>_engine.h / _engine.c
  └── Portable engine. No Move, no Schwung headers. Pure C.
      Per-instance struct, init/reset, set_*/get_*, tick().

src/host/<module>_plugin.c
  └── Schwung host wrapper. Owns per-instance state (wraps engine).
      Implements the plugin_api_v1 and midi_fx_api_v1 interfaces.
      Handles MIDI dispatch, parameter parsing, state serialization.
```

Do NOT mix engine logic into the host wrapper.
Do NOT include Schwung or Move headers in the engine layer.

## Bootstrap Checklist

Work through this before writing any code:

1. Read the required docs above.
2. Inspect the existing module (`src/`) as the reference implementation.
3. Confirm whether the target is:
   - extending the existing module
   - a new independent module in the same repo
4. Confirm the implementation model:
   - native MIDI FX only (standard Schwung UI via `module.json`)
   - native MIDI FX + custom `ui.js` (full Move UI)
   - native MIDI FX + `ui.js` + `ui_chain.js` (chain-context editing too)
5. Confirm the clock strategy:
   - Move transport sync (`get_clock_status()` + `get_bpm()`)
   - internal BPM (user-controlled, no clock API)
   - free-running (constant BPM — must NOT call `get_clock_status()` or `get_bpm()`)
6. Write the implementation brief below before coding.

## Implementation Brief

Produce this before any code changes:

### Module Summary
- Module name:
- Module id:
- Component type: `midi_fx`
- Native DSP required:
- Custom `ui.js` required:
- Custom `ui_chain.js` required:
- Clock strategy:

### Functional Goal
Describe exactly what the module does to incoming and outgoing MIDI.

### User Controls
List every exposed parameter and its type:
- `float` — 0.0 to 1.0
- `int` — with min/max range
- `enum` — with option list
- `toggle` — boolean

### Move Mapping
Describe:
- Knob assignments (up to 8)
- Shift modifier behavior (if any)
- Pad behavior (if any)
- Step button behavior (if any)
- Menu/back navigation structure
- LED plan (informative only — not decorative)

### Technical Plan
Describe:
- Source files to create or modify
- State persistence approach (`save_state` / `load_state` format)
- Note lifecycle strategy (when note-offs are guaranteed)
- Test plan (what native tests will cover the new behavior)

## API Gotchas — Critical

### `get_param` return value
`get_param` must return `snprintf(buf, buf_len, ...)` — never `return 0` for a valid param.

```c
// WRONG — host silently ignores the value:
if (strcmp(key, "foo") == 0) { snprintf(buf, buf_len, "%d", val); return 0; }

// CORRECT:
if (strcmp(key, "foo") == 0) return snprintf(buf, buf_len, "%d", val);

// Unknown param:
return -1;
```

Returning `0` silently breaks param display, chain editing, and state recall.

### Clock sync
- `get_clock_status()` returns `MOVE_CLOCK_STATUS_UNAVAILABLE` when "MIDI Clock Out" is not enabled in Move settings
- Treat `UNAVAILABLE` the same as `STOPPED`
- Initialize `clock_running = 0` in `create_instance` — never 1
- Free-running modules must NOT call `get_clock_status()` or `get_bpm()` — causes SIGSEGV on some firmware
- MIDI transport messages `0xFA/0xFB/0xFC` are NOT forwarded to external plugins

### Parameter encoding
- Float params: clamp 0.0–1.0, scale to `uint8_t` (×255) for the engine, unscale for `get_param`
- Enum params: validate against known values, fall through to default on unknown
- Int params: clamp to min/max explicitly
- All params need: default in `create_instance`, `set_param` handler, `get_param` handler, entry in `module.json`

## Install Path

```
/data/UserData/schwung/modules/midi_fx/<module_id>/
```

Restart Schwung after install:
```bash
ssh root@move.local 'cd /data/UserData/schwung && nohup ./start.sh >/tmp/start_schwung.log 2>&1 </dev/null &'
```

## Guardrails

- Do not start coding before completing the implementation brief.
- Do not assume audio output is available in a `midi_fx` slot.
- Do not mix engine logic into the host wrapper.
- Do not call `get_clock_status()` or `get_bpm()` in free-running modules.
- Do not design desktop-style UI patterns that don't translate to Move hardware.
- Prefer consistency with the existing module over theoretical elegance.
- Test natively before deploying to hardware.

## Output Contract

When this skill is used, produce:
1. A brief repo-aware summary (one paragraph)
2. The completed implementation brief
3. A proposed file tree
4. A list of files to inspect or create next
