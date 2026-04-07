# MarkovGroove

MarkovGroove is a chainable `midi_fx` module for **Schwung on Ableton Move**.

It generates melodic phrases from a tiny Markov engine, then lets you shape the result with a small set of performance-friendly controls: scale, range, spread, density, chaos, resolve, swing, and rest.

Think of it as a sequencer with a memory for where it has been, but not a fixed obligation to repeat itself.

## Why It Exists

A lot of generative tools are either:

- too random to feel playable
- too rigid to feel alive
- too deep to be fun on a hardware box

MarkovGroove aims for the middle:

- immediate enough for hands-on tweaking on Move
- musical enough to land on useful phrases quickly
- unstable enough to stay interesting for more than one bar

It is especially good at:

- melodic loops that keep moving without collapsing into noise
- pseudo-arps that feel more human than a strict up/down arp
- bass and lead phrases that can drift, snap back, breathe, and swing

## Features

- Chainable MIDI FX module for Schwung
- Syncs from MIDI transport and MIDI clock
- Passes incoming non-transport MIDI through unchanged
- Markov-driven note selection with scale-aware output
- Playable register shaping via `range` and `spread`
- Phrase stability control via `chaos` and `resolve`
- Groove shaping via `density`, `rest`, `swing`, `gate`, and `vel`
- Compact control surface designed for Move
- Native C engine with automated tests

## Quick Start

### Prerequisites

