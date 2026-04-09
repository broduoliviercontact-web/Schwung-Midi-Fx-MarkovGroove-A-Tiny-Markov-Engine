# Convert Open-Source MIDI FX to Schwung

## Purpose
Use this skill to perform the full end-to-end conversion of an open-source MIDI FX project into a Schwung module.

This is the orchestration skill.
It combines audit, design, implementation, UI planning, and packaging into one workflow.

## Inputs
You may receive:
- a repo URL
- one or more source files
- a README
- a plugin bundle
- a device spec
- an earlier audit
- a user request describing desired behavior

## Goal
Deliver a clean Schwung-native module, not a shallow wrapper.

The converted module should:
- honor the musical intent of the original project
- fit Schwung conventions
- feel usable on Move
- be maintainable in the repo
- be testable incrementally

## Workflow

### Phase 1: Audit
First determine:
- what the source project really does
- what part is worth porting
- what must be rewritten
- whether the port should be reduced in scope

### Phase 2: Design
Produce:
- module identity
- manifest draft
- parameter set
- file tree
- native engine decision
- Move interaction plan

### Phase 3: Engine Port
Port or rewrite the musical core.

Prioritize:
- correctness
- note lifecycle safety
- deterministic timing
- predictable parameter mapping

### Phase 4: Move UX
Design the control surface for Move.
Reduce complexity where needed.

### Phase 5: State and Chain Integration
Add:
- parameter restoration
- optional custom state
- chain parameter support
- warnings/errors when relevant

### Phase 6: Validation
Test:
- note input/output
- note-off correctness
- sustain interactions if relevant
- timing behavior
- clock sync behavior
- empty-state behavior
- mode changes
- chain editing
- state restore

### Phase 7: Packaging
Prepare the module for either:
- in-repo integration
- external drop-in packaging

## Required Output Format

### Source Summary
Explain the original project in plain language.

### Port Scope
State what is being ported and what is intentionally excluded.

### Schwung Design
Provide:
- file tree
- manifest draft
- parameter map
- UI plan

### Implementation Plan
List the files to create and the order of implementation.

### Risks
List technical or UX risks.

### Milestone Plan
Provide a short milestone sequence such as:
1. minimal engine
2. parameter wiring
3. state restore
4. Move UI
5. validation
6. package

### Code
Generate the actual files when asked.

## Guardrails
- Do not chase feature parity at the expense of usability.
- Do not preserve source-project complexity that makes no sense on Move.
- Do not claim “ported” if the result is only a loose conceptual rewrite unless you say so clearly.
- Prefer a solid V1 over an ambitious unstable clone.