/*
 * markov_groove_engine.c — MarkovGroove portable engine implementation
 *
 * No platform dependencies. Testable on any machine.
 *
 * Markov chain:
 *   4 states mapped to intervals defined by the selected scale.
 *   Transition weights shaped by 'chaos' parameter (0=melodic, 1=erratic),
 *   'resolve' bias toward the root state, and 'spread' bias toward higher
 *   register states selected by 'range'.
 *   Step occupancy is controlled by 'density'; 'rest' shapes whether
 *   silences are isolated skips or grouped breaths.
 *   RNG: minimal LCG (no stdlib random needed).
 *
 * Swing:
 *   Odd steps delayed by swing_f * 0.5 * frames_per_step.
 *   Odd-step velocity attenuated by (1 - swing_f * 0.3).
 *   Model matches Ableton's swing formula from groove.py.
 *
 * Timing model:
 *   frames_per_step = sample_rate * 60.0 / (bpm * steps_divisor)
 *   where steps_divisor: 4-steps=1.0, 8-steps=2.0, 16-steps=4.0.
 *   Accumulated via frames_accum double counter; step boundaries checked
 *   once per tick(). Max timing error: one audio buffer (~3–12 ms at
 *   typical BPMs). Acceptable for V1.
 */

#include "markov_groove_engine.h"

#include <stddef.h>   /* NULL */

/* --- LCG random number generator ----------------------------------------- */
/* Park-Miller constants. State must never be 0. */
static uint32_t lcg_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* --- Markov transition table ---------------------------------------------- */
/*
 * T[from][to] = {base_weight, chaos_bonus}
 *
 * effective_weight = base + chaos_w * bonus
 * chaos_w = round(chaos / 255.0 * 4.0), range 0–4
 *
 * At chaos_w=0: oct state unreachable; movement stays within root/3rd/5th.
 * At chaos_w=4: octave jumps become significant; distribution flattens.
 */
static const int8_t T[MG_NUM_STATES][MG_NUM_STATES][2] = {
    /* from root */ { {2,0}, {3,0}, {2,1}, {0,2} },
    /* from 3rd  */ { {3,1}, {1,0}, {3,0}, {0,1} },
    /* from 5th  */ { {3,0}, {1,1}, {1,0}, {0,2} },
    /* from oct  */ { {4,0}, {1,1}, {2,0}, {0,1} },
};

/* Semitone intervals for each scale/state, relative to root+60. */
static const int8_t scale_intervals[MG_NUM_SCALES][MG_NUM_STATES] = {
    /* ionian        */ {0, 4, 7, 12},
    /* aeolian       */ {0, 3, 7, 12},
    /* dorian        */ {0, 3, 7, 10},
    /* mixolydian    */ {0, 4, 7, 10},
    /* major pent    */ {0, 4, 7, 14},
    /* minor pent    */ {0, 3, 7, 10},
    /* suspended     */ {0, 5, 7, 12},
    /* power         */ {0, 7, 12, 19},
    /* phrygian      */ {0, 1, 7, 10},
    /* lydian        */ {0, 4, 6, 11},
    /* harm minor    */ {0, 3, 7, 11},
    /* blues         */ {0, 3, 6, 10},
};

/* Steps divisors: converts BPM to step duration for each steps_idx. */
static const float steps_divisors[3] = {1.0f, 2.0f, 4.0f};

/* Octave offsets applied on top of the scale intervals. */
static const int8_t range_offsets[MG_NUM_RANGES][MG_NUM_STATES] = {
    /* close  */ {0, 0, 0, 0},
    /* octave */ {0, 0, 0, 12},
    /* wide   */ {0, 0, 12, 12},
};

static int metric_is_strong_step(const mg_engine_t *e)
{
    int steps_per_beat = 1;

    if (e->steps_idx == MG_STEPS_8) steps_per_beat = 2;
    else if (e->steps_idx == MG_STEPS_16) steps_per_beat = 4;

    return ((int)e->step_count % steps_per_beat) == 0;
}

