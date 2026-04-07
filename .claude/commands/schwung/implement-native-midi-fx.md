# Implement Native Schwung MIDI FX

## Purpose
Use this skill to implement the native engine of a Schwung MIDI FX module.

This skill assumes the module should process MIDI at the engine layer and optionally generate timed events.

## Goal
Build a stable native MIDI FX implementation that:
- transforms or generates MIDI correctly
- exposes parameters cleanly
- behaves correctly in the Signal Chain
- is safe with note lifecycle and state restore
- can be edited from the UI and chain context

## Recommended Strategy
Follow the two-layer split used in this project:

1. **Portable engine** (`dsp/<module>_engine.h` + `dsp/<module>_engine.c`) — no Schwung/Move headers
2. **Host wrapper** (`host/<module>_plugin.c`) — Schwung plugin API, MIDI dispatch, param I/O

Inspect the existing module in this repo as the reference pattern before implementing.

Prefer a clear separation between:
- instance state
- MIDI input handling
- timed event generation
- parameter parsing
- state serialization

## Native Implementation Plan

### 1. Define Instance State
Create a per-instance struct that contains:
- current parameter values
- internal working state
- note tracking (`active_note[]`, `active_channel[]`)
- timing counters (`frames_until_step`, `frames_left` for scheduled events)
- clock sync state (`running`, `sync_mode`)
- any scheduled event queue

### 2. Define MIDI Behavior
Document exactly how the module responds to:
- note on / note off / velocity-0 note on
- CC
- MIDI clock (note: `0xFA/0xFB/0xFC` are NOT forwarded to external plugins)
- transport start/stop/continue
- pass-through of unsupported messages

Be explicit about which messages are consumed, transformed, or passed through.

### 3. Handle Timed Generation
If the module generates timed notes or CC:
- decide whether timing comes from Move transport (`get_bpm()`) or internal BPM
- use `tick()` for emitting events over time
- define reset rules on stop / mode change / parameter change

### 4. Implement Parameter I/O
Implement:
- `set_param(instance, key, value)` — validates, clamps, updates engine
- `get_param(instance, key, buf, len)` — **must return `snprintf(...)`, never `return 0`**
- `save_state()` / `load_state()` with key=value string format
- `chain_params` in `module.json` if chain editing matters

When implementing editable params for Move, assume `value` may arrive as:
- raw int strings such as `"7"` or `"-12"`
- raw float strings such as `"7.0000"` or `"64.0000"`
- normalized strings such as `"0.5000"`

For int params:
- support signed ranges when musically useful
- map normalized `0.0–1.0` values back into the declared min/max
- accept float-formatted raw ints without misclassifying them as normalized values

For enum params:
- accept enum names, raw numeric indices, and normalized float strings
- keep enum option order stable once exposed in the UI

### 5. Manage Note Lifecycle Safely
Always protect against:
- stuck notes on transport stop
- stuck notes on mode change
- stuck notes on state restore
- missing note-offs when held note state is cleared
- mode changes while notes are sounding

Pattern: always send note-off for active notes before resetting state.

### 6. Add Defensive Parsing
When parsing parameter strings or state:
- validate values before applying
- clamp numeric ranges
- handle missing keys gracefully
- `atof()` / `atoi()` for numeric params, explicit string comparison for enums
- never crash on malformed state

### 7. Separate Musical Jobs Cleanly
When a module grows beyond a trivial V1, prefer parameters that each do one clear job:
- overall occupancy vs silence character
- register size vs upward bias
- randomness vs return-to-home gravity

Good examples:
- `density` = how many steps speak
- `rest` = how silences cluster and breathe
- `range` = the register shape
- `spread` = how much the line leans upward inside that range
- `chaos` = instability
- `resolve` = attraction back to root

Avoid one overloaded parameter trying to do both macro amount and micro character.

## Transport Sync — Critical

### Clock Status
`get_clock_status()` returns `MOVE_CLOCK_STATUS_UNAVAILABLE` when "MIDI Clock Out" is not enabled in Move settings. Treat `UNAVAILABLE` the same as `STOPPED`.

Use the Brindille pattern in `tick()`:
```c
if (inst->sync_mode == 0 && g_host && g_host->get_clock_status) {
    int status = g_host->get_clock_status();
    if (status == MOVE_CLOCK_STATUS_STOPPED ||
        status == MOVE_CLOCK_STATUS_UNAVAILABLE) {
        inst->running = 0;
    } else if (status == MOVE_CLOCK_STATUS_RUNNING) {
        inst->running = 1;
    }
} else if (inst->sync_mode != 0) {
    inst->running = 1;
}
```

Initialize `running = 0` in `create_instance`. Never initialize to 1.

### Free-Running Clock
If the module does NOT need transport sync, do NOT call `get_clock_status()` or `get_bpm()` — this causes SIGSEGV on some Move firmware versions.

Use a constant:
```c
#define DEFAULT_BPM 120.0f
static float current_bpm(void) { return DEFAULT_BPM; }
// create_instance: inst->running = 1;
```

### sync_warn — user-visible feedback
If transport sync requires MIDI Clock Out, expose a `sync_warn` virtual param:
```c
if (strcmp(key, "sync_warn") == 0) {
    if (inst->sync_mode == 0 && g_host && g_host->get_clock_status) {
        int status = g_host->get_clock_status();
        if (status == MOVE_CLOCK_STATUS_UNAVAILABLE)
            return snprintf(buf, buf_len, "Enable MIDI Clock Out");
        if (status == MOVE_CLOCK_STATUS_STOPPED)
            return snprintf(buf, buf_len, "stopped");
    }
    return snprintf(buf, buf_len, "");
}
```

## Required Engineering Checklist
- [ ] All parameters have defaults in `create_instance`
- [ ] Note lifecycle is explicit and stuck notes are prevented
- [ ] Unsupported MIDI is passed through or intentionally filtered
- [ ] Clock behavior is explicit and documented
- [ ] Transport stop resets `running = 0`
- [ ] `get_clock_status()` treats UNAVAILABLE as not-running
- [ ] `sync_warn` or equivalent if transport sync requires user action
- [ ] State restore is deterministic
- [ ] `chain_params` matches real parameter support
- [ ] `chain_params` uses real parameter objects when chain editing is required
- [ ] `get_param` returns `snprintf(...)` for all params, `-1` for unknown
- [ ] Int and enum params parse raw, float-formatted, and normalized Move values
- [ ] Signed transposition params are reflected correctly in both code and `module.json`

## Required Output Format
When using this skill, produce:

### Engine Summary
One paragraph describing the runtime model.

### Instance State
Show the struct layout (both engine struct and plugin instance struct if split).

### MIDI Rules
List the exact MIDI message behavior.

### Functions to Implement
List the required functions and purpose of each.

### Edge Cases
List the failure cases and how they are handled.

### Code
Generate both the engine and host wrapper files.

### Self-Review
At the end, review the implementation for:
- stuck note risk
- invalid state risk
- timing drift risk
- pass-through correctness
- `get_param` return value correctness
- parameter/UI mismatch vs `module.json`

## Guardrails
- Do not emit notes without a reliable note-off strategy.
- Do not hide timing assumptions.
- Do not implement chain parameters that are not actually supported.
- Do not let malformed state crash the module.
- Do not call `get_clock_status()` or `get_bpm()` in free-running modules.
- Prefer boring reliability over clever abstractions.
