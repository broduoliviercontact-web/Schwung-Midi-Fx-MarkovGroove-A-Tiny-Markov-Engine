/*
 * markov_groove_midi_fx_test.c — host wrapper and MIDI dispatch tests
 *
 * Tests the full plugin interface (midi_fx_api_v1_t) using a minimal mock host.
 * Build: gcc -std=c99 -Wall -Wextra -Werror -Isrc/dsp -Isrc/host \
 *             tests/markov_groove_midi_fx_test.c \
 *             src/dsp/markov_groove_engine.c src/host/markov_groove_plugin.c \
 *             -o build/tests/markov_groove_midi_fx_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "../src/host/midi_fx_api_v1.h"
#include "../src/host/plugin_api_v1.h"
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

/* =========================================================================
 * Mock host — get_clock_status/get_bpm are intentionally NULL.
 * The plugin no longer calls these (they cause SIGSEGV on some firmware).
 * Transport state is driven entirely by MIDI bytes (0xFA/0xFB/0xFC/0xF8).
 * ========================================================================= */
static host_api_v1_t mock_host = {
    .api_version        = 1,
    .sample_rate        = 44100,
    .frames_per_block   = 512,
    .log                = NULL,
    .get_clock_status   = NULL,   /* not used — MIDI bytes drive transport */
    .get_bpm            = NULL,   /* not used — 0xF8 clock bytes drive BPM */
    .midi_send_internal = NULL,
    .midi_send_external = NULL,
    .mod_emit_value     = NULL,
    .mod_clear_source   = NULL,
    .mod_host_ctx       = NULL,
};

/* Global API pointer — set once in main */
static midi_fx_api_v1_t *api = NULL;

/* Convenience: tick until a note-on appears or max_ticks reached */
#define SAMPLE_RATE  44100
#define FRAMES_STEP  5513   /* ~16th note at 120 BPM, 44100 Hz */

/* Send MIDI start + enough 0xF8 clocks to seed BPM, then tick until note-on */
static int run_until_note_on(void *inst, int max_ticks,
                              uint8_t out[][3], int lens[], int max_out)
{
    /* Start transport */
    uint8_t start[1] = {0xFA};
    api->process_midi(inst, start, 1, out, lens, max_out);

    /* Feed 48 clock ticks (2 quarter notes) so BPM is estimated */
    uint8_t clk[1] = {0xF8};
    for (int i = 0; i < 48; i++)
        api->process_midi(inst, clk, 1, out, lens, max_out);

    for (int t = 0; t < max_ticks; t++) {
        int n = api->tick(inst, FRAMES_STEP, SAMPLE_RATE, out, lens, max_out);
        for (int i = 0; i < n; i++) {
            if ((out[i][0] & 0xF0) == 0x90)
                return n;
        }
    }
    return 0;
}

/* =========================================================================
 * create_instance: returns non-NULL, defaults are correct
 * ========================================================================= */
