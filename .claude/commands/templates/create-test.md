---
description: Create native tests for a Schwung MIDI FX module
argument-hint: [module-name-and-behavior-summary]
---

Create native C tests for a Schwung MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions. Read `repo-bootstrap` if not already done.

Before writing tests, inspect:
- `src/dsp/<module>_engine.h` — engine API to test
- `src/host/<module>_plugin.c` — host wrapper functions to test
- `src/module.json` — parameter surface to cover

## Two Test Files Required

Every module has two test files:

### `tests/<module>_engine_test.c`
Tests the portable engine in isolation — no Schwung headers, no host API.

Covers:
- Init and defaults (all params at expected defaults after `engine_init`)
- Parameter setters and getters (set, then verify with getter)
- Core algorithm behavior (does the engine produce the expected output?)
- Edge cases: zero input, max input, boundary values
- State reset (engine_reset clears active state without stuck notes)
- If scales exist: at least one targeted test proving a scale-specific interval really changes the output

### `tests/<module>_midi_fx_test.c`
Tests the host wrapper and MIDI dispatch — uses `midi_fx_api_v1.h` and `plugin_api_v1.h`.

Covers:
- `create_instance` returns non-NULL with correct defaults
- `set_param` / `get_param` round-trip for every parameter in `module.json`
- `get_param` returns a positive value (not 0, not -1) for all known keys
- `process_midi` passes through unrecognized messages
- `process_midi` handles 0xFC (transport stop) — no stuck notes
- `save_state` / `load_state` round-trip — params survive a save/restore cycle
- Note lifecycle: note-off is sent for active notes on transport stop
- Parsing robustness for Move param formats:
  - raw ints
  - raw float-formatted ints
  - normalized float strings for int and enum params

## Test Harness Pattern

Use a minimal hand-rolled test harness — no external dependencies:

```c
/* tests/<module>_engine_test.c */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "../src/dsp/<module>_engine.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond) do { \
    if (cond) { passed++; } \
    else { fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); failed++; } \
} while(0)

int main(void) {
    /* --- test cases --- */

    printf("Engine tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
```

## `get_param` Return Value Test

This is the single most important test in the host wrapper suite:

```c
/* Every known param key must return > 0 from get_param */
static const char *param_keys[] = {
    "density", "steps", "mode", /* ... all keys from module.json ... */
    NULL
};

void test_get_param_returns_snprintf(void *inst) {
    char buf[64];
    for (int i = 0; param_keys[i]; i++) {
        int r = api->get_param(inst, param_keys[i], buf, sizeof(buf));
        CHECK(r > 0); /* must be snprintf result, not 0 or -1 */
    }
    /* unknown key must return -1 */
    CHECK(api->get_param(inst, "__unknown__", buf, sizeof(buf)) == -1);
}
```

## Move Param Parsing Test Pattern

When int or enum params are editable from the hardware, add round-trip cases for:

```c
api->set_param(inst, "root", "7");
api->set_param(inst, "root", "7.0000");
api->set_param(inst, "root", "0.5000");
api->set_param(inst, "scale", "phrygian");
api->set_param(inst, "scale", "8");
api->set_param(inst, "scale", "0.9000");
```

The exact expected values depend on the declared range and enum order in `module.json`.

## Scale Test Pattern

If the module exposes `scale` or `mode`, add both:

1. a wrapper round-trip test:
```c
api->set_param(inst, "scale", "phrygian");
api->get_param(inst, "scale", buf, sizeof(buf));
CHECK(strcmp(buf, "phrygian") == 0);
```

2. an engine behavior test proving one color note is real:
```c
mg_engine_set_scale(&e, MG_SCALE_AEOLIAN);
/* drive a deterministic transition and verify the minor third appears */
```

This catches the common failure mode where names are added in the manifest, but the engine interval table or wrapper mapping does not match.

## Note Lifecycle Test Pattern

```c
void test_no_stuck_notes_on_stop(void *inst) {
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];

    /* send transport stop */
    uint8_t stop[1] = { 0xFC };
    int n = api->process_midi(inst, stop, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* verify all outputs are note-offs (0x80..0x8F) or nothing */
    for (int i = 0; i < n; i++) {
        CHECK((out[i][0] & 0xF0) == 0x80); /* note-off */
    }
}
```

## Save/Load Round-Trip Test Pattern

```c
void test_save_load_roundtrip(void *inst) {
    char state[1024];

    /* set a non-default value */
    api->set_param(inst, "density", "0.75");

    /* save state */
    api->save_state(inst, state, sizeof(state));

    /* reset to default */
    api->set_param(inst, "density", "0.5");

    /* restore */
    api->load_state(inst, state, (int)strlen(state));

    /* confirm restored */
    char buf[32];
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK(fabs(atof(buf) - 0.75) < 0.01);
}
```

## Makefile Integration

After generating test files, update the Makefile automatically:
- `tests/<module>_engine_test.c` is picked up by `$(wildcard tests/*_test.c)`
- `tests/<module>_midi_fx_test.c` is picked up by the same wildcard
- No manual Makefile edits required for standard test files

Run tests:
```bash
make test
```

All tests must pass before building for hardware.

## Return Format

Return exactly:
1. A short test plan summary (what behaviors are covered and why)
2. The full `tests/<module>_engine_test.c` in a fenced `c` block
3. The full `tests/<module>_midi_fx_test.c` in a fenced `c` block
4. A short coverage review:
   - Which failure modes are caught by these tests
   - Which behaviors require hardware validation (cannot be tested natively)

Do not generate engine, host wrapper, or module.json files in this step.

## Guardrails
- Do not use external test frameworks — keep it portable and dependency-free
- Do not test transport-dependent timing natively — test timing logic in the engine, hardware behavior on Move
- Do not skip the `get_param` return value test — it catches the most common silent bug
- Do not serialize or test active note state in save/load tests — it must not be saved
- Write failing tests first when adding new behavior (they are the spec)
- If `chain_params` or signed int params changed, add explicit wrapper tests for those bounds and parsing forms
- If scale options changed, add explicit tests for enum order, wrapper parsing, and at least one scale-specific interval
