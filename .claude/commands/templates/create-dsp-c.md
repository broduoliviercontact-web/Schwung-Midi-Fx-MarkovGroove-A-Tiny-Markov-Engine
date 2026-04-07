---
description: Create the portable engine layer for a Schwung MIDI FX module
argument-hint: [module-name-and-behavior]
---

Create the portable C engine for a Schwung MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions established in this repo. Read `repo-bootstrap` if not already done.

Before writing code, inspect:
- `src/dsp/` — inspect the existing engine header/implementation pattern in this repo
- `src/host/` — inspect how the existing engine is driven by the host wrapper
- `tests/` — inspect the existing engine test pattern in this repo

## Engine Layer Rules

The engine (`dsp/<module>_engine.h` + `dsp/<module>_engine.c`) must:
- Have NO dependency on Schwung, Move, or any platform-specific headers
- Be testable with `make test` on any machine
- Define a clean per-instance state struct
- Expose explicit `init`, `reset`, `tick`, `set_*`, `get_*` functions

## Parameter Representation

Float params use `uint8_t` scaled 0–255 internally:
```c
// set from host (0.0–1.0 string):
uint8_t val = (uint8_t)(atof(str) * 255.0f + 0.5f);
engine_set_foo(&inst->engine, val);

// get for host:
snprintf(buf, len, "%.2f", inst->engine.foo / 255.0f);
```

## Implementation Requirements

State struct:
- One struct per instance (no global state)
- Explicit defaults for every field
- Separate: current state / memory / output results

Separate clearly:
- instance state
- input handling
- timed generation (if needed)
- parameter setters/getters
- state serialization

Note lifecycle:
- Track active notes per lane/channel
- Always send note-off before resetting or clearing state
- Handle: mode changes, transport stop, empty state, state restore

Clock (if time-based):
- Make the timing model explicit
- If using Move transport: use the Brindille pattern with `get_clock_status()`
- If free-running: use a constant BPM — do NOT call `get_clock_status()` or `get_bpm()`
- `MOVE_CLOCK_STATUS_UNAVAILABLE` must be treated the same as `STOPPED`
- Initialize `clock_running = 0` — never 1

MIDI pass-through:
- Define which messages are consumed, transformed, or forwarded
- Unsupported messages should be passed through unless there is a reason to drop them

Coding rules:
- Prefer boring reliability over cleverness
- Avoid hidden behavior and fragile parsing
- Handle malformed state gracefully
- Keep comments focused and useful
- Generate real compilable C, not pseudocode

## Scale Handling Pattern

If the module is melodic, encode scales in the engine, not in the UI layer.

Recommended pattern:
- store `scale` as a small enum index
- keep one interval table in the engine
- derive generated notes from:
  - base note or `root`
  - selected scale interval
  - optional range/register offset

Example starter interval families for 4-state melodic generators:
- `ionian`: `{0, 4, 7, 12}`
- `aeolian`: `{0, 3, 7, 12}`
- `dorian`: `{0, 3, 7, 10}`
- `mixolydian`: `{0, 4, 7, 10}`
- `major_pent`: `{0, 4, 7, 14}`
- `minor_pent`: `{0, 3, 7, 10}`
- `suspended`: `{0, 5, 7, 12}`
- `power`: `{0, 7, 12, 19}`

Useful expansion set:
- `phrygian`: `{0, 1, 7, 10}`
- `lydian`: `{0, 4, 6, 11}`
- `harmonic_minor`: `{0, 3, 7, 11}`
- `blues`: `{0, 3, 6, 10}`

Advanced flavor set:
- `locrian`: `{0, 1, 6, 10}`
- `melodic_minor`: `{0, 3, 7, 11}`
- `whole_tone`: `{0, 4, 8, 10}`
- `diminished`: `{0, 3, 6, 9}`
- `phrygian_dominant`: `{0, 1, 4, 10}`

Rules:
- keep scale order stable once the wrapper and manifest expose it
- clamp unknown scale indices to the last valid scale or a documented default
- do not hardcode scale names only in comments; keep the enum, interval table, and wrapper names aligned

## Return Format

Return exactly:
1. A short engine summary (one paragraph)
2. The full `dsp/<module>_engine.h` header in a fenced `c` block
3. The full `dsp/<module>_engine.c` implementation in a fenced `c` block
4. A short edge-case review covering:
   - stuck note risk
   - invalid state risk
   - timing risk
   - pass-through behavior

Do not generate `module.json`, `host/<module>_plugin.c`, `ui.js`, or `ui_chain.js` in this step.
