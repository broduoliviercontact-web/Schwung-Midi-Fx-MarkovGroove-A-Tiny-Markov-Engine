/*
 * plugin_api_v1.h — Schwung Host API
 *
 * The host_api_v1_t pointer is passed to move_midi_fx_init().
 * Store it as a module-level static and use it in tick() and process_midi().
 *
 * IMPORTANT: Free-running modules must NOT call get_clock_status() or get_bpm().
 * This causes SIGSEGV on some Move firmware versions. Use a constant BPM instead.
 */

#ifndef PLUGIN_API_V1_H
#define PLUGIN_API_V1_H

#include <stdint.h>

/* Clock status returned by get_clock_status() */
#define MOVE_CLOCK_STATUS_UNAVAILABLE 0  /* MIDI Clock Out not enabled in Move settings */
#define MOVE_CLOCK_STATUS_STOPPED     1  /* transport stopped */
#define MOVE_CLOCK_STATUS_RUNNING     2  /* transport running */

/*
 * Modulation routing — advanced use only.
 * Emit a modulation value from a source slot in the chain.
 */
typedef void (*move_mod_emit_value_fn)(void *ctx, int source_id, float value);
typedef void (*move_mod_clear_source_fn)(void *ctx, int source_id);

/*
 * host_api_v1_t — host services available to a plugin
 */
typedef struct host_api_v1 {
    uint32_t api_version;   /* host API version */
    int      sample_rate;   /* current sample rate (typically 44100 or 48000) */
    int      frames_per_block; /* nominal block size */

    /*
     * log — write a debug message to the host log
     * Use sparingly in production code. Do not call from tick() on every block.
     */
    void (*log)(const char *msg);

    /*
     * get_clock_status — returns MOVE_CLOCK_STATUS_* value
     *
     * Returns UNAVAILABLE when "MIDI Clock Out" is not enabled in
     * Move Settings → MIDI → MIDI Clock Out.
     *
     * Treat UNAVAILABLE the same as STOPPED.
     *
     * Brindille pattern for tick():
     *   int status = g_host->get_clock_status();
     *   if (status == MOVE_CLOCK_STATUS_STOPPED ||
     *       status == MOVE_CLOCK_STATUS_UNAVAILABLE)
     *       inst->running = 0;
     *   else if (status == MOVE_CLOCK_STATUS_RUNNING)
     *       inst->running = 1;
     *
     * DO NOT call this from free-running modules — SIGSEGV on some firmware.
     */
    int (*get_clock_status)(void);

    /*
     * get_bpm — returns current Move BPM as a float
     *
     * Only valid when get_clock_status() returns RUNNING.
     * DO NOT call this from free-running modules — SIGSEGV on some firmware.
     */
    float (*get_bpm)(void);

    /*
     * midi_send_internal — send a MIDI message to internal Move routing
     * midi_send_external — send a MIDI message to external MIDI output
     *
     * These are for side-channel MIDI (e.g. visual feedback, clock forwarding).
     * Normal plugin output should go through the out_msgs array in process_midi/tick.
     */
    int (*midi_send_internal)(const uint8_t *msg, int len);
    int (*midi_send_external)(const uint8_t *msg, int len);

    /* Modulation routing — advanced use */
    move_mod_emit_value_fn  mod_emit_value;
    move_mod_clear_source_fn mod_clear_source;
    void                   *mod_host_ctx;

} host_api_v1_t;

#endif /* PLUGIN_API_V1_H */