- [Schwung](https://github.com/charlesvestal/move-anything) installed on your Ableton Move
- SSH enabled on Move: `http://move.local/development/ssh`
- A local C toolchain for tests
- For Move builds: `aarch64-linux-gnu-gcc`, `aarch64-linux-musl-gcc`, or `zig`

### Build And Install

```bash
make test
./scripts/build.sh native
./scripts/install.sh move.local
```

If the module does not appear immediately:

```bash
ssh root@move.local 'pkill -x schwung || true; sleep 1; nohup /data/UserData/schwung/schwung >/tmp/schwung.log 2>&1 </dev/null &'
```

## How To Use It

1. Insert **MarkovGroove** in a Schwung MIDI FX slot.
2. Start Move transport so MIDI clock and transport are running.
3. Route the chain into a synth, sampler, or drum rack that responds to MIDI notes.
4. Start with a simple sound.
5. Turn one control at a time until the phrase starts behaving like a bandmate instead of a metronome.

## The Core Idea

MarkovGroove walks between a few note states inside the selected scale.

Each new step asks:

- should I play or stay silent?
- if I play, which scale degree should come next?
- should I stay near home or wander?
- should I stay compact or jump upward?

That is where the parameters come in.

## Live Controls

By default, the 8 main knobs are:

- `root`
- `scale`
- `range`
- `spread`
- `density`
- `chaos`
- `resolve`
- `rest`

Additional parameters available in the full parameter list:

- `swing`
- `steps`
- `gate`
- `vel`

## Parameter Guide

This section is written for actual tweaking, not just for reference.

### `root`

**What it does**

Transposes the whole generator in semitones.

**Range**

`-24` to `24`

**Use it when**

- the phrase is in the wrong register
- you want to drop the whole thing down for bass duties
- you want to keep the same behavior but move the harmony

**Good to know**

- `0` keeps the current center
- negative values transpose down
- `-12` is one octave below the center
- `-24` is two octaves below the center

**Quick feel**

If the pattern sounds clever but too polite, try `root = -12` or `root = -24` with a round bass patch.

### `scale`

**What it does**

Chooses the pitch vocabulary used by the Markov engine.

**Options**

- `ionian`
- `aeolian`
- `dorian`
- `mixolydian`
- `major_pent`
- `minor_pent`
- `suspended`
- `power`
- `phrygian`
- `lydian`
- `harmonic_minor`
- `blues`

**Use it when**

- you want to change the emotional color without rebuilding the groove
- you want fewer wrong-looking notes from a busy phrase
- you want something more riff-like than harmonic

**Quick guide**

- `ionian`: bright, stable, classic major
- `aeolian`: darker, more moody
- `dorian`: minor, but a bit hopeful
- `mixolydian`: major with a looser, funkier pull
- `major_pent`: forgiving and pop-friendly
- `minor_pent`: instant hook machine
- `suspended`: open, unresolved, floating
- `power`: blunt, strong, riff-first
- `phrygian`: darker, tighter, slightly dangerous
- `lydian`: bright, airy, more "floating above the chord"
- `harmonic_minor`: dramatic, eastern-leaning, instantly tense
- `blues`: dirty, direct, riff-ready

**Quick feel**

If the phrase is too harmonically busy, try `major_pent` or `minor_pent`.

### `range`

**What it does**

Sets the general height profile of the generated phrase.

**Options**

- `close`
- `octave`
- `wide`

**Use it when**

- you want the line to stay compact
- you want a little lift without going full fireworks
- you want a more dramatic upper contour

**Quick guide**

- `close`: keeps the phrase tight and centered
- `octave`: allows a cleaner top lift
- `wide`: invites more upper-register motion

**Quick feel**

If the sequence feels flat, `range = octave` is usually the first useful move.

### `spread`

**What it does**

Controls how strongly the engine favors higher states inside the chosen range.

**Range**

`0.0` to `1.0`

**Use it when**

- `range` feels right, but the line still does not climb enough
- you want more contour without changing scale or chaos
- you want a phrase to lean upward over time

**Quick guide**

- low `spread`: grounded, center-heavy
- medium `spread`: more shape, still controlled
- high `spread`: noticeably more pull toward the upper states

**Quick feel**

`range` chooses the room. `spread` chooses how often you look at the ceiling.

### `density`

**What it does**

Sets the overall amount of played steps.

This is the main note occupancy control.

**Range**

`0.0` to `1.0`

**Use it when**

- the sequence is too busy
- you want a more spacious pocket
- you want to leave room for drums or another melodic lane

**Quick guide**

- `1.0`: almost every step wants to speak
- `0.6`: active but breathable
- `0.35`: sparse and useful
- `0.15`: occasional punctuation

**Quick feel**

If `density` is the amount of conversation, `rest` is the way the speaker pauses.

### `chaos`

**What it does**

Pushes the Markov transitions away from the most stable melodic paths.

**Range**

`0.0` to `1.0`

**Use it when**

- the phrase feels too predictable
- you want more leaps and less obedient behavior
- you want the generator to stop sounding like a polite arp

**Quick guide**

- low `chaos`: melodic, anchored, legible
- medium `chaos`: lively and varied
- high `chaos`: jumpy, unstable, risky

**Quick feel**

If a phrase sounds boring, raise `chaos`.
If it sounds drunk, raise `resolve`.

### `resolve`

**What it does**

Increases the engine's tendency to return to the root state.

It makes phrases feel more cadential and more "at home".

**Range**

`0.0` to `1.0`

**Use it when**

- `chaos` is fun but too untethered
- you want phrases to land more often
- you want a melodic center of gravity

**Quick guide**

- low `resolve`: floating, drifting
- medium `resolve`: recognizable returns
- high `resolve`: more root attraction, fewer wild leaps hanging in the air

**Quick feel**

`resolve` is the gravity knob.

### `rest`

**What it does**

Shapes how silences behave.

Important: `rest` is not the main density control anymore. `density` decides how much is played overall. `rest` decides whether silences feel like occasional missing notes or grouped little breaths.

It also gives a small accent to the note that comes back after a silence streak.

**Range**

`0.0` to `1.0`

**Use it when**

- you want the phrase to breathe more musically
- you want silences to feel intentional
- you want re-entries to pop a little

**Quick guide**

- low `rest`: isolated gaps, lighter interruption
- medium `rest`: more phrase-like breathing
- high `rest`: grouped silences and more obvious re-entry punctuation

**Quick feel**

Low `density` plus low `rest` sounds sparse.
Low `density` plus high `rest` sounds phrased.

### `swing`

**What it does**

Delays off-steps and slightly reshapes their velocity feel.

**Range**

`0.0` to `1.0`

**Use it when**

- the phrase is too rigid
- the line needs pocket
- straight sixteenths feel too robotic

**Quick guide**

- low `swing`: subtle movement
- medium `swing`: clear groove
- high `swing`: pronounced lilt

**Quick feel**

If the pattern is good but not dancing, try `swing` before touching harmony.

### `steps`

**What it does**

Chooses the timing resolution.

**Options**

- `4`
- `8`
- `16`

**Use it when**

- you want broad pulses instead of chatter
- you want faster internal motion
- you want the exact same behavior, but at a different rhythmic granularity

**Quick guide**

- `4`: bold and minimal
- `8`: solid middle ground
- `16`: most detailed and lively

### `gate`

**What it does**

Controls note length.

**Range**

`0.0` to `1.0`

**Use it when**

- notes feel too clipped
- notes are stepping on each other
- you want plucks, stabs, or held behavior

**Quick guide**

- low `gate`: dry, short, percussive
- medium `gate`: default all-rounder
- high `gate`: longer and smoother

### `vel`

**What it does**

Sets the base output velocity.

**Range**

`20` to `127`

**Use it when**

- your destination synth is too soft or too aggressive
- you want a more delicate line
- you want to drive filters or velocity modulation harder

**Good to know**

Re-entry accents from `rest` are applied on top of this base velocity.

## Starter Recipes

Here are a few good starting points.

### 1. Friendly melodic engine

- `scale = ionian`
- `range = close`
- `spread = 0.20`
- `density = 0.70`
- `chaos = 0.25`
- `resolve = 0.55`
- `rest = 0.20`
- `swing = 0.10`

Use this when you want something musical immediately.

### 2. Moody bass walker

- `root = -12`
- `scale = aeolian`
- `range = close`
- `spread = 0.10`
- `density = 0.55`
- `chaos = 0.30`
- `resolve = 0.70`
- `rest = 0.35`
- `gate = 0.45`

Short notes, dark scale, strong center of gravity.

### 3. Pentatonic hook machine

- `scale = minor_pent`
- `range = octave`
- `spread = 0.45`
- `density = 0.75`
- `chaos = 0.45`
- `resolve = 0.40`
- `rest = 0.15`
- `swing = 0.05`

Very hard to make this sound truly wrong.

### 4. Floating suspended line

- `scale = suspended`
- `range = octave`
- `spread = 0.35`
- `density = 0.50`
- `chaos = 0.35`
- `resolve = 0.20`
- `rest = 0.45`
- `gate = 0.65`

Useful for ambient pads, mallets, and soft leads.

### 5. Riff gremlin

- `scale = power`
- `range = wide`
- `spread = 0.80`
- `density = 0.85`
- `chaos = 0.70`
- `resolve = 0.35`
- `rest = 0.10`
- `gate = 0.35`

This is where it stops pretending to be polite.

## Tweak Strategy

If you are not sure where to start, use this order:

1. `scale`
2. `root`
3. `range`
4. `density`
5. `chaos`
6. `resolve`
7. `rest`
8. `swing`

Why this order:

- first choose the harmonic world
- then place it in the right register
- then decide how much space it gets
- only then decide how disciplined or unruly it should be

## Development

### Run Tests

```bash
make test
```

### Build For Move

```bash
./scripts/build.sh native
```

### Install To Move

```bash
./scripts/install.sh move.local
```

## Implementation Notes

- Written in portable C
- Uses a small Markov engine for note-state transitions
- Syncs from MIDI transport bytes and MIDI clock
- Safe defaults for live use: transport-aware, no host clock API dependency

## Credits

Built for the Schwung ecosystem and inspired by hands-on generative tools for Ableton Move.

MarkovGroove is also directly informed by a Schwung-native conversion of the Subsequence project:

- [simonholliday/subsequence](https://github.com/simonholliday/subsequence)

Special nod to projects that helped shape the tone and presentation style:

- [genera-move](https://github.com/fillioning/genera-move)
- [schwung-eucalypso](https://github.com/handcraftedcc/schwung-eucalypso)
- [schwung-superarp](https://github.com/handcraftedcc/schwung-superarp)

## License

[MIT](LICENSE)
