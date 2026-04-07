/*
 * markov_groove_engine_test.c — portable engine tests
 *
 * Tests the engine in complete isolation: no Schwung headers, no host API.
 * Build: gcc -std=c99 -Wall -Wextra -Werror -Isrc/dsp -Isrc/host \
 *             tests/markov_groove_engine_test.c src/dsp/markov_groove_engine.c \
 *             -o build/tests/markov_groove_engine_test
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "../src/dsp/markov_groove_engine.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond) do { \
    if (cond) { passed++; } \
    else { \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        failed++; \
    } \
} while(0)

#define CHECK_FLOAT_EQ(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) < (double)(tol))

/* Helpers */
#define SAMPLE_RATE  44100
#define BPM          120.0f

/* frames_per_step at 120 BPM, 16th notes, 44100 Hz = 5512.5 */
#define FRAMES_PER_16TH  5513

static int tick_n(mg_engine_t *e, int nframes,
                  uint8_t out[][3], int lens[], int max_out)
{
    return mg_engine_tick(e, nframes, BPM, SAMPLE_RATE, out, lens, max_out);
}

/* =========================================================================
 * Init and defaults
 * ========================================================================= */
static void test_init_defaults(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    CHECK(mg_engine_get_root(&e)      == 0);
    CHECK(mg_engine_get_steps_idx(&e) == MG_STEPS_16);
    CHECK(mg_engine_get_scale(&e)     == MG_SCALE_IONIAN);
    CHECK(mg_engine_get_range(&e)     == MG_RANGE_CLOSE);
    CHECK_FLOAT_EQ(mg_engine_get_spread(&e), 0.0f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_density(&e), 1.0f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 0.3f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_rest(&e),  0.0f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_resolve(&e),  0.0f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_swing(&e), 0.0f, 0.01f);
    CHECK_FLOAT_EQ(mg_engine_get_gate(&e),  0.5f, 0.01f);
    CHECK(mg_engine_get_vel(&e) == 90);

    CHECK(e.mk_state    == MG_STATE_ROOT);
    CHECK(e.active_on   == 0);
    CHECK(e.active_note == MG_NO_NOTE);
    CHECK(e.swing_pending == 0);  /* running lives in wrapper, not engine */
    CHECK(e.rest_streak == 0);
    CHECK(e.rng != 0);               /* seed must never be 0 */
}

/* =========================================================================
 * Parameter setters and getters — clamp and round-trip
 * ========================================================================= */
static void test_param_root(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_root(&e, 5);
    CHECK(mg_engine_get_root(&e) == 5);

    mg_engine_set_root(&e, 11);
    CHECK(mg_engine_get_root(&e) == 11);

    /* Clamp low */
    mg_engine_set_root(&e, -5);
    CHECK(mg_engine_get_root(&e) == -5);

    /* Clamp high */
    mg_engine_set_root(&e, -99);
    CHECK(mg_engine_get_root(&e) == -24);

    mg_engine_set_root(&e, 99);
    CHECK(mg_engine_get_root(&e) == 24);
}

static void test_param_steps(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_steps(&e, MG_STEPS_4);
    CHECK(mg_engine_get_steps_idx(&e) == MG_STEPS_4);

    mg_engine_set_steps(&e, MG_STEPS_8);
    CHECK(mg_engine_get_steps_idx(&e) == MG_STEPS_8);

    mg_engine_set_steps(&e, MG_STEPS_16);
    CHECK(mg_engine_get_steps_idx(&e) == MG_STEPS_16);

    /* Clamp */
    mg_engine_set_steps(&e, 99);
    CHECK(mg_engine_get_steps_idx(&e) == 2);
}

static void test_param_scale(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_scale(&e, MG_SCALE_AEOLIAN);
    CHECK(mg_engine_get_scale(&e) == MG_SCALE_AEOLIAN);

    mg_engine_set_scale(&e, MG_SCALE_SUSPENDED);
    CHECK(mg_engine_get_scale(&e) == MG_SCALE_SUSPENDED);

    mg_engine_set_scale(&e, MG_SCALE_HARM_MINOR);
    CHECK(mg_engine_get_scale(&e) == MG_SCALE_HARM_MINOR);

    mg_engine_set_scale(&e, 99);
    CHECK(mg_engine_get_scale(&e) == MG_SCALE_BLUES);
}

