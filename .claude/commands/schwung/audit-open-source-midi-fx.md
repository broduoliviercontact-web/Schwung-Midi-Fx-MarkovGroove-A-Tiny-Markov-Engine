# Audit Open-Source MIDI FX for Schwung Conversion

## Purpose
Use this skill to evaluate whether an open-source MIDI effect project can be converted into a Schwung module.

The goal is not to code immediately. The goal is to determine feasibility, conversion strategy, risks, and the cleanest architecture.

## Inputs
You may receive:
- a GitHub repo or URL
- source files or a README
- a plugin project (JUCE, Max, Pure Data, Eurorack firmware)
- a ChatGPT spec or design description
- an informal description of desired behavior

## Main Question

**Can this become a clean Schwung MIDI FX module with reasonable effort?**

Answer: what the original does, what gets converted vs dropped, how it maps to Move, what the risks are.

---

## Audit Process

### Step 1 — Product Classification

Categorize the source project:
- MIDI FX (note transform, arpeggio, quantizer, chord generator…)
- Instrument / sound generator → cannot be a `midi_fx`
- Audio FX → cannot be a `midi_fx`
- Sequencer or step editor
- Controller utility
- Hybrid — identify which portion is worth porting

### Step 2 — Locate the Musical Core

Isolate the actual MIDI transformation or generation logic from:
- Plugin wrapper code (VST, AU, Max, JUCE host glue)
- GUI / UI code
- Preset management
- Platform-specific utilities

Identify what the core does: note transformation, sequencing, quantization, velocity remapping, arpeggiation, probability, timing…

### Step 3 — Evaluate Portability

Classify the algorithmic core:

| Class | Description |
|---|---|
| **Trivially portable** | Pure stateless math or lookup — straightforward C translation |
| **Moderately rewritable** | Clear logic with manageable dependencies — clean port with effort |
| **Conceptually portable** | Intent is clear but implementation is too coupled — rewrite from behavior spec |
| **Unsuitable** | Audio DSP, heavy threading, or platform-locked — not viable as `midi_fx` |

Assess:
- Language and external dependencies
- Timing model (block-based, event-based, sample-accurate?)
- Threading assumptions
- Memory allocation patterns
- Clock synchronization approach
- Filesystem or preset dependencies

### Step 4 — Assess Schwung Fit

Evaluate against Schwung constraints:
- MIDI only — no audio generation in a `midi_fx` slot
- Move hardware UI — 8 knobs, pads, step buttons, transport
- Chainable in the Signal Chain
- Standard parameter system (`module.json`)
- Native C engine (`dsp.so`) via the portable engine + host wrapper split
- No filesystem access at runtime

Flag immediately if:
- Requires audio output → `midi_fx` not applicable
- Floating-point-heavy DSP → ARM performance risk on Move
- Persistent filesystem → must be redesigned as state-only recall
- UI assumes a screen larger than Move's display

### Step 5 — Propose the Architecture

Decide:
- **Engine split needed?** Two-layer split (portable engine + host wrapper) when timing or testability matters. Single file for simple transforms.
- **Parameters that survive translation** — keep only what makes sense on Move
- **Clock strategy** — Move transport sync, internal BPM, or free-running (no clock API calls)
- **Note lifecycle model** — how notes are tracked and closed safely

### Step 6 — Define the Move UX

Even at audit stage, sketch how this module would feel on Move hardware:
- **Knob map (1–8)** — which parameters get direct knob access
- **Shift layer** — any secondary controls worth the added complexity
- **Pads / step buttons** — only if they add clear musical value
- **LEDs** — what state they would communicate
- **Menus** — how deep the parameter hierarchy needs to go

Keep it minimal. V1 should be navigable without reading a manual.

### Step 7 — Risk Assessment

List concisely:
- Licensing risk (check license vs. project intent — flag uncertainty)
- Algorithmic complexity risk
- Timing / jitter risk
- Stuck note risk
- Behavioral drift (what will sound different from the original)
- Scope creep risk (too many features for V1)

### Step 8 — Decision

Conclude with a clear feasibility rating:

| Rating | Meaning |
|---|---|
| **Strong fit** | Clean port, minimal risk, natural Schwung module |
| **Good fit** | Feasible with deliberate scope reduction |
| **Partial fit** | Only a portion is worth porting; the rest is dropped |
| **Poor fit** | Fundamental mismatch — not recommended |

State: copy/adapt algorithm vs. reimplement from behavior description, and what goes in V1.

---

## Required Output Format

### Summary
One paragraph: what the original does, what the Schwung version would do.

### Feasibility
`Strong fit` / `Good fit` / `Partial fit` / `Poor fit` — explain why.

### Source Summary
- Name, license, language
- Core behavior
- Dependencies
- Host assumptions

### Conversion Strategy
Four columns: **keep** / **rewrite** / **discard** / **simplify**

### Portability Class
`Trivially portable` / `Moderately rewritable` / `Conceptually portable` / `Unsuitable`

With rationale.

### Proposed Architecture
Engine split decision, clock strategy, note lifecycle model, parameter list.

### Move UX Proposal
Knob map, shift layer, pad/step usage, LED plan, menu depth.

### Risk List
Concise bullets: licensing, algorithmic, timing, stuck notes, behavioral drift.

### Recommended Next Step
`design-module` → `create-module-json` → `create-dsp-c` → `create-host-wrapper`

---

## Guardrails
- Do not start coding in this step.
- Do not assume all features from the original must be preserved.
- Do not propose audio synthesis inside a `midi_fx` module.
- Flag licensing uncertainty clearly — do not assume permissive licensing.
- Distinguish the musical engine from wrapper/UI code before judging portability.
- Prefer "Partial fit with reduced scope" over "Not recommended" when the core is salvageable.