static void test_create_instance(void)
{
    void *inst = api->create_instance(".", NULL);
    CHECK(inst != NULL);

    char buf[64];
    /* Check a few defaults via get_param */
    int r;

    r = api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK(atoi(buf) == 0);

    r = api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK(strcmp(buf, "16") == 0);

    r = api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK(strcmp(buf, "ionian") == 0);

    r = api->get_param(inst, "range", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK(strcmp(buf, "close") == 0);

    r = api->get_param(inst, "spread", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK_FLOAT_EQ(atof(buf), 0.0, 0.01);

    r = api->get_param(inst, "density", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK_FLOAT_EQ(atof(buf), 1.0, 0.01);

    r = api->get_param(inst, "vel", buf, sizeof(buf));
    CHECK(r > 0);
    CHECK(atoi(buf) == 90);

    api->destroy_instance(inst);
}

/* =========================================================================
 * get_param: every known key returns > 0; unknown key returns -1
 * The most critical single test — catches silent param display breakage.
 * ========================================================================= */
static void test_get_param_returns_snprintf(void)
{
    void *inst = api->create_instance(".", NULL);
    CHECK(inst != NULL);

    static const char *known_keys[] = {
        "root", "steps", "scale", "range", "spread", "density", "chaos", "rest", "resolve", "swing", "gate", "vel",
        "sync_warn",
        NULL
    };

    char buf[64];
    for (int i = 0; known_keys[i]; i++) {
        int r = api->get_param(inst, known_keys[i], buf, sizeof(buf));
        CHECK(r > 0);   /* must be snprintf result — never 0 or -1 */
    }

    /* Unknown key must return -1 */
    CHECK(api->get_param(inst, "__unknown__", buf, sizeof(buf)) == -1);

    api->destroy_instance(inst);
}

/* =========================================================================
 * set_param / get_param: round-trip for every parameter
 * ========================================================================= */
static void test_set_get_roundtrip_root(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "root", "7");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 7);

    api->set_param(inst, "root", "5.0000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 5);

    api->set_param(inst, "root", "0.5000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 0);

    api->set_param(inst, "root", "-5");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -5);

    api->set_param(inst, "root", "-5.0000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -5);

    api->set_param(inst, "root", "24");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 24);

    api->set_param(inst, "root", "-24");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -24);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_steps(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "steps", "4");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "4") == 0);

    api->set_param(inst, "steps", "8");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "8") == 0);

    api->set_param(inst, "steps", "16");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "16") == 0);

    api->set_param(inst, "steps", "8.0000");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "8") == 0);

    api->set_param(inst, "steps", "0.5000");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "8") == 0);

    /* Unknown steps value → default to "16" */
    api->set_param(inst, "steps", "99");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(strcmp(buf, "16") == 0);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_scale(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "scale", "aeolian");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(strcmp(buf, "aeolian") == 0);

    api->set_param(inst, "scale", "4");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(strcmp(buf, "major_pent") == 0);

    api->set_param(inst, "scale", "phrygian");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(strcmp(buf, "phrygian") == 0);

    api->set_param(inst, "scale", "0.9000");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(strcmp(buf, "harmonic_minor") == 0);

    api->set_param(inst, "scale", "1.0000");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK(strcmp(buf, "blues") == 0);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_range(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "range", "octave");
    api->get_param(inst, "range", buf, sizeof(buf));
    CHECK(strcmp(buf, "octave") == 0);

    api->set_param(inst, "range", "0.9000");
    api->get_param(inst, "range", buf, sizeof(buf));
    CHECK(strcmp(buf, "wide") == 0);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_spread(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "spread", "0.65");
    api->get_param(inst, "spread", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.65, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_density(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "density", "0.35");
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.35, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_chaos(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "chaos", "0.75");
    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.75, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_rest(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "rest", "0.4");
    api->get_param(inst, "rest", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.4, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_resolve(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "resolve", "0.6");
    api->get_param(inst, "resolve", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.6, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_swing(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "swing", "0.5");
    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.5, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_gate(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "gate", "0.8");
    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_FLOAT_EQ(atof(buf), 0.8, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_roundtrip_vel(void)
{
    void *inst = api->create_instance(".", NULL);
    char buf[32];

    api->set_param(inst, "vel", "64");
    api->get_param(inst, "vel", buf, sizeof(buf));
    CHECK(atoi(buf) == 64);

    api->set_param(inst, "vel", "64.0000");
    api->get_param(inst, "vel", buf, sizeof(buf));
    CHECK(atoi(buf) == 64);

    api->set_param(inst, "vel", "0.5000");
    api->get_param(inst, "vel", buf, sizeof(buf));
    CHECK(atoi(buf) == 74);

    api->destroy_instance(inst);
}

/* =========================================================================
 * process_midi: unrecognized messages are passed through unchanged
 * ========================================================================= */
static void test_process_midi_passthrough(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Note-on from upstream */
    uint8_t msg_on[3] = {0x90, 60, 100};
    int n = api->process_midi(inst, msg_on, 3, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK(out[0][0] == 0x90);
    CHECK(out[0][1] == 60);
    CHECK(out[0][2] == 100);

    /* CC from upstream */
    uint8_t msg_cc[3] = {0xB0, 7, 64};
    n = api->process_midi(inst, msg_cc, 3, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK(out[0][0] == 0xB0);
    CHECK(out[0][1] == 7);

    /* Note-off */
    uint8_t msg_off[3] = {0x80, 60, 0};
    n = api->process_midi(inst, msg_off, 3, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK((out[0][0] & 0xF0) == 0x80);

    api->destroy_instance(inst);
}

/* =========================================================================
 * process_midi: 0xFA/0xFB (start/continue) are consumed, not forwarded
 * ========================================================================= */
static void test_process_midi_transport_start(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    uint8_t start[1] = {0xFA};
    int n = api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);   /* consumed, not forwarded */

    uint8_t cont[1] = {0xFB};
    n = api->process_midi(inst, cont, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);

    api->destroy_instance(inst);
}

/* =========================================================================
 * process_midi: 0xFC (stop) flushes any active note → only note-offs emitted
 * ========================================================================= */
static void test_process_midi_transport_stop_no_stuck_notes(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Start transport and produce a note */
    run_until_note_on(inst, 10, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* Send transport stop */
    uint8_t stop[1] = {0xFC};
    int n = api->process_midi(inst, stop, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* All emitted messages must be note-offs (or nothing) */
    for (int i = 0; i < n; i++) {
        CHECK((out[i][0] & 0xF0) == 0x80);
    }

    /* After stop, ticking should produce no note-ons */
    int n2 = api->tick(inst, FRAMES_STEP * 20, SAMPLE_RATE,
                       out, lens, MIDI_FX_MAX_OUT_MSGS);
    for (int i = 0; i < n2; i++) {
        CHECK((out[i][0] & 0xF0) != 0x90);
    }

    api->destroy_instance(inst);
}

/* =========================================================================
 * tick: generates notes when transport is running
 * ========================================================================= */
static void test_tick_generates_notes_when_running(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Should get a note-on within a few steps after transport start */
    int n = run_until_note_on(inst, 10, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);

    api->destroy_instance(inst);
}

/* =========================================================================
 * tick: no notes when transport is stopped
 * ========================================================================= */
static void test_tick_silent_when_stopped(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* No 0xFA sent — running stays 0 */
    for (int t = 0; t < 20; t++) {
        int n = api->tick(inst, FRAMES_STEP, SAMPLE_RATE,
                          out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int i = 0; i < n; i++) {
            CHECK((out[i][0] & 0xF0) != 0x90);
        }
    }

    api->destroy_instance(inst);
}

/* =========================================================================
 * tick: UNAVAILABLE treated same as STOPPED
 * ========================================================================= */
static void test_tick_silent_before_start_byte(void)
{
    /* No 0xFA/0xFB received — module must stay silent regardless of ticks */
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Feed plenty of 0xF8 clock ticks (BPM ready) but no start */
    uint8_t clk[1] = {0xF8};
    for (int i = 0; i < 96; i++)
        api->process_midi(inst, clk, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);

    for (int t = 0; t < 20; t++) {
        int n = api->tick(inst, FRAMES_STEP, SAMPLE_RATE,
                          out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int i = 0; i < n; i++) {
            CHECK((out[i][0] & 0xF0) != 0x90);
        }
    }

    api->destroy_instance(inst);
}

/* =========================================================================
 * tick: transition from RUNNING → STOPPED flushes active note
 * ========================================================================= */
static void test_stop_byte_flushes_active_note(void)
{
    void *inst = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Set gate=1.0 so the note stays open through the step */
    api->set_param(inst, "gate", "1.0");
    run_until_note_on(inst, 10, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* Send 0xFC stop — must flush the active note */
    uint8_t stop[1] = {0xFC};
    int n = api->process_midi(inst, stop, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);

    int found_off = 0;
    for (int i = 0; i < n; i++) {
        if ((out[i][0] & 0xF0) == 0x80) found_off = 1;
        CHECK((out[i][0] & 0xF0) != 0x90);
    }
    CHECK(found_off);

    /* After stop, ticking produces no note-ons */
    int n2 = api->tick(inst, FRAMES_STEP * 5, SAMPLE_RATE,
                       out, lens, MIDI_FX_MAX_OUT_MSGS);
    for (int i = 0; i < n2; i++)
        CHECK((out[i][0] & 0xF0) != 0x90);

    api->destroy_instance(inst);
}

/* =========================================================================
 * save_state / load_state: all params survive a save/restore cycle
 * ========================================================================= */
static void test_save_load_roundtrip(void)
{
    void *inst = api->create_instance(".", NULL);

    /* Set non-default values */
    api->set_param(inst, "root",  "-7");
    api->set_param(inst, "steps", "8");
    api->set_param(inst, "scale", "suspended");
    api->set_param(inst, "range", "wide");
    api->set_param(inst, "spread", "0.7");
    api->set_param(inst, "density", "0.45");
    api->set_param(inst, "chaos", "0.8");
    api->set_param(inst, "rest", "0.2");
    api->set_param(inst, "resolve", "0.6");
    api->set_param(inst, "swing", "0.6");
    api->set_param(inst, "gate",  "0.3");
    api->set_param(inst, "vel",   "64");

    /* Save */
    char state[512];
    int  n = api->save_state(inst, state, (int)sizeof(state));
    CHECK(n > 0);

    /* Reset to defaults */
    api->set_param(inst, "root",  "0");
    api->set_param(inst, "steps", "16");
    api->set_param(inst, "scale", "ionian");
    api->set_param(inst, "range", "close");
    api->set_param(inst, "spread", "0.0");
    api->set_param(inst, "density", "1.0");
    api->set_param(inst, "chaos", "0.3");
    api->set_param(inst, "rest", "0.0");
    api->set_param(inst, "resolve", "0.0");
    api->set_param(inst, "swing", "0.0");
    api->set_param(inst, "gate",  "0.5");
    api->set_param(inst, "vel",   "90");

    /* Restore */
    api->load_state(inst, state, (int)strlen(state));

    /* Verify */
    char buf[64];

    api->get_param(inst, "root",  buf, sizeof(buf)); CHECK(atoi(buf) == -7);
    api->get_param(inst, "steps", buf, sizeof(buf)); CHECK(strcmp(buf, "8") == 0);
    api->get_param(inst, "scale",  buf, sizeof(buf)); CHECK(strcmp(buf, "suspended") == 0);
    api->get_param(inst, "range",  buf, sizeof(buf)); CHECK(strcmp(buf, "wide") == 0);
    api->get_param(inst, "spread", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.7, 0.01);
    api->get_param(inst, "density", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.45, 0.01);
    api->get_param(inst, "chaos", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.8, 0.01);
    api->get_param(inst, "rest", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.2, 0.01);
    api->get_param(inst, "resolve", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.6, 0.01);
    api->get_param(inst, "swing", buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.6, 0.01);
    api->get_param(inst, "gate",  buf, sizeof(buf)); CHECK_FLOAT_EQ(atof(buf), 0.3, 0.01);
    api->get_param(inst, "vel",   buf, sizeof(buf)); CHECK(atoi(buf) == 64);

    api->destroy_instance(inst);
}

/* =========================================================================
 * save_state: active note state is NOT serialised
 * Loading state into a fresh instance must not leave a stuck note.
 * ========================================================================= */
static void test_save_does_not_serialise_note_state(void)
{
    void *inst_a = api->create_instance(".", NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Produce a note in inst_a */
    api->set_param(inst_a, "gate", "1.0");
    run_until_note_on(inst_a, 10, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* Save while note is active */
    char state[512];
    api->save_state(inst_a, state, (int)sizeof(state));

    /* Load into a fresh instance — no transport start sent */
    void *inst_b = api->create_instance(".", NULL);
    api->load_state(inst_b, state, (int)strlen(state));

    /* Tick without start byte — must produce no note-ons */
    int n = api->tick(inst_b, FRAMES_STEP * 5, SAMPLE_RATE,
                      out, lens, MIDI_FX_MAX_OUT_MSGS);
    for (int i = 0; i < n; i++) {
        CHECK((out[i][0] & 0xF0) != 0x90);
    }

    api->destroy_instance(inst_a);
    api->destroy_instance(inst_b);
}

/* =========================================================================
 * load_state: malformed state does not crash
 * ========================================================================= */
static void test_load_state_malformed(void)
{
    void *inst = api->create_instance(".", NULL);

    /* These should all be silent no-ops */
    api->load_state(inst, "",       0);
    api->load_state(inst, NULL,     0);
    api->load_state(inst, "=\n",    2);
    api->load_state(inst, "root\n", 5);           /* key without value */
    api->load_state(inst, "root=garbage\n", 13);  /* invalid int → clamped */

    /* Instance still usable after bad loads */
    char buf[32];
    int r = api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(r > 0);

    api->destroy_instance(inst);
}

/* =========================================================================
 * API version is correct
 * ========================================================================= */
static void test_api_version(void)
{
    CHECK(api->api_version == MIDI_FX_API_VERSION);
}

/* =========================================================================
 * Main
 * ========================================================================= */
int main(void)
{
    printf("=== markov_groove_midi_fx_test ===\n");

    api = move_midi_fx_init(&mock_host);
    if (!api) {
        fprintf(stderr, "FATAL: move_midi_fx_init returned NULL\n");
        return 1;
    }

    test_api_version();
    test_create_instance();
    test_get_param_returns_snprintf();
    test_set_get_roundtrip_root();
    test_set_get_roundtrip_steps();
    test_set_get_roundtrip_scale();
    test_set_get_roundtrip_range();
    test_set_get_roundtrip_spread();
    test_set_get_roundtrip_density();
    test_set_get_roundtrip_chaos();
    test_set_get_roundtrip_rest();
    test_set_get_roundtrip_resolve();
    test_set_get_roundtrip_swing();
    test_set_get_roundtrip_gate();
    test_set_get_roundtrip_vel();
    test_process_midi_passthrough();
    test_process_midi_transport_start();
    test_process_midi_transport_stop_no_stuck_notes();
    test_tick_generates_notes_when_running();
    test_tick_silent_when_stopped();
    test_tick_silent_before_start_byte();
    test_stop_byte_flushes_active_note();
    test_save_load_roundtrip();
    test_save_does_not_serialise_note_state();
    test_load_state_malformed();

    printf("MIDI FX tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
