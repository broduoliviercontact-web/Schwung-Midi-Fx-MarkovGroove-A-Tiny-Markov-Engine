/*
 * markov_groove_engine.h — MarkovGroove portable engine
 *
 * Pure C. No Schwung, Move, or platform-specific headers.
 * Testable on any machine with: gcc -std=c99 -Wall -Wextra -Werror
 *
 * Algorithm:
 *   - Walks a first-order Markov chain over 4 pitch states relative
 *     to a configurable root note, scale, and register range.
 *   - 'spread' fine-tunes how strongly upper-register states are favoured.
 *   - 'density' sets the overall proportion of sounded steps.
 *   - 'resolve' shapes transitions toward cadential motion back to root.
 *   - 'rest' shapes how silences cluster into short gaps or longer breaths.
 *   - Rested steps lightly accent the next sounded note.
 *   - Applies Ableton-style swing: odd steps delayed by swing_f * 0.5 * step_duration.
 *   - Velocity of swing-delayed (odd) steps is attenuated by swing_f * 0.3.
 *   - Timing driven externally (BPM + sample_rate passed into tick).
 */

#ifndef MARKOV_GROOVE_ENGINE_H
#define MARKOV_GROOVE_ENGINE_H

#include <stdint.h>

/* --- Markov state indices ------------------------------------------------- */
#define MG_STATE_ROOT  0
#define MG_STATE_3RD   1
#define MG_STATE_5TH   2
#define MG_STATE_OCT   3
#define MG_NUM_STATES  4

/* --- Steps enum indices --------------------------------------------------- */
#define MG_STEPS_4     0   /* quarter notes — 4 per bar  */
#define MG_STEPS_8     1   /* eighth notes  — 8 per bar  */
#define MG_STEPS_16    2   /* sixteenth notes — 16 per bar */

/* --- Scale choices -------------------------------------------------------- */
#define MG_SCALE_IONIAN         0
#define MG_SCALE_AEOLIAN        1
#define MG_SCALE_DORIAN         2
#define MG_SCALE_MIXOLYDIAN     3
#define MG_SCALE_MAJOR_PENT     4
#define MG_SCALE_MINOR_PENT     5
#define MG_SCALE_SUSPENDED      6
#define MG_SCALE_POWER          7
#define MG_SCALE_PHRYGIAN       8
#define MG_SCALE_LYDIAN         9
#define MG_SCALE_HARM_MINOR     10
#define MG_SCALE_BLUES          11
#define MG_NUM_SCALES           12

/* --- Register range ------------------------------------------------------- */
#define MG_RANGE_CLOSE          0
#define MG_RANGE_OCTAVE         1
#define MG_RANGE_WIDE           2
#define MG_NUM_RANGES           3

/* --- MIDI channel for generated notes (0-indexed) ------------------------- */
#define MG_OUTPUT_CHANNEL  0   /* MIDI channel 1 */

/* --- Sentinel for "no active note" --------------------------------------- */
#define MG_NO_NOTE  255

/* --- Per-instance engine state ------------------------------------------- */
typedef struct {
    /* Parameters (encoded) */
    int8_t  root;       /* -24..24: signed semitone offset; generated note = 60 + root + interval */
    uint8_t steps_idx;  /* MG_STEPS_4 / MG_STEPS_8 / MG_STEPS_16 */
    uint8_t scale;      /* MG_SCALE_* note palette */
    uint8_t range;      /* MG_RANGE_* octave spread */
    uint8_t spread;     /* 0–255 → 0.0–1.0: extra preference for upper register states */
    uint8_t density;    /* 0–255 → 0.0–1.0: overall note density */
    uint8_t chaos;      /* 0–255 → 0.0–1.0: controls Markov jump probability */
    uint8_t rest;       /* 0–255 → 0.0–1.0: rest clustering / breath shaping */
    uint8_t resolve;    /* 0–255 → 0.0–1.0: bias back toward root state */
    uint8_t swing;      /* 0–255 → 0.0–1.0: odd-step delay & velocity attenuation */
    uint8_t gate;       /* 0–255 → 0.0–1.0: note duration as fraction of step */
    uint8_t vel;        /* 20–127: base MIDI velocity */

    /* Markov chain runtime */
    uint8_t  mk_state;   /* current state: MG_STATE_* */
    uint32_t rng;        /* LCG state; initialised to a fixed non-zero seed */

    /* Step timing */
    double   frames_per_step;  /* recomputed each tick from BPM */
    double   frames_accum;     /* frames elapsed since last step boundary */
    uint32_t step_count;       /* total steps emitted; parity drives swing */
    uint8_t  rest_streak;      /* consecutive rested steps, used for re-entry accent */

    /* Active note tracking */
    uint8_t  active_note;        /* MG_NO_NOTE when silent */
    uint8_t  active_on;          /* 1 = note is currently sounding */
    double   frames_until_off;   /* frames until scheduled note-off (0 = none pending) */

    /* Swing-delayed note-on (odd steps when swing > 0) */
    uint8_t  swing_pending;   /* 1 = a note-on is waiting for swing delay */
    uint8_t  swing_note;      /* note to fire */
    uint8_t  swing_vel;       /* attenuated velocity */
    double   swing_frames;    /* frames remaining until note-on fires */
} mg_engine_t;

