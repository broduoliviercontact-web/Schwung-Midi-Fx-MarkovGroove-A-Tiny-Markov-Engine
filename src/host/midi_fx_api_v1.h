/*
 * midi_fx_api_v1.h — Schwung MIDI FX Plugin Interface
 *
 * A Schwung MIDI FX module exposes one symbol:
 *   midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host);
 *
 * Called once at load time. Returns a pointer to a static vtable.
 * The host pointer must be stored as a module-level static for use in tick().
 */

#ifndef MIDI_FX_API_V1_H
#define MIDI_FX_API_V1_H

#include <stdint.h>

#define MIDI_FX_API_VERSION  1
#define MIDI_FX_MAX_OUT_MSGS 16

/* Forward declaration */
struct host_api_v1;

/*
 * midi_fx_api_v1_t — Plugin vtable
 *
 * Returned by move_midi_fx_init(). All function pointers must be set.
 */
typedef struct midi_fx_api_v1 {
    uint32_t api_version; /* must be MIDI_FX_API_VERSION */

    /*
     * create_instance — allocate per-instance state
     *
     * Called once per slot insert.
     * - Allocate with calloc(1, sizeof(YourInstance))
     * - Set ALL parameter defaults — do not rely on set_param being called at init
     * - Initialize running = 0 (never 1)
     * - Return NULL on allocation failure
     */
    void *(*create_instance)(const char *module_dir, const char *config_json);

    /*
     * destroy_instance — free per-instance state
     *
     * Called when the module is removed from the chain.
     * Implementation: free(instance)
     */
    void (*destroy_instance)(void *instance);

    /*
     * process_midi — handle one incoming MIDI message
     *
     * Called for each incoming MIDI message.
     * Returns the number of output messages written (0 to max_out).
     *
     * Rules:
     * - Pass through unrecognized messages by copying to out_msgs
     * - Handle velocity-0 note-on as note-off
     * - Track active notes for note-off safety
     * - Flush active notes on 0xFC (transport stop)
     * - Note: 0xFA/0xFB/0xFC transport bytes are NOT forwarded by the host
     *   to external plugins — handle them here for internal state if needed
     */
    int (*process_midi)(void *instance,
                        const uint8_t *in_msg, int in_len,
                        uint8_t out_msgs[][3], int out_lens[],
                        int max_out);

    /*
     * tick — called every audio buffer
     *
     * Called every audio block with the number of frames in the block.
     * Returns the number of output messages written.
     *
     * Rules:
     * - Use nframes to advance timing counters
     * - For transport-synced modules: use get_clock_status() in tick()
     * - For free-running modules: do NOT call get_clock_status() or get_bpm()
     * - Do not allocate or block in tick
     * - Emit scheduled MIDI events (steps, arpeggios, LFO CCs, etc.)
     */
    int (*tick)(void *instance,
                int nframes, int sample_rate,
                uint8_t out_msgs[][3], int out_lens[],
                int max_out);

    /*
     * set_param — set a parameter by key/value string
     *
     * val is always a string — parse with atof(), atoi(), or strcmp()
     * Validate and clamp before applying. Call engine setters.
     */
    void (*set_param)(void *instance, const char *key, const char *val);

    /*
     * get_param — read a parameter value into buf
     *
     * CRITICAL: must return snprintf(buf, buf_len, ...) — never return 0.
     * Returning 0 silently breaks param display, chain editing, and state recall.
     * Return -1 for unknown keys.
     *
     * Example:
     *   if (strcmp(key, "density") == 0)
     *       return snprintf(buf, buf_len, "%.4f", inst->density / 255.0f);
     *   return -1;
     */
    int (*get_param)(void *instance, const char *key, char *buf, int buf_len);

    /*
     * save_state / load_state — state serialization
     *
     * Format: simple key=value text, one per line.
     *   density=0.5000
     *   mode=drum
     *   steps=16
     *
     * save_state: write all playback-affecting params. Do NOT serialize transient
     *             note state (active notes, scheduled events).
     * load_state: parse each line, validate, apply with same logic as set_param.
     *             Handle missing keys gracefully — use defaults. Never crash.
     *
     * buf is a caller-allocated buffer of buf_len bytes.
     * Returns number of bytes written (save) or 0 (load).
     */
    int  (*save_state)(void *instance, char *buf, int buf_len);
    void (*load_state)(void *instance, const char *buf, int buf_len);

} midi_fx_api_v1_t;

/*
 * Entry point — exported symbol, called once at load time.
 *
 * Store the host pointer as a module-level static:
 *   static const host_api_v1_t *g_host = NULL;
 *   midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host) {
 *       g_host = host;
 *       return &g_api;
 *   }
 */
midi_fx_api_v1_t *move_midi_fx_init(const struct host_api_v1 *host);

#endif /* MIDI_FX_API_V1_H */
