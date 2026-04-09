# Audit Open-Source MIDI FX for Schwung Conversion

## Purpose
Use this skill to evaluate whether an open-source MIDI effect project can be converted into a Schwung module.

The goal is not to code immediately.
The goal is to determine feasibility, conversion strategy, risks, and the cleanest architecture.

## Inputs
You may receive:
- a GitHub repo
- source files
- a README
- a plugin project
- a Max device
- a JUCE plugin
- a small standalone MIDI processor
- a shared ChatGPT spec or design description

## Main Question
Can this project be converted into a clean Schwung MIDI FX module with acceptable effort and acceptable behavior on Move?

## Audit Process

### 1. Identify the Real Product Type
Classify the source project as one of:
- MIDI FX
- instrument
- audio FX
- sequencer
- controller utility
- hybrid plugin

If it is not primarily a MIDI FX, explain whether only part of it should be ported.

### 2. Identify the Runtime Core
Find the actual MIDI transformation logic:
- note in -> note out
- chord expansion
- arp sequencing
- scale quantization
- timing generation
- CC generation
- note filtering
- velocity remapping
- randomization / humanization
- transposition / harmonization

Distinguish core logic from:
- UI
- plugin wrapper
- DAW glue
- preset browser
- platform-specific code

### 3. Assess Portability
Determine whether the source core is:
- trivially portable
- portable with moderate rewrite
- only conceptually portable
- poor fit for Schwung

Assess:
- language
- dependencies
- timing model
- threading model
- memory model
- clock assumptions
- host assumptions
- MIDI API assumptions

### 4. Determine Schwung Architecture
Choose one:
- native MIDI FX with lightweight JS UI
- native MIDI FX with chain-aware editing
- JS-heavy module with no native engine
- not recommended

### 5. Define User-Facing Controls
Extract the minimal parameter surface needed for Move.

For each parameter, define:
- key
- display name
- type
- range/options
- default
- step size if numeric

Do not blindly port every original control.
Prefer a compact Move-friendly control surface.

### 6. Define Risks
Look for:
- excessive complexity
- heavy external dependencies
- poor timing determinism
- DAW-only assumptions
- UI that cannot translate to Move
- polyphony edge cases
- sustain / note-off bugs
- state restore complexity
- external sync assumptions

## Required Output Format

### Feasibility
One of:
- Strong fit
- Good fit
- Partial fit
- Poor fit

### Source Summary
- Name:
- License:
- Language:
- Core behavior:
- Dependencies:
- Host assumptions:

### Conversion Strategy
- Keep:
- Rewrite:
- Discard:
- Simplify:

### Schwung Target Shape
- Module type:
- Native engine:
- UI files:
- Chain integration:
- State persistence:

### Parameter Proposal
List the final proposed parameter set.

### Move UX Proposal
Describe:
- knob mapping
- shift layers
- any pad usage
- any step button usage
- LED feedback

### Risks
List concrete technical risks.

### Recommendation
Answer clearly:
- Should we port this?
- Should we port a reduced version?
- What should be the first implementation milestone?

## Guardrails
- Do not confuse plugin wrapper code with the musical engine.
- Do not promise a one-to-one port unless it is realistic.
- Prefer a smaller stable Schwung version over an incomplete feature clone.
- Be explicit when you are inferring behavior from incomplete source material.