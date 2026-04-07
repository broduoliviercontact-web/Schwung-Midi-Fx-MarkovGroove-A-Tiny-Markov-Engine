/*
 * markov_groove_plugin.c — Schwung host wrapper for MarkovGroove
 *
 * Clock strategy: MIDI transport bytes only — NO get_clock_status() or get_bpm().
 *   Calling get_clock_status()/get_bpm() via the host vtable causes SIGSEGV
 *   on some Move firmware versions, even when the pointer is non-NULL.
 *
 * Transport sync:
 *   - 0xFA (start)    → running = 1
 *   - 0xFB (continue) → running = 1
 *   - 0xFC (stop)     → flush active note, reset timing, running = 0
 *
 * BPM tracking:
 *   - MIDI clock 0xF8 bytes (24 per quarter note) are counted in process_midi.
 *   - tick() accumulates frames and recomputes BPM every 24 clock ticks.
 *   - Defaults to 120.0 BPM until the first MIDI clock is received.
 *
 * MIDI pass-through:
 *   - All non-transport incoming MIDI is forwarded unchanged.
 *   - Single-byte transport/clock messages (0xF8/0xFA/0xFB/0xFC) are consumed.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "midi_fx_api_v1.h"
#include "plugin_api_v1.h"
#include "markov_groove_engine.h"

/* Module-level host pointer — kept for optional log() calls only. */
static const host_api_v1_t *g_host = NULL;

/* Per-instance state */
typedef struct {
    mg_engine_t engine;
    int         running;          /* 0=stopped, 1=running; set by MIDI transport bytes */

    /* BPM estimation from 0xF8 MIDI clock ticks */
    uint32_t    clock_ticks;      /* 0xF8 bytes received since last BPM recalc */
    uint32_t    clock_frames;     /* frames accumulated since last BPM recalc */
    float       estimated_bpm;   /* last stable BPM estimate; default 120.0 */
} mg_instance_t;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

static void *create_instance(const char *module_dir, const char *config_json)
{
    (void)module_dir;
    (void)config_json;

    mg_instance_t *inst = calloc(1, sizeof(mg_instance_t));
    if (!inst) return NULL;

    mg_engine_init(&inst->engine);
    inst->running       = 0;       /* never 1 */
    inst->clock_ticks   = 0;
    inst->clock_frames  = 0;
    inst->estimated_bpm = 120.0f;

    return inst;
}

static void destroy_instance(void *instance)
{
    free(instance);
}

/* =========================================================================
 * process_midi
 * ========================================================================= */

static int process_midi(void *instance,
                        const uint8_t *in_msg, int in_len,
                        uint8_t out_msgs[][3], int out_lens[],
                        int max_out)
{
    mg_instance_t *inst = (mg_instance_t *)instance;
    if (!inst || !in_msg || in_len < 1 || max_out < 1) return 0;

    if (in_len == 1) {
        switch (in_msg[0]) {
            case 0xF8:   /* MIDI clock tick — count for BPM estimation */
                inst->clock_ticks++;
                return 0;

            case 0xFA:   /* start */
            case 0xFB:   /* continue */
                inst->running = 1;
                return 0;

            case 0xFC:   /* stop — flush active note, reset timing */
                {
                    int n = mg_engine_flush(&inst->engine,
                                            out_msgs, out_lens, max_out);
                    mg_engine_reset(&inst->engine);
                    inst->running      = 0;
                    inst->clock_ticks  = 0;
                    inst->clock_frames = 0;
                    return n;
                }

            default:
                return 0;   /* other single-byte system messages: consume */
        }
    }

    /* Pass through all other incoming MIDI unchanged. */
    if (in_len <= 3) {
        memcpy(out_msgs[0], in_msg, (size_t)in_len);
        out_lens[0] = in_len;
        return 1;
    }

    return 0;
}

/* =========================================================================
 * tick
 * ========================================================================= */

static int tick(void *instance,
                int nframes, int sample_rate,
                uint8_t out_msgs[][3], int out_lens[],
                int max_out)
{
    mg_instance_t *inst = (mg_instance_t *)instance;
    if (!inst || nframes <= 0 || max_out < 1) return 0;

    /* --- BPM estimation from accumulated 0xF8 clock ticks ---
     *
     * MIDI clock: 24 ticks per quarter note.
     * Recalculate every 24 ticks (once per quarter note) for stability.
     * If no clock ticks have arrived yet, keep the last estimate (default 120).
     */
    inst->clock_frames += (uint32_t)nframes;

    if (inst->clock_ticks >= 24 && inst->clock_frames > 0) {
        float bpm = 60.0f * (float)sample_rate
                    * (float)inst->clock_ticks
                    / (24.0f * (float)inst->clock_frames);
        if (bpm >= 20.0f && bpm <= 300.0f)
            inst->estimated_bpm = bpm;
        inst->clock_ticks  = 0;
        inst->clock_frames = 0;
    }

    if (!inst->running) return 0;

    return mg_engine_tick(&inst->engine,
                          nframes, inst->estimated_bpm, sample_rate,
                          out_msgs, out_lens, max_out);
}