static void test_param_range(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_range(&e, MG_RANGE_OCTAVE);
    CHECK(mg_engine_get_range(&e) == MG_RANGE_OCTAVE);

    mg_engine_set_range(&e, 99);
    CHECK(mg_engine_get_range(&e) == MG_RANGE_WIDE);
}

static void test_param_spread(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_spread(&e, 0.5f);
    CHECK_FLOAT_EQ(mg_engine_get_spread(&e), 0.5f, 0.01f);

    mg_engine_set_spread(&e, 2.0f);
    CHECK_FLOAT_EQ(mg_engine_get_spread(&e), 1.0f, 0.01f);
}

static void test_param_density(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_density(&e, 0.5f);
    CHECK_FLOAT_EQ(mg_engine_get_density(&e), 0.5f, 0.01f);

    mg_engine_set_density(&e, -1.0f);
    CHECK_FLOAT_EQ(mg_engine_get_density(&e), 0.0f, 0.01f);
}

static void test_param_chaos(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_chaos(&e, 0.0f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 0.0f, 0.01f);

    mg_engine_set_chaos(&e, 1.0f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 1.0f, 0.01f);

    mg_engine_set_chaos(&e, 0.75f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 0.75f, 0.01f);

    /* Clamp */
    mg_engine_set_chaos(&e, -0.5f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 0.0f, 0.01f);

    mg_engine_set_chaos(&e, 1.5f);
    CHECK_FLOAT_EQ(mg_engine_get_chaos(&e), 1.0f, 0.01f);
}

static void test_param_swing(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_swing(&e, 0.5f);
    CHECK_FLOAT_EQ(mg_engine_get_swing(&e), 0.5f, 0.01f);

    mg_engine_set_swing(&e, 1.0f);
    CHECK_FLOAT_EQ(mg_engine_get_swing(&e), 1.0f, 0.01f);

    /* Clamp */
    mg_engine_set_swing(&e, 2.0f);
    CHECK_FLOAT_EQ(mg_engine_get_swing(&e), 1.0f, 0.01f);
}

static void test_param_rest(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_rest(&e, 0.5f);
    CHECK_FLOAT_EQ(mg_engine_get_rest(&e), 0.5f, 0.01f);

    mg_engine_set_rest(&e, 2.0f);
    CHECK_FLOAT_EQ(mg_engine_get_rest(&e), 1.0f, 0.01f);
}

static void test_param_resolve(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_resolve(&e, 0.75f);
    CHECK_FLOAT_EQ(mg_engine_get_resolve(&e), 0.75f, 0.01f);

    mg_engine_set_resolve(&e, -1.0f);
    CHECK_FLOAT_EQ(mg_engine_get_resolve(&e), 0.0f, 0.01f);
}

static void test_param_gate(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_gate(&e, 0.8f);
    CHECK_FLOAT_EQ(mg_engine_get_gate(&e), 0.8f, 0.01f);

    mg_engine_set_gate(&e, 0.0f);
    CHECK_FLOAT_EQ(mg_engine_get_gate(&e), 0.0f, 0.01f);
}

static void test_param_vel(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    mg_engine_set_vel(&e, 64);
    CHECK(mg_engine_get_vel(&e) == 64);

    mg_engine_set_vel(&e, 127);
    CHECK(mg_engine_get_vel(&e) == 127);

    /* Clamp low */
    mg_engine_set_vel(&e, 0);
    CHECK(mg_engine_get_vel(&e) == 20);

    /* Clamp high */
    mg_engine_set_vel(&e, 200);
    CHECK(mg_engine_get_vel(&e) == 127);
}

/* =========================================================================
 * Tick: step boundary produces a note-on
 * ========================================================================= */
static void test_tick_emits_note_on(void)
{
    mg_engine_t e;
    mg_engine_init(&e);

    uint8_t out[16][3];
    int     lens[16];
    int n = 0;

    /* Tick just under one step — nothing yet */
    n = tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    CHECK(n == 0);

    /* Tick past the step boundary */
    n = tick_n(&e, 20, out, lens, 16);
    CHECK(n >= 1);

    /* First message must be a note-on (0x90) on channel 0 */
    int found_note_on = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90 && lens[i] == 3) {
            found_note_on = 1;
            /* Velocity must be our default (90) for even steps */
            CHECK(out[i][2] == 90);
        }
    }
    CHECK(found_note_on);
}

