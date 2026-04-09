# Build Move UI and Controls for Schwung Module

## Purpose
Use this skill to design and implement the Move-facing control surface for a Schwung module.

This includes:
- parameter-to-knob mapping
- shift behaviors
- pad behaviors
- step button behaviors
- LED feedback
- optional `ui.js`
- optional `ui_chain.js`

## Goal
Make the module feel native on Move.
The user should be able to understand and control the module quickly with minimal cognitive load.

## UI Design Principles
- Keep the first screen simple.
- Put the most important parameters on direct knobs.
- Use Shift only when it meaningfully improves control density.
- Use pads or steps only if they genuinely improve musical interaction.
- Prefer stable visual conventions over novelty.
- Reuse shared helpers and patterns from the repo.

## Hardware Reference

### Move MIDI Mapping

**Pads:** Notes 68–99 (32 pads, bottom-left to top-right, 4 rows of 8)
**Step buttons:** Notes 16–31
**Knob touch (capacitive):** Notes 0–9 — filter with `if (data[1] < 10) return;` unless `raw_midi` is set

| CC  | Control            | Notes                                |
|-----|--------------------|--------------------------------------|
| 3   | Jog wheel click    | 127 = pressed                        |
| 14  | Jog wheel rotate   | 1–63 = CW, 65–127 = CCW             |
| 40–43 | Track buttons   | CC43=Track1, CC40=Track4 (reversed)  |
| 49  | Shift              | 127 = held                           |
| 50  | Menu               |                                      |
| 51  | Back               |                                      |
| 54  | Down arrow         |                                      |
| 55  | Up arrow           |                                      |
| 62  | Left arrow         |                                      |
| 63  | Right arrow        |                                      |
| 71–78 | Knobs 1–8       | Relative (1–63 CW, 65–127 CCW)       |
| 79  | Master volume      | Relative encoder                     |
| 85  | Play               |                                      |
| 86  | Record             |                                      |
| 118 | Record button LED  | Use Note for input                   |
| 119 | Delete             |                                      |

### Display API (128×64, 1-bit monochrome)

```javascript
clear_screen()                    // Clear to black
print(x, y, text, color)          // color: 0=black, 1=white
set_pixel(x, y, value)
draw_rect(x, y, w, h, value)
fill_rect(x, y, w, h, value)
text_width(text)                   // Returns pixel width

// Object-oriented alternative:
display.clear()
display.drawText(x, y, text, color)
display.fillRect(x, y, w, h, value)
display.drawRect(x, y, w, h, value)
display.drawLine(x1, y1, x2, y2, value)
display.flush()
```

### MIDI Send

```javascript
// To external USB-A port:
move_midi_external_send([cable, status, data1, data2])

// To internal Move hardware (LEDs):
move_midi_internal_send([0x09, status, note, velocity])  // Note (LED)
move_midi_internal_send([0x0b, status, cc, value])       // CC (LED)
```

### LED Colors (from `constants.mjs`)

```javascript
const Black = 0;        const White = 120;     const LightGrey = 118;
const Red = 127;        const Blue = 125;      const BrightGreen = 8;
const BrightRed = 1;
```
Full palette: see `src/shared/constants.mjs`.

### LED Buffer Constraint
The hardware mailbox holds ~64 USB-MIDI packets. Sending more than ~60 LED commands per frame causes buffer overflow. Use progressive LED init:

```javascript
const LEDS_PER_FRAME = 8;
let ledInitIndex = 0;
let ledInitPending = true;

globalThis.tick = function() {
    if (ledInitPending) {
        const leds = [...]; // all LEDs to set
        const end = Math.min(ledInitIndex + LEDS_PER_FRAME, leds.length);
        for (let i = ledInitIndex; i < end; i++) setLED(leds[i].note, leds[i].color);
        ledInitIndex = end;
        if (ledInitIndex >= leds.length) ledInitPending = false;
    }
    drawUI();
};
```

### Host Functions Available in UI

```javascript
host_module_get_param(key)         // Get DSP parameter as string
host_module_set_param(key, val)    // Set DSP parameter
host_module_get_error()            // Get last module error message
host_module_send_midi(msg, source) // Inject MIDI into DSP (source: "internal"/"external"/"host")
host_is_module_loaded()            // Returns bool
host_get_volume()                  // 0–100
host_set_volume(vol)
host_get_setting(key)              // 'velocity_curve', 'aftertouch_enabled', 'aftertouch_deadzone'
host_return_to_menu()              // Exit to host menu
host_rescan_modules()              // Rescan modules directory
```

### Capability Flags Affecting UI Behavior

| Flag in `module.json` | Effect |
|-----------------------|--------|
| `"raw_midi": true` | Host skips MIDI transforms (velocity curve, aftertouch, knob-touch filter) |
| `"raw_ui": true` | Host won't intercept Back to return to menu; module must call `host_return_to_menu()` |
| `"claims_master_knob": true` | CC 79 (volume knob) routes to module instead of host volume |
| `"skip_led_clear": true` | Host skips LED clear on load/unload |

### Shared Helpers

```javascript
import { decodeDelta, decodeAcceleratedDelta, setLED, setButtonLED, clearAllLEDs }
    from '../../shared/input_filter.mjs';
import { MoveMainKnob, MoveShift, MoveMenu, MovePad1, MovePad32, MidiNoteOn, MidiCC }
    from '../../shared/constants.mjs';
```

## Hardware Awareness
Assume Move controls include:
- 8 relative encoders (knobs 1–8, CC 71–78)
- 32 pads (notes 68–99)
- 16 step buttons (notes 16–31)
- menu/back/shift buttons
- jog wheel (CC 14) + jog click (CC 3)
- LEDs on pads, steps, and buttons

Treat hardware noise and non-musical internal messages carefully.
Filter knob-touch notes (0–9) unless the module genuinely needs them.

## UI Design Process

### 1. Identify Core Controls
Choose the primary controls that deserve direct knob access.

Typical direct controls:
- mode
- rate/division
- range
- gate
- amount
- probability
- min/max
- channel
- scale
- transpose

### 2. Define Shift Layer
Use shift for:
- secondary value adjustment
- alternate interpretation
- quick reset
- fine/coarse resolution
- hidden but useful performance options

Do not hide critical functionality behind Shift unless necessary.

### 3. Define Pads / Step Buttons
Use them only if they add immediate value, such as:
- mode selection
- scale selection
- lane enable/disable
- step toggles
- octave selection
- chord slot recall

Otherwise leave them unused or decorative.

### 4. Define LED Behavior
Specify:
- status indication
- active mode indication
- selection feedback
- error/warning state
- timing or pulse feedback only if it helps

Avoid LED spam.
LEDs should communicate state, not create noise.

### 5. Chain UI Strategy
If a chain UI is needed:
- expose only the most important controls
- keep chain editing compact
- avoid interactions that depend on a full-screen module context unless clearly supported

## Required Output Format

### Interaction Summary
Describe how the user operates the module on Move.

### Knob Map
List knob 1 through knob 8.

### Shift Layer
List all shift-modified actions.

### Pad Map
If pads are used, define each zone or behavior.

### Step Button Map
If steps are used, define their meanings.

### LED Plan
Describe all visual feedback.

### Files
Specify whether to create:
- `ui.js`
- `ui_chain.js`
- neither

### Implementation Notes
Provide concrete notes for coding.

## Guardrails
- Do not create a UI that requires memorizing too many hidden states.
- Do not use pads just because they are available.
- Do not overload LEDs with decorative behavior.
- Keep chain UI smaller and more focused than full UI.
- Prefer consistency with existing modules over experimental interaction design.