/* =========================================================================
 * set_param / get_param
 * ========================================================================= */

static int is_floatish(const char *val)
{
    return val && (strchr(val, '.') != NULL ||
                   strchr(val, 'e') != NULL ||
                   strchr(val, 'E') != NULL);
}

static int round_nearest(float v)
{
    return (v >= 0.0f) ? (int)(v + 0.5f) : (int)(v - 0.5f);
}

static int float_is_near(float a, float b)
{
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0005f;
}

/*
 * parse_scaled_int_param
 *
 * Move can surface int params in multiple string forms depending on context:
 *   - raw integer strings from state recall, e.g. "7"
 *   - normalized knob values, e.g. "0.6364"
 *   - raw values formatted as floats, e.g. "64.0000"
 */
static int parse_scaled_int_param(const char *val, int raw_min, int raw_max)
{
    float v;
    int rounded;

    if (!val) return raw_min;
    if (!is_floatish(val)) return atoi(val);

    v = (float)atof(val);
    rounded = round_nearest(v);

    /* Raw integer values may arrive as float strings such as "64.0000" or "-5.0000". */
    if (float_is_near(v, (float)rounded) &&
        rounded >= raw_min && rounded <= raw_max)
        return rounded;

    if (v <= 0.0f) return raw_min;
    if (v >= 1.0f) return raw_max;

    return raw_min + round_nearest(v * (float)(raw_max - raw_min));
}

static int parse_steps_param(const char *val)
{
    float v;

    if (!val) return MG_STEPS_16;

    if (strcmp(val, "4") == 0 || strcmp(val, "0") == 0) return MG_STEPS_4;
    if (strcmp(val, "8") == 0 || strcmp(val, "1") == 0) return MG_STEPS_8;
    if (strcmp(val, "16") == 0 || strcmp(val, "2") == 0) return MG_STEPS_16;

    v = (float)atof(val);

    if (float_is_near(v, 4.0f) || float_is_near(v, 0.0f)) return MG_STEPS_4;
    if (float_is_near(v, 8.0f)) return MG_STEPS_8;
    if (float_is_near(v, 16.0f) || float_is_near(v, 2.0f)) return MG_STEPS_16;

    /* Normalized float from the hardware knob. */
    return (v < 0.33f) ? MG_STEPS_4 : (v < 0.67f) ? MG_STEPS_8 : MG_STEPS_16;
}

static int parse_scale_param(const char *val)
{
    float v;

    if (!val) return MG_SCALE_IONIAN;

    if (strcmp(val, "ionian") == 0 || strcmp(val, "major") == 0 || strcmp(val, "0") == 0)
        return MG_SCALE_IONIAN;
    if (strcmp(val, "aeolian") == 0 || strcmp(val, "minor") == 0 || strcmp(val, "1") == 0)
        return MG_SCALE_AEOLIAN;
    if (strcmp(val, "dorian") == 0 || strcmp(val, "2") == 0)
        return MG_SCALE_DORIAN;
    if (strcmp(val, "mixolydian") == 0 || strcmp(val, "3") == 0)
        return MG_SCALE_MIXOLYDIAN;
    if (strcmp(val, "major_pent") == 0 || strcmp(val, "major_pentatonic") == 0 || strcmp(val, "4") == 0)
        return MG_SCALE_MAJOR_PENT;
    if (strcmp(val, "minor_pent") == 0 || strcmp(val, "minor_pentatonic") == 0 || strcmp(val, "5") == 0)
        return MG_SCALE_MINOR_PENT;
    if (strcmp(val, "suspended") == 0 || strcmp(val, "sus") == 0 || strcmp(val, "6") == 0)
        return MG_SCALE_SUSPENDED;
    if (strcmp(val, "power") == 0 || strcmp(val, "7") == 0)
        return MG_SCALE_POWER;
    if (strcmp(val, "phrygian") == 0 || strcmp(val, "8") == 0)
        return MG_SCALE_PHRYGIAN;
    if (strcmp(val, "lydian") == 0 || strcmp(val, "9") == 0)
        return MG_SCALE_LYDIAN;
    if (strcmp(val, "harmonic_minor") == 0 || strcmp(val, "harm_minor") == 0 || strcmp(val, "10") == 0)
        return MG_SCALE_HARM_MINOR;
    if (strcmp(val, "blues") == 0 || strcmp(val, "11") == 0)
        return MG_SCALE_BLUES;

    v = (float)atof(val);

    if (v <= 0.0f) return MG_SCALE_IONIAN;

    /* UI knobs send normalized 0.0–1.0 float strings. */
    if (v <= 1.0f) {
        int idx = (int)(v * (float)MG_NUM_SCALES);
        if (idx >= MG_NUM_SCALES) idx = MG_NUM_SCALES - 1;
        return idx;
    }

    /* State or host code may still surface raw float indices. */
    if (float_is_near(v, 1.0f)) return MG_SCALE_AEOLIAN;
    if (float_is_near(v, 2.0f)) return MG_SCALE_DORIAN;
    if (float_is_near(v, 3.0f)) return MG_SCALE_MIXOLYDIAN;
    if (float_is_near(v, 4.0f)) return MG_SCALE_MAJOR_PENT;
    if (float_is_near(v, 5.0f)) return MG_SCALE_MINOR_PENT;
    if (float_is_near(v, 6.0f)) return MG_SCALE_SUSPENDED;
    if (float_is_near(v, 7.0f)) return MG_SCALE_POWER;
    if (float_is_near(v, 8.0f)) return MG_SCALE_PHRYGIAN;
    if (float_is_near(v, 9.0f)) return MG_SCALE_LYDIAN;
    if (float_is_near(v, 10.0f)) return MG_SCALE_HARM_MINOR;
    return MG_SCALE_BLUES;
}