/* =========================================================================
 * Tick: generated notes are in valid MIDI range for default scale/range
 * ========================================================================= */
static void test_tick_note_range(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_chaos(&e, 1.0f);   /* enable full range to see all states */

    uint8_t out[16][3];
    int     lens[16];

    /* Run 40 steps, collect all note-on pitches */
    for (int step = 0; step < 40; step++) {
        int n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x90) {
                uint8_t pitch = out[i][1];
                CHECK(pitch >= 60);
                CHECK(pitch <= 72);
            }
        }
    }
}

/* =========================================================================
 * Tick: root offset shifts all generated pitches correctly
 * ========================================================================= */
static void test_tick_root_offset(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 5);   /* F — all notes shift by +5 semitones */
    mg_engine_set_chaos(&e, 0.0f);  /* no chaos → stays in root/3rd/5th */

    uint8_t out[16][3];
    int     lens[16];

    /* Get the first even step (immediate note-on) */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            /* All pitches must be >= 65 (60+5) */
            CHECK(out[i][1] >= 65);
            found = 1;
        }
    }
    CHECK(found);
}

/* =========================================================================
 * Tick: negative root offset transposes downward correctly
 * ========================================================================= */
static void test_tick_negative_root_offset(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, -24);
    mg_engine_set_chaos(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][1] >= 36);
            CHECK(out[i][1] <= 48);
            found = 1;
        }
    }
    CHECK(found);
}

/* =========================================================================
 * Scale: aeolian shifts the colour note down a semitone
 * ========================================================================= */
static void test_scale_aeolian_uses_flat_third(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_scale(&e, MG_SCALE_AEOLIAN);
    e.mk_state = MG_STATE_ROOT;
    e.rng = 3u; /* deterministic roll that selects state 1 with current weights */

    uint8_t out[16][3];
    int     lens[16];

    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][1] == 63);
            found = 1;
        }
    }
    CHECK(found);
}

/* =========================================================================
 * Scale: phrygian exposes the flat second as its colour note
 * ========================================================================= */
static void test_scale_phrygian_uses_flat_second(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_scale(&e, MG_SCALE_PHRYGIAN);
    e.mk_state = MG_STATE_ROOT;
    e.rng = 3u; /* deterministic roll that selects state 1 with current weights */

    uint8_t out[16][3];
    int     lens[16];

    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][1] == 61);
            found = 1;
        }
    }
    CHECK(found);
}

/* =========================================================================
 * Range: wide mode can raise the upper states by an octave
 * ========================================================================= */
static void test_range_wide_expands_register(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_scale(&e, MG_SCALE_IONIAN);
    mg_engine_set_range(&e, MG_RANGE_WIDE);
    e.mk_state = MG_STATE_ROOT;
    e.rng = 0u; /* deterministic roll that selects state 2 with current weights */

    uint8_t out[16][3];
    int     lens[16];

    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][1] == 79);
            found = 1;
        }
    }
    CHECK(found);
}

/* =========================================================================
 * Spread: high spread should favour upper-register notes in wide range
 * ========================================================================= */
static void test_spread_biases_upper_register(void)
{
    uint8_t out[16][3];
    int     lens[16];
    int hi_lo = 0;
    int hi_hi = 0;

    mg_engine_t narrow;
    mg_engine_init(&narrow);
    mg_engine_set_root(&narrow, 0);
    mg_engine_set_scale(&narrow, MG_SCALE_IONIAN);
    mg_engine_set_range(&narrow, MG_RANGE_WIDE);
    mg_engine_set_spread(&narrow, 0.0f);
    mg_engine_set_chaos(&narrow, 1.0f);
    mg_engine_set_swing(&narrow, 0.0f);

    mg_engine_t spread;
    mg_engine_init(&spread);
    mg_engine_set_root(&spread, 0);
    mg_engine_set_scale(&spread, MG_SCALE_IONIAN);
    mg_engine_set_range(&spread, MG_RANGE_WIDE);
    mg_engine_set_spread(&spread, 1.0f);
    mg_engine_set_chaos(&spread, 1.0f);
    mg_engine_set_swing(&spread, 0.0f);

    for (int step = 0; step < 200; step++) {
        int n_lo = tick_n(&narrow, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_lo; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] >= 79)
                hi_lo++;
        }

        int n_hi = tick_n(&spread, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_hi; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] >= 79)
                hi_hi++;
        }
    }

    CHECK(hi_hi > hi_lo);
}