static int step_should_rest(mg_engine_t *e)
{
    float density_p;
    float rest_shape;
    float rest_prob;
    int streak;
    uint32_t roll;

    density_p = (float)e->density / 255.0f;
    rest_shape = (float)e->rest / 255.0f;
    rest_prob = 1.0f - density_p;

    if (rest_prob <= 0.0f) return 0;
    if (rest_prob >= 1.0f) return 1;

    streak = (int)e->rest_streak;
    if (streak > 4) streak = 4;

    if (streak == 0) {
        rest_prob *= 1.0f - rest_shape * 0.65f;
        if (metric_is_strong_step(e))
            rest_prob *= 0.85f;
    } else {
        rest_prob += (1.0f - rest_prob) * (0.20f + rest_shape * 0.55f);
        rest_prob += (float)streak * (0.03f + rest_shape * 0.02f);
    }

    if (rest_prob < 0.0f) rest_prob = 0.0f;
    if (rest_prob > 0.98f) rest_prob = 0.98f;

    roll = lcg_next(&e->rng) & 0xFFu;
    return roll < (uint32_t)(rest_prob * 255.0f + 0.5f);
}

static uint8_t apply_reentry_accent(mg_engine_t *e, uint8_t vel)
{
    int streak;
    int boosted;
    float rest_shape;

    if (e->rest_streak == 0) return vel;

    streak = (int)e->rest_streak;
    if (streak > 4) streak = 4;
    rest_shape = (float)e->rest / 255.0f;

    boosted = (int)vel + 4 + streak * (2 + (int)(rest_shape * 4.0f + 0.5f));
    if (boosted > 127) boosted = 127;

    e->rest_streak = 0;
    return (uint8_t)boosted;
}

/* --- Internal: walk the Markov chain, return MIDI note number ------------- */
static uint8_t markov_step(mg_engine_t *e)
{
    int chaos_w = (int)(e->chaos / 255.0f * 4.0f + 0.5f);
    int resolve_w = (int)(e->resolve / 255.0f * 6.0f + 0.5f);
    int spread_w = (int)(e->spread / 255.0f * 5.0f + 0.5f);
    int cadence_w = metric_is_strong_step(e) ? (resolve_w / 2 + 1) : 0;
    if (chaos_w > 4) chaos_w = 4;

    int from = (int)e->mk_state;
    int scale = (e->scale < MG_NUM_SCALES) ? (int)e->scale : MG_SCALE_IONIAN;
    int range = (e->range < MG_NUM_RANGES) ? (int)e->range : MG_RANGE_CLOSE;
    int weights[MG_NUM_STATES];
    int total = 0;

    for (int i = 0; i < MG_NUM_STATES; i++) {
        weights[i] = (int)T[from][i][0] + (int)T[from][i][1] * chaos_w;
    }

    if (resolve_w > 0) {
        switch (from) {
            case MG_STATE_ROOT:
                weights[MG_STATE_ROOT] += resolve_w / 2 + cadence_w;
                weights[MG_STATE_OCT]  -= resolve_w / 2;
                break;

            case MG_STATE_3RD:
                weights[MG_STATE_ROOT] += resolve_w * 2 + cadence_w;
                weights[MG_STATE_5TH]  += resolve_w / 2;
                weights[MG_STATE_OCT]  -= resolve_w;
                break;

            case MG_STATE_5TH:
                weights[MG_STATE_ROOT] += resolve_w * 2 + cadence_w;
                weights[MG_STATE_3RD]  += resolve_w;
                weights[MG_STATE_OCT]  -= resolve_w;
                break;

            case MG_STATE_OCT:
                weights[MG_STATE_ROOT] += resolve_w * 3 + cadence_w;
                weights[MG_STATE_5TH]  += resolve_w;
                weights[MG_STATE_3RD]  += resolve_w / 2;
                weights[MG_STATE_OCT]  -= resolve_w * 2;
                break;
        }
    }

    switch (range) {
        case MG_RANGE_CLOSE:
            weights[MG_STATE_ROOT] += 1;
            weights[MG_STATE_OCT]  -= 2;
            if (spread_w > 0) {
                weights[MG_STATE_5TH] += spread_w;
                weights[MG_STATE_OCT] += spread_w / 2;
                weights[MG_STATE_ROOT] -= spread_w / 2;
            }
            break;

        case MG_RANGE_OCTAVE:
            if (spread_w > 0) {
                weights[MG_STATE_5TH] += spread_w / 2;
                weights[MG_STATE_OCT] += spread_w * 2;
                weights[MG_STATE_ROOT] -= spread_w / 2;
            }
            break;

        case MG_RANGE_WIDE:
            weights[MG_STATE_5TH] += 1;
            weights[MG_STATE_OCT] += 2;
            if (spread_w > 0) {
                weights[MG_STATE_5TH] += spread_w;
                weights[MG_STATE_OCT] += spread_w * 2;
                weights[MG_STATE_ROOT] -= spread_w;
            }
            break;

        default:
            break;
    }

    for (int i = 0; i < MG_NUM_STATES; i++) {
        if (weights[i] < 0) weights[i] = 0;
        total += weights[i];
    }

    /* Weighted random selection. total is always > 0 (base weights ensure it). */
    int roll = (int)(lcg_next(&e->rng) % (uint32_t)total);
    int next_state = from;   /* safe fallback */
    int accum = 0;

    for (int i = 0; i < MG_NUM_STATES; i++) {
        accum += weights[i];
        if (roll < accum) {
            next_state = i;
            break;
        }
    }

    e->mk_state = (uint8_t)next_state;

    int note = 60 + (int)e->root
               + (int)scale_intervals[scale][next_state]
               + (int)range_offsets[range][next_state];
    if (note < 0)   note = 0;
    if (note > 127) note = 127;
    return (uint8_t)note;
}