static int parse_range_param(const char *val)
{
    float v;

    if (!val) return MG_RANGE_CLOSE;

    if (strcmp(val, "close") == 0 || strcmp(val, "0") == 0) return MG_RANGE_CLOSE;
    if (strcmp(val, "octave") == 0 || strcmp(val, "1") == 0) return MG_RANGE_OCTAVE;
    if (strcmp(val, "wide") == 0 || strcmp(val, "2") == 0) return MG_RANGE_WIDE;

    v = (float)atof(val);

    if (float_is_near(v, 0.0f)) return MG_RANGE_CLOSE;
    if (float_is_near(v, 1.0f)) return MG_RANGE_OCTAVE;
    if (float_is_near(v, 2.0f)) return MG_RANGE_WIDE;

    return (v < 0.33f) ? MG_RANGE_CLOSE : (v < 0.67f) ? MG_RANGE_OCTAVE : MG_RANGE_WIDE;
}

static void set_param(void *instance, const char *key, const char *val)
{
    mg_instance_t *inst = (mg_instance_t *)instance;
    if (!inst || !key || !val) return;

    if (strcmp(key, "root") == 0) {
        mg_engine_set_root(&inst->engine, parse_scaled_int_param(val, -24, 24));

    } else if (strcmp(key, "steps") == 0) {
        mg_engine_set_steps(&inst->engine, parse_steps_param(val));

    } else if (strcmp(key, "scale") == 0 || strcmp(key, "mode") == 0) {
        mg_engine_set_scale(&inst->engine, parse_scale_param(val));

    } else if (strcmp(key, "range") == 0) {
        mg_engine_set_range(&inst->engine, parse_range_param(val));

    } else if (strcmp(key, "spread") == 0) {
        mg_engine_set_spread(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "density") == 0) {
        mg_engine_set_density(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "chaos") == 0) {
        mg_engine_set_chaos(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "rest") == 0) {
        mg_engine_set_rest(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "resolve") == 0) {
        mg_engine_set_resolve(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "swing") == 0) {
        mg_engine_set_swing(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "gate") == 0) {
        mg_engine_set_gate(&inst->engine, (float)atof(val));

    } else if (strcmp(key, "vel") == 0) {
        mg_engine_set_vel(&inst->engine, parse_scaled_int_param(val, 20, 127));
    }
}