/* =========================================================================
 * Markov: all 4 states reachable at maximum chaos
 * ========================================================================= */
static void test_markov_all_states_reachable(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_chaos(&e, 1.0f);
    mg_engine_set_swing(&e, 0.0f);  /* disable swing to see all notes immediately */

    uint8_t out[16][3];
    int     lens[16];

    /* Expected notes for root=0: 60=root, 64=3rd, 67=5th, 72=oct */
    int seen[4] = {0, 0, 0, 0};

    /* Run 200 steps — at max chaos all states should appear */
    for (int step = 0; step < 200; step++) {
        int n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x90) {
                uint8_t p = out[i][1];
                if (p == 60) seen[0] = 1;
                if (p == 64) seen[1] = 1;
                if (p == 67) seen[2] = 1;
                if (p == 72) seen[3] = 1;
            }
        }
    }

    CHECK(seen[0]);   /* root (60) seen */
    CHECK(seen[1]);   /* 3rd  (64) seen */
    CHECK(seen[2]);   /* 5th  (67) seen */
    CHECK(seen[3]);   /* oct  (72) seen */
}

/* =========================================================================
 * Markov: at chaos=0, oct state is unreachable (weight=0 from all states)
 * ========================================================================= */
static void test_markov_no_oct_at_zero_chaos(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_root(&e, 0);
    mg_engine_set_chaos(&e, 0.0f);
    mg_engine_set_swing(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    int saw_oct = 0;

    for (int step = 0; step < 200; step++) {
        int n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] == 72)
                saw_oct = 1;
        }
    }

    CHECK(!saw_oct);   /* no oct (72) at chaos=0 */
}

/* =========================================================================
 * Density: zero density suppresses all note-ons
 * ========================================================================= */
static void test_density_zero_suppresses_note_ons(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_density(&e, 0.0f);
    mg_engine_set_swing(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    for (int step = 0; step < 16; step++) {
        int n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n; i++)
            CHECK((out[i][0] & 0xF0) != 0x90);
    }
}

/* =========================================================================
 * Density: high density should emit more notes than low density
 * ========================================================================= */
static void test_density_high_emits_more_notes(void)
{
    uint8_t out[16][3];
    int     lens[16];
    int lo_count = 0;
    int hi_count = 0;

    mg_engine_t sparse;
    mg_engine_init(&sparse);
    mg_engine_set_density(&sparse, 0.25f);
    mg_engine_set_swing(&sparse, 0.0f);

    mg_engine_t dense;
    mg_engine_init(&dense);
    mg_engine_set_density(&dense, 0.85f);
    mg_engine_set_swing(&dense, 0.0f);

    for (int step = 0; step < 200; step++) {
        int n_lo = tick_n(&sparse, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_lo; i++) {
            if ((out[i][0] & 0xF0) == 0x90) lo_count++;
        }

        int n_hi = tick_n(&dense, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_hi; i++) {
            if ((out[i][0] & 0xF0) == 0x90) hi_count++;
        }
    }

    CHECK(hi_count > lo_count * 2);
}

/* =========================================================================
 * Resolve: strong resolve should yield more root notes than none
 * ========================================================================= */