/* --- Engine API ----------------------------------------------------------- */

/*
 * mg_engine_init — initialise instance to defaults.
 * Must be called once after allocation. Sets all parameter defaults
 * and puts the engine in a clean stopped state.
 */
void mg_engine_init(mg_engine_t *e);

/*
 * mg_engine_reset — reset timing & pending state.
 * Call after transport stop (after flushing active notes with mg_engine_flush).
 * Does NOT emit any MIDI. Does NOT reset parameters.
 */
void mg_engine_reset(mg_engine_t *e);

/*
 * mg_engine_flush — emit note-offs for any sounding note.
 * Returns number of messages written (0 or 1).
 * Call before reset or state change that would leave a note open.
 */
int mg_engine_flush(mg_engine_t *e,
                    uint8_t out_msgs[][3], int out_lens[], int max_out);

/*
 * mg_engine_tick — advance engine by nframes, emit scheduled MIDI.
 *
 * bpm:         current BPM from host (clamped 20–300 before calling)
 * sample_rate: host sample rate
 * out_msgs:    caller-allocated output array
 * Returns:     number of messages written (0 to 3 per call)
 *
 * May emit: swing note-on, time-based note-off, step note-off + note-on.
 * Never emits more than 3 messages per call.
 */
int mg_engine_tick(mg_engine_t *e,
                   int nframes, float bpm, int sample_rate,
                   uint8_t out_msgs[][3], int out_lens[], int max_out);

/* --- Parameter setters --------------------------------------------------- */
void mg_engine_set_root     (mg_engine_t *e, int root);       /* clamps -24..24 */
void mg_engine_set_steps    (mg_engine_t *e, int steps_idx);  /* MG_STEPS_* */
void mg_engine_set_scale    (mg_engine_t *e, int scale);      /* MG_SCALE_* */
void mg_engine_set_range    (mg_engine_t *e, int range);      /* MG_RANGE_* */
void mg_engine_set_spread   (mg_engine_t *e, float spread);   /* clamps 0.0–1.0 */
void mg_engine_set_density  (mg_engine_t *e, float density);  /* clamps 0.0–1.0 */
void mg_engine_set_chaos    (mg_engine_t *e, float chaos);    /* clamps 0.0–1.0 */
void mg_engine_set_rest     (mg_engine_t *e, float rest);     /* clamps 0.0–1.0 */
void mg_engine_set_resolve  (mg_engine_t *e, float resolve);  /* clamps 0.0–1.0 */
void mg_engine_set_swing    (mg_engine_t *e, float swing);    /* clamps 0.0–1.0 */
void mg_engine_set_gate     (mg_engine_t *e, float gate);     /* clamps 0.0–1.0 */
void mg_engine_set_vel      (mg_engine_t *e, int vel);        /* clamps 20–127 */

/* --- Parameter getters --------------------------------------------------- */
int   mg_engine_get_root     (const mg_engine_t *e);
int   mg_engine_get_steps_idx(const mg_engine_t *e);
int   mg_engine_get_scale    (const mg_engine_t *e);
int   mg_engine_get_range    (const mg_engine_t *e);
float mg_engine_get_spread   (const mg_engine_t *e);
float mg_engine_get_density  (const mg_engine_t *e);
float mg_engine_get_chaos    (const mg_engine_t *e);
float mg_engine_get_rest     (const mg_engine_t *e);
float mg_engine_get_resolve  (const mg_engine_t *e);
float mg_engine_get_swing    (const mg_engine_t *e);
float mg_engine_get_gate     (const mg_engine_t *e);
int   mg_engine_get_vel      (const mg_engine_t *e);

#endif /* MARKOV_GROOVE_ENGINE_H */