/* --- Internal: write a note-off for the active note ----------------------- */
static int emit_note_off(mg_engine_t *e,
                         uint8_t out_msgs[][3], int out_lens[], int max_out,
                         int n_out)
{
    if (!e->active_on || n_out >= max_out) return n_out;
    out_msgs[n_out][0] = (uint8_t)(0x80 | MG_OUTPUT_CHANNEL);
    out_msgs[n_out][1] = e->active_note;
    out_msgs[n_out][2] = 0;
    out_lens[n_out] = 3;
    e->active_on       = 0;
    e->active_note     = MG_NO_NOTE;
    e->frames_until_off = 0.0;
    return n_out + 1;
}

/* --- Internal: write a note-on, schedule note-off ------------------------- */
static int emit_note_on(mg_engine_t *e,
                        uint8_t note, uint8_t vel,
                        uint8_t out_msgs[][3], int out_lens[], int max_out,
                        int n_out)
{
    if (n_out >= max_out) return n_out;
    out_msgs[n_out][0] = (uint8_t)(0x90 | MG_OUTPUT_CHANNEL);
    out_msgs[n_out][1] = note;
    out_msgs[n_out][2] = vel;
    out_lens[n_out] = 3;
    e->active_note      = note;
    e->active_on        = 1;
    e->frames_until_off = e->frames_per_step * ((double)e->gate / 255.0);
    if (e->frames_until_off < 1.0) e->frames_until_off = 1.0;
    return n_out + 1;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

void mg_engine_init(mg_engine_t *e)
{
    /* Parameters — V1 defaults */
    e->root       = 0;          /* C4 anchor */
    e->steps_idx  = MG_STEPS_16;
    e->scale      = MG_SCALE_IONIAN;
    e->range      = MG_RANGE_CLOSE;
    e->spread     = 0;
    e->density    = 255;
    e->chaos      = (uint8_t)(0.3f * 255.0f + 0.5f);   /* 0.3 */
    e->rest       = 0;
    e->resolve    = 0;
    e->swing      = 0;          /* straight */
    e->gate       = (uint8_t)(0.5f * 255.0f + 0.5f);   /* 0.5 */
    e->vel        = 90;

    /* Markov state */
    e->mk_state = MG_STATE_ROOT;
    e->rng      = 0x12345678u;  /* fixed seed: deterministic, non-zero */

    /* Timing */
    e->frames_per_step = 5512.5;   /* 120 BPM, 16ths, 44100 Hz — updated in tick */
    e->frames_accum    = 0.0;
    e->step_count      = 0;
    e->rest_streak     = 0;

    /* Note state */
    e->active_note      = MG_NO_NOTE;
    e->active_on        = 0;
    e->frames_until_off = 0.0;

    /* Swing pending */
    e->swing_pending = 0;
    e->swing_note    = MG_NO_NOTE;
    e->swing_vel     = 0;
    e->swing_frames  = 0.0;
}

void mg_engine_reset(mg_engine_t *e)
{
    /* Timing only — parameters unchanged, Markov state unchanged */
    e->frames_accum     = 0.0;
    e->step_count       = 0;
    e->rest_streak      = 0;
    e->frames_until_off = 0.0;
    e->swing_pending    = 0;
    e->swing_frames     = 0.0;
    /* active_note/active_on cleared by flush before reset */
}

int mg_engine_flush(mg_engine_t *e,
                    uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    int n = 0;
    /* Discard any pending swing note — it has not sounded yet */
    e->swing_pending = 0;
    /* Send note-off for any currently sounding note */
    n = emit_note_off(e, out_msgs, out_lens, max_out, n);
    return n;
}

int mg_engine_tick(mg_engine_t *e,
                   int nframes, float bpm, int sample_rate,
                   uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    int n_out = 0;

    /* Recompute step duration from current BPM each tick.
       This keeps tempo changes smooth without a reset. */
    {
        float div = steps_divisors[e->steps_idx < 3 ? e->steps_idx : 2];
        e->frames_per_step = (double)sample_rate * 60.0 / ((double)bpm * (double)div);
    }

    float swing_f = (float)e->swing / 255.0f;

    /* ---------------------------------------------------------------
     * 1. Swing-delayed note-on
     *    An odd step queued a note-on; fire it when delay expires.
     * --------------------------------------------------------------- */
    if (e->swing_pending) {
        e->swing_frames -= (double)nframes;
        if (e->swing_frames <= 0.0) {
            n_out = emit_note_on(e, e->swing_note, e->swing_vel,
                                 out_msgs, out_lens, max_out, n_out);
            e->swing_pending = 0;
        }
    }

    /* ---------------------------------------------------------------
     * 2. Time-based note-off
     *    Note has been sounding long enough (gate duration expired).
     * --------------------------------------------------------------- */
    if (e->active_on && e->frames_until_off > 0.0) {
        e->frames_until_off -= (double)nframes;
        if (e->frames_until_off <= 0.0) {
            n_out = emit_note_off(e, out_msgs, out_lens, max_out, n_out);
        }
    }

    /* ---------------------------------------------------------------
     * 3. Step boundary
     *    Advance step counter; walk Markov; emit next note.
     * --------------------------------------------------------------- */
    e->frames_accum += (double)nframes;

    if (e->frames_accum >= e->frames_per_step) {
        e->frames_accum -= e->frames_per_step;

        /* Cancel any lingering swing pending — next step has arrived.
           The swing-delayed note has been superseded. */
        e->swing_pending = 0;

        /* Cut the active note early (step boundary = hard gate). */
        n_out = emit_note_off(e, out_msgs, out_lens, max_out, n_out);

        /* Walk Markov chain to determine next pitch. */
        uint8_t new_note = markov_step(e);
        int     is_odd   = (int)(e->step_count & 1u);
        e->step_count++;

        if (step_should_rest(e)) {
            if (e->rest_streak < 255) e->rest_streak++;
            /* Silence this step, but preserve Markov movement. */
        } else if (is_odd && swing_f > 0.005f) {
            /* Swing: delay this note-on.
               swing_delay = swing_f * 0.5 * frames_per_step
               matches: offset = (percent/100 - 0.5) * pair_duration
               from groove.py with percent = 50 + swing_f * 25. */
            uint8_t played_vel = apply_reentry_accent(e, e->vel);
            uint8_t sv = (uint8_t)((float)played_vel * (1.0f - swing_f * 0.3f));
            if (sv < 1) sv = 1;
            e->swing_note    = new_note;
            e->swing_vel     = sv;
            e->swing_frames  = e->frames_per_step * (double)swing_f * 0.5;
            e->swing_pending = 1;
        } else {
            /* Even step, or swing disabled: fire immediately. */
            uint8_t played_vel = apply_reentry_accent(e, e->vel);
            n_out = emit_note_on(e, new_note, played_vel,
                                 out_msgs, out_lens, max_out, n_out);
        }
    }

    return n_out;
}

/* --- Parameter setters --------------------------------------------------- */

void mg_engine_set_root(mg_engine_t *e, int root)
{
    if (root < -24) root = -24;
    if (root > 24)  root = 24;
    e->root = (int8_t)root;
}

void mg_engine_set_steps(mg_engine_t *e, int steps_idx)
{
    if (steps_idx < 0) steps_idx = 0;
    if (steps_idx > 2) steps_idx = 2;
    e->steps_idx = (uint8_t)steps_idx;
    /* Reset timing so step count stays consistent after density change. */
    e->frames_accum  = 0.0;
    e->swing_pending = 0;
}

void mg_engine_set_scale(mg_engine_t *e, int scale)
{
    if (scale < 0) scale = 0;
    if (scale >= MG_NUM_SCALES) scale = MG_NUM_SCALES - 1;
    e->scale = (uint8_t)scale;
}

void mg_engine_set_range(mg_engine_t *e, int range)
{
    if (range < 0) range = 0;
    if (range >= MG_NUM_RANGES) range = MG_NUM_RANGES - 1;
    e->range = (uint8_t)range;
}

void mg_engine_set_spread(mg_engine_t *e, float spread)
{
    if (spread < 0.0f) spread = 0.0f;
    if (spread > 1.0f) spread = 1.0f;
    e->spread = (uint8_t)(spread * 255.0f + 0.5f);
}

void mg_engine_set_density(mg_engine_t *e, float density)
{
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;
    e->density = (uint8_t)(density * 255.0f + 0.5f);
}

void mg_engine_set_chaos(mg_engine_t *e, float chaos)
{
    if (chaos < 0.0f) chaos = 0.0f;
    if (chaos > 1.0f) chaos = 1.0f;
    e->chaos = (uint8_t)(chaos * 255.0f + 0.5f);
}

void mg_engine_set_rest(mg_engine_t *e, float rest)
{
    if (rest < 0.0f) rest = 0.0f;
    if (rest > 1.0f) rest = 1.0f;
    e->rest = (uint8_t)(rest * 255.0f + 0.5f);
}

void mg_engine_set_resolve(mg_engine_t *e, float resolve)
{
    if (resolve < 0.0f) resolve = 0.0f;
    if (resolve > 1.0f) resolve = 1.0f;
    e->resolve = (uint8_t)(resolve * 255.0f + 0.5f);
}

void mg_engine_set_swing(mg_engine_t *e, float swing)
{
    if (swing < 0.0f) swing = 0.0f;
    if (swing > 1.0f) swing = 1.0f;
    e->swing = (uint8_t)(swing * 255.0f + 0.5f);
}

void mg_engine_set_gate(mg_engine_t *e, float gate)
{
    if (gate < 0.0f) gate = 0.0f;
    if (gate > 1.0f) gate = 1.0f;
    e->gate = (uint8_t)(gate * 255.0f + 0.5f);
}

void mg_engine_set_vel(mg_engine_t *e, int vel)
{
    if (vel < 20)  vel = 20;
    if (vel > 127) vel = 127;
    e->vel = (uint8_t)vel;
}

/* --- Parameter getters --------------------------------------------------- */

int   mg_engine_get_root     (const mg_engine_t *e) { return (int)e->root; }
int   mg_engine_get_steps_idx(const mg_engine_t *e) { return (int)e->steps_idx; }
int   mg_engine_get_scale    (const mg_engine_t *e) { return (int)e->scale; }
int   mg_engine_get_range    (const mg_engine_t *e) { return (int)e->range; }
float mg_engine_get_spread   (const mg_engine_t *e) { return (float)e->spread / 255.0f; }
float mg_engine_get_density  (const mg_engine_t *e) { return (float)e->density / 255.0f; }
float mg_engine_get_chaos    (const mg_engine_t *e) { return (float)e->chaos / 255.0f; }
float mg_engine_get_rest     (const mg_engine_t *e) { return (float)e->rest / 255.0f; }
float mg_engine_get_resolve  (const mg_engine_t *e) { return (float)e->resolve / 255.0f; }
float mg_engine_get_swing    (const mg_engine_t *e) { return (float)e->swing / 255.0f; }
float mg_engine_get_gate     (const mg_engine_t *e) { return (float)e->gate  / 255.0f; }
int   mg_engine_get_vel      (const mg_engine_t *e) { return (int)e->vel; }