static void test_resolve_biases_toward_root(void)
{
    uint8_t out[16][3];
    int     lens[16];
    int root_lo = 0;
    int root_hi = 0;

    mg_engine_t loose;
    mg_engine_init(&loose);
    mg_engine_set_root(&loose, 0);
    mg_engine_set_chaos(&loose, 1.0f);
    mg_engine_set_resolve(&loose, 0.0f);
    mg_engine_set_swing(&loose, 0.0f);

    mg_engine_t tight;
    mg_engine_init(&tight);
    mg_engine_set_root(&tight, 0);
    mg_engine_set_chaos(&tight, 1.0f);
    mg_engine_set_resolve(&tight, 1.0f);
    mg_engine_set_swing(&tight, 0.0f);

    for (int step = 0; step < 200; step++) {
        int n_lo = tick_n(&loose, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_lo; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] == 60)
                root_lo++;
        }

        int n_hi = tick_n(&tight, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_hi; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] == 60)
                root_hi++;
        }
    }

    CHECK(root_hi > root_lo);
}

/* =========================================================================
 * Resolve: strong resolve should reduce octave landings
 * ========================================================================= */
static void test_resolve_reduces_octave_leaps(void)
{
    uint8_t out[16][3];
    int     lens[16];
    int oct_lo = 0;
    int oct_hi = 0;

    mg_engine_t loose;
    mg_engine_init(&loose);
    mg_engine_set_root(&loose, 0);
    mg_engine_set_chaos(&loose, 1.0f);
    mg_engine_set_resolve(&loose, 0.0f);
    mg_engine_set_swing(&loose, 0.0f);

    mg_engine_t tight;
    mg_engine_init(&tight);
    mg_engine_set_root(&tight, 0);
    mg_engine_set_chaos(&tight, 1.0f);
    mg_engine_set_resolve(&tight, 1.0f);
    mg_engine_set_swing(&tight, 0.0f);

    for (int step = 0; step < 200; step++) {
        int n_lo = tick_n(&loose, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_lo; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] == 72)
                oct_lo++;
        }

        int n_hi = tick_n(&tight, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_hi; i++) {
            if ((out[i][0] & 0xF0) == 0x90 && out[i][1] == 72)
                oct_hi++;
        }
    }

    CHECK(oct_hi < oct_lo);
}

/* =========================================================================
 * Rest: re-entry after silence gets a light accent
 * ========================================================================= */
static void test_rest_reentry_gets_accent(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_density(&e, 0.0f);
    mg_engine_set_rest(&e, 1.0f);
    mg_engine_set_swing(&e, 0.0f);
    mg_engine_set_vel(&e, 90);

    uint8_t out[16][3];
    int     lens[16];

    int n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);
    for (int i = 0; i < n; i++)
        CHECK((out[i][0] & 0xF0) != 0x90);
    CHECK(e.rest_streak == 1);

    mg_engine_set_density(&e, 1.0f);
    mg_engine_set_rest(&e, 0.0f);
    n = tick_n(&e, FRAMES_PER_16TH + 5, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][2] > 90);
            found = 1;
        }
    }
    CHECK(found);
    CHECK(e.rest_streak == 0);
}

/* =========================================================================
 * Rest: high rest should create longer silence clusters at equal density
 * ========================================================================= */
static void test_rest_clusters_silences(void)
{
    uint8_t out[16][3];
    int     lens[16];
    int max_gap_lo = 0;
    int max_gap_hi = 0;
    int gap_lo = 0;
    int gap_hi = 0;

    mg_engine_t loose;
    mg_engine_init(&loose);
    mg_engine_set_density(&loose, 0.55f);
    mg_engine_set_rest(&loose, 0.0f);
    mg_engine_set_swing(&loose, 0.0f);

    mg_engine_t grouped;
    mg_engine_init(&grouped);
    mg_engine_set_density(&grouped, 0.55f);
    mg_engine_set_rest(&grouped, 1.0f);
    mg_engine_set_swing(&grouped, 0.0f);

    for (int step = 0; step < 200; step++) {
        int sounded_lo = 0;
        int n_lo = tick_n(&loose, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_lo; i++) {
            if ((out[i][0] & 0xF0) == 0x90) sounded_lo = 1;
        }
        if (sounded_lo) gap_lo = 0;
        else if (++gap_lo > max_gap_lo) max_gap_lo = gap_lo;

        int sounded_hi = 0;
        int n_hi = tick_n(&grouped, FRAMES_PER_16TH + 5, out, lens, 16);
        for (int i = 0; i < n_hi; i++) {
            if ((out[i][0] & 0xF0) == 0x90) sounded_hi = 1;
        }
        if (sounded_hi) gap_hi = 0;
        else if (++gap_hi > max_gap_hi) max_gap_hi = gap_hi;
    }

    CHECK(max_gap_hi > max_gap_lo);
}