static int get_param(void *instance, const char *key, char *buf, int buf_len)
{
    mg_instance_t *inst = (mg_instance_t *)instance;
    if (!inst || !key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "root") == 0)
        return snprintf(buf, (size_t)buf_len, "%d",
                        mg_engine_get_root(&inst->engine));

    if (strcmp(key, "steps") == 0) {
        static const char *names[] = {"4", "8", "16"};
        int idx = mg_engine_get_steps_idx(&inst->engine);
        if (idx < 0 || idx > 2) idx = 2;
        return snprintf(buf, (size_t)buf_len, "%s", names[idx]);
    }

    if (strcmp(key, "scale") == 0) {
        static const char *names[] = {
            "ionian", "aeolian", "dorian", "mixolydian",
            "major_pent", "minor_pent", "suspended", "power",
            "phrygian", "lydian", "harmonic_minor", "blues"
        };
        int idx = mg_engine_get_scale(&inst->engine);
        if (idx < 0 || idx >= MG_NUM_SCALES) idx = MG_SCALE_IONIAN;
        return snprintf(buf, (size_t)buf_len, "%s", names[idx]);
    }

    if (strcmp(key, "range") == 0) {
        static const char *names[] = {"close", "octave", "wide"};
        int idx = mg_engine_get_range(&inst->engine);
        if (idx < 0 || idx >= MG_NUM_RANGES) idx = MG_RANGE_CLOSE;
        return snprintf(buf, (size_t)buf_len, "%s", names[idx]);
    }

    if (strcmp(key, "spread") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_spread(&inst->engine));

    if (strcmp(key, "density") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_density(&inst->engine));

    if (strcmp(key, "chaos") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_chaos(&inst->engine));

    if (strcmp(key, "rest") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_rest(&inst->engine));

    if (strcmp(key, "resolve") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_resolve(&inst->engine));

    if (strcmp(key, "swing") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_swing(&inst->engine));

    if (strcmp(key, "gate") == 0)
        return snprintf(buf, (size_t)buf_len, "%.4f",
                        mg_engine_get_gate(&inst->engine));

    if (strcmp(key, "vel") == 0)
        return snprintf(buf, (size_t)buf_len, "%d",
                        mg_engine_get_vel(&inst->engine));

    if (strcmp(key, "sync_warn") == 0)
        return snprintf(buf, (size_t)buf_len, "ok");

    return -1;
}

/* =========================================================================
 * save_state / load_state
 * ========================================================================= */

static int save_state(void *instance, char *buf, int buf_len)
{
    mg_instance_t *inst = (mg_instance_t *)instance;
    if (!inst || !buf || buf_len <= 0) return 0;

    static const char *steps_names[] = {"4", "8", "16"};
    static const char *scale_names[] = {
        "ionian", "aeolian", "dorian", "mixolydian",
        "major_pent", "minor_pent", "suspended", "power",
        "phrygian", "lydian", "harmonic_minor", "blues"
    };
    static const char *range_names[] = {"close", "octave", "wide"};
    int steps_idx = mg_engine_get_steps_idx(&inst->engine);
    int scale_idx = mg_engine_get_scale(&inst->engine);
    int range_idx = mg_engine_get_range(&inst->engine);
    if (steps_idx < 0 || steps_idx > 2) steps_idx = 2;
    if (scale_idx < 0 || scale_idx >= MG_NUM_SCALES) scale_idx = MG_SCALE_IONIAN;
    if (range_idx < 0 || range_idx >= MG_NUM_RANGES) range_idx = MG_RANGE_CLOSE;

    return snprintf(buf, (size_t)buf_len,
        "root=%d\nsteps=%s\nscale=%s\nrange=%s\nspread=%.4f\ndensity=%.4f\nchaos=%.4f\nrest=%.4f\nresolve=%.4f\nswing=%.4f\ngate=%.4f\nvel=%d\n",
        mg_engine_get_root(&inst->engine),
        steps_names[steps_idx],
        scale_names[scale_idx],
        range_names[range_idx],
        mg_engine_get_spread(&inst->engine),
        mg_engine_get_density(&inst->engine),
        mg_engine_get_chaos(&inst->engine),
        mg_engine_get_rest(&inst->engine),
        mg_engine_get_resolve(&inst->engine),
        mg_engine_get_swing(&inst->engine),
        mg_engine_get_gate(&inst->engine),
        mg_engine_get_vel(&inst->engine));
}

static void load_state(void *instance, const char *buf, int buf_len)
{
    if (!instance || !buf || buf_len <= 0) return;

    char line[64];
    const char *p   = buf;
    const char *end = buf + buf_len;

    while (p < end) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        int len = nl ? (int)(nl - p) : (int)(end - p);

        if (len > 0 && len < (int)sizeof(line) - 1) {
            memcpy(line, p, (size_t)len);
            line[len] = '\0';
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                set_param(instance, line, eq + 1);
            }
        }

        p = nl ? nl + 1 : end;
    }
}

/* =========================================================================
 * Plugin vtable + entry point
 * ========================================================================= */

static midi_fx_api_v1_t g_api = {
    .api_version      = MIDI_FX_API_VERSION,
    .create_instance  = create_instance,
    .destroy_instance = destroy_instance,
    .process_midi     = process_midi,
    .tick             = tick,
    .set_param        = set_param,
    .get_param        = get_param,
    .save_state       = save_state,
    .load_state       = load_state,
};

midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host)
{
    g_host = host;   /* kept for optional log() calls */
    return &g_api;
}