/* =========================================================================
 * Swing: even step fires immediately with full velocity
 * ========================================================================= */
static void test_swing_even_step_immediate(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_swing(&e, 1.0f);
    mg_engine_set_vel(&e, 100);
    mg_engine_set_swing(&e, 1.0f);

    uint8_t out[16][3];
    int     lens[16];

    /* First step (step_count=0 → even): should fire immediately, no swing delay */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90) {
            CHECK(out[i][2] == 100);   /* full velocity on even step */
            found = 1;
        }
    }
    CHECK(found);

    /* No swing pending for even step */
    CHECK(e.swing_pending == 0);
}

/* =========================================================================
 * Swing: odd step is delayed and velocity is attenuated
 * ========================================================================= */
static void test_swing_odd_step_delayed(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_swing(&e, 1.0f);
    mg_engine_set_vel(&e, 100);

    uint8_t out[16][3];
    int     lens[16];

    /* Consume step 0 (even) */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);

    /* After step 0: tick past step 1 (odd) boundary
       — note-on should NOT fire yet (swing pending) */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    int n = tick_n(&e, 20, out, lens, 16);

    /* We may get a note-off for step 0, but no note-on for step 1 */
    int got_note_on = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x90)
            got_note_on = 1;
    }
    CHECK(!got_note_on);          /* note-on not yet */
    CHECK(e.swing_pending == 1);  /* swing delay armed */
    CHECK(e.swing_vel < 100);     /* attenuated velocity stored */
    /* Expected: 100 * (1 - 1.0 * 0.3) = 70 */
    CHECK(e.swing_vel >= 65 && e.swing_vel <= 75);
}

/* =========================================================================
 * Swing: pending note-on fires after swing delay
 * ========================================================================= */
static void test_swing_fires_after_delay(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_swing(&e, 1.0f);
    mg_engine_set_vel(&e, 100);

    uint8_t out[16][3];
    int     lens[16];

    /* Consume step 0 (even) */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);

    /* Cross step 1 boundary (odd) — swing pending */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);

    CHECK(e.swing_pending == 1);

    /* Tick past swing delay: swing_frames = frames_per_step * 1.0 * 0.5 ≈ 2756 */
    int found_delayed_on = 0;
    for (int t = 0; t < 40 && !found_delayed_on; t++) {
        int n = tick_n(&e, 100, out, lens, 16);
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x90) {
                found_delayed_on = 1;
                /* velocity must be attenuated */
                CHECK(out[i][2] >= 65 && out[i][2] <= 75);
            }
        }
    }
    CHECK(found_delayed_on);
    CHECK(e.swing_pending == 0);
}

/* =========================================================================
 * Note lifecycle: note-off is sent after gate duration
 * ========================================================================= */
static void test_note_off_after_gate(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_gate(&e, 0.5f);   /* note-off after 50% of step */
    mg_engine_set_swing(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    /* Fire step 0 */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);
    CHECK(e.active_on == 1);

    /* Tick past gate duration (50% of 5512 ≈ 2756 frames) */
    int found_note_off = 0;
    int total = 0;
    while (total < FRAMES_PER_16TH && !found_note_off) {
        int n = tick_n(&e, 100, out, lens, 16);
        total += 100;
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x80)
                found_note_off = 1;
        }
    }
    CHECK(found_note_off);
}

/* =========================================================================
 * Flush: emits note-off when note is active
 * ========================================================================= */
static void test_flush_emits_note_off(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_gate(&e, 1.0f);   /* hold full step to ensure active */
    mg_engine_set_swing(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    /* Fire a note */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);
    CHECK(e.active_on == 1);

    /* Flush should emit exactly one note-off */
    int n = mg_engine_flush(&e, out, lens, 16);
    CHECK(n == 1);
    CHECK((out[0][0] & 0xF0) == 0x80);
    CHECK(e.active_on == 0);
    CHECK(e.active_note == MG_NO_NOTE);
    CHECK(e.swing_pending == 0);
}

/* =========================================================================
 * Flush: idempotent — second flush emits nothing
 * ========================================================================= */
static void test_flush_idempotent(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_gate(&e, 1.0f);
    mg_engine_set_swing(&e, 0.0f);

    uint8_t out[16][3];
    int     lens[16];

    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    tick_n(&e, 20, out, lens, 16);

    mg_engine_flush(&e, out, lens, 16);
    int n2 = mg_engine_flush(&e, out, lens, 16);
    CHECK(n2 == 0);
}

/* =========================================================================
 * Reset: clears timing, leaves params intact
 * ========================================================================= */
static void test_reset_clears_timing(void)
{
    mg_engine_t e;
    mg_engine_init(&e);
    mg_engine_set_vel(&e, 77);

    uint8_t out[16][3];
    int     lens[16];

    /* Advance timing */
    tick_n(&e, FRAMES_PER_16TH - 10, out, lens, 16);
    CHECK(e.frames_accum > 0.0);

    mg_engine_flush(&e, out, lens, 16);
    mg_engine_reset(&e);

    CHECK(e.frames_accum    == 0.0);
    CHECK(e.step_count      == 0);
    CHECK(e.swing_pending   == 0);
    CHECK(e.frames_until_off == 0.0);

    /* Parameters unaffected */
    CHECK(mg_engine_get_vel(&e) == 77);
}

/* =========================================================================
 * Steps: quarter-note mode fires 4x less often than 16th-note mode
 * ========================================================================= */
static void test_steps_quarter_fires_less(void)
{
    uint8_t out[16][3];
    int     lens[16];

    /* Count note-ons in 20000 frames at 16th-note mode */
    mg_engine_t e16;
    mg_engine_init(&e16);
    mg_engine_set_steps(&e16, MG_STEPS_16);
    mg_engine_set_swing(&e16, 0.0f);
    int count16 = 0;
    for (int i = 0; i < 20000; i += 100) {
        int n = tick_n(&e16, 100, out, lens, 16);
        for (int j = 0; j < n; j++)
            if ((out[j][0] & 0xF0) == 0x90) count16++;
    }

    /* Count note-ons in 20000 frames at quarter-note mode */
    mg_engine_t e4;
    mg_engine_init(&e4);
    mg_engine_set_steps(&e4, MG_STEPS_4);
    mg_engine_set_swing(&e4, 0.0f);
    int count4 = 0;
    for (int i = 0; i < 20000; i += 100) {
        int n = tick_n(&e4, 100, out, lens, 16);
        for (int j = 0; j < n; j++)
            if ((out[j][0] & 0xF0) == 0x90) count4++;
    }

    /* 16th mode should produce ~4x more notes than quarter mode */
    CHECK(count16 > count4 * 3);
}

/* =========================================================================
 * Main
 * ========================================================================= */
int main(void)
{
    printf("=== markov_groove_engine_test ===\n");

    test_init_defaults();
    test_param_root();
    test_param_steps();
    test_param_scale();
    test_param_range();
    test_param_spread();
    test_param_density();
    test_param_chaos();
    test_param_rest();
    test_param_resolve();
    test_param_swing();
    test_param_gate();
    test_param_vel();
    test_tick_emits_note_on();
    test_tick_note_range();
    test_tick_root_offset();
    test_tick_negative_root_offset();
    test_scale_aeolian_uses_flat_third();
    test_scale_phrygian_uses_flat_second();
    test_range_wide_expands_register();
    test_spread_biases_upper_register();
    test_markov_all_states_reachable();
    test_markov_no_oct_at_zero_chaos();
    test_density_zero_suppresses_note_ons();
    test_density_high_emits_more_notes();
    test_resolve_biases_toward_root();
    test_resolve_reduces_octave_leaps();
    test_rest_reentry_gets_accent();
    test_rest_clusters_silences();
    test_swing_even_step_immediate();
    test_swing_odd_step_delayed();
    test_swing_fires_after_delay();
    test_note_off_after_gate();
    test_flush_emits_note_off();
    test_flush_idempotent();
    test_reset_clears_timing();
    test_steps_quarter_fires_less();

    printf("Engine tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
