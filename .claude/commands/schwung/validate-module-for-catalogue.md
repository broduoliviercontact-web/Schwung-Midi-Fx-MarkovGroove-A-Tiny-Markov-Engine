# Validate Module for Schwung Catalogue

## Purpose
Use this skill to audit, fix, and certify a Schwung module so it appears correctly in the Schwung device catalogue and can be installed as a drop-in external module.

This skill covers the gap between "module that runs" and "module that is publishable".

## Goal
Produce a module that:
- appears in the Schwung device catalogue under the correct category
- installs cleanly as a drop-in at `/data/UserData/schwung/modules/<module-id>/`
- declares all required and recommended manifest fields
- uses `api_version: 2` for Signal Chain compatibility
- has correct `capabilities` including `component_type` and `chainable`
- has no missing referenced files
- has aligned parameters across `module.json`, engine, UI, and chain UI
- passes the full catalogue smoke-test checklist

## Inputs
Provide one of:
- a module folder path
- a `module.json` + list of accompanying files
- a partial module described in plain language

## Catalogue Compatibility Rules

### Required Manifest Fields
Every catalogue-ready module must have:
- `id` — lowercase, filesystem-safe, unique
- `name` — human-readable display name
- `version` — semver string (e.g., `"1.0.0"`)
- `api_version` — must be `2` for Signal Chain support and new modules

### Strongly Recommended Fields
- `description` — shown in the catalogue browser
- `author` — shown on the module page
- `abbrev` — 3–6 char short name for Shadow UI slot display

### Capabilities for Catalogue Placement
Use the `capabilities` block to declare what the module does.

For MIDI FX modules targeting the Signal Chain catalogue:
```json
{
    "capabilities": {
        "midi_in": true,
        "midi_out": true,
        "chainable": true,
        "component_type": "midi_fx"
    }
}
```

Valid `component_type` values:
| Value | Where it appears |
|-------|-----------------|
| `midi_fx` | Signal Chain MIDI FX slot |
| `sound_generator` | Instrument slot |
| `audio_fx` | Audio FX slot |
| `utility` | Utility section |
| `tool` | Tools menu |
| `featured` | Featured / top section |

Do not set `component_type` to a value not in this list.

### File Integrity
Every file referenced in `module.json` must exist in the module folder:
- `"ui"` → `ui.js` must be present
- `"ui_chain"` → `ui_chain.js` must be present
- `"dsp"` → `dsp.so` must be present
- remove manifest references to files that are missing and not expected

### api_version Requirement
New modules must use `"api_version": 2`.
`api_version: 1` does not support multiple instances or Signal Chain.

## Validation Process

### Step 1: Manifest Audit
Check `module.json` for:
- [ ] `id` present, lowercase, no spaces
- [ ] `name` present and readable
- [ ] `version` present and semver
- [ ] `api_version` is `2`
- [ ] `abbrev` present and 3–6 chars
- [ ] `description` present
- [ ] `author` present
- [ ] `capabilities.component_type` set to a valid value
- [ ] `capabilities.chainable` is `true` if it belongs in Signal Chain
- [ ] `capabilities.midi_in` / `midi_out` match what the module actually does
- [ ] No unsupported or invented manifest fields
- [ ] File size under 8KB

### Step 2: File Integrity Check
For every file listed in `module.json`:
- [ ] File exists in the module folder
- [ ] File is not empty
- [ ] No broken relative references inside UI JS files (import paths)

### Step 3: Parameter Alignment Check
For every parameter defined in `module.json` (defaults or ui_hierarchy):
- [ ] Parameter exists in the engine (`dsp.c` or equivalent)
- [ ] Parameter key is spelled the same across manifest, UI, and engine
- [ ] Default values are sensible and within range
- [ ] Chain params (`chain_params`) are a subset of full params

### Step 4: UX Catalogue Readiness
- [ ] `abbrev` is readable at small size on Move Shadow UI
- [ ] Module name is not too long for the catalogue list
- [ ] Description is one clear sentence

### Step 5: Drop-In Install Verification

Install path depends on `component_type`:

| `component_type` | Install path under `/data/UserData/schwung/` |
|------------------|----------------------------------------------|
| `midi_fx` | `modules/midi_fx/<id>/` |
| `sound_generator` | `modules/sound_generators/<id>/` |
| `audio_fx` | `modules/audio_fx/<id>/` |
| `utility` | `modules/utilities/<id>/` |
| `overtake` | `modules/overtake/<id>/` |
| `tool` | `modules/tools/<id>/` |

Confirm the module folder structure is self-contained:
```
<module-id>/
  module.json        ← required
  ui.js              ← if declared
  ui_chain.js        ← if declared
  dsp.so             ← if declared
```
No build artifacts, no source-only files, no external dependencies outside the folder.

After install: call `host_rescan_modules()` from any Schwung JS context, or restart Schwung. No host recompile is needed.

### Step 5b: Catalog Entry Check (if submitting to official catalog)

If the module is being submitted to the Schwung module catalog, verify the catalog entry:

```json
{
    "id": "<module-id>",
    "name": "<display name>",
    "description": "<one clear sentence>",
    "component_type": "<valid type>",
    "latest_version": "<semver>",
    "download_url": "<direct tarball URL>"
}
```

The tarball must:
- be a `.tar.gz`
- extract into exactly one folder named `<id>/`
- contain `module.json` at the root of that folder
- contain all referenced files

### Step 6: Smoke Test Checklist
Provide a post-install verification checklist:
- [ ] Module appears in the correct catalogue section
- [ ] Module loads without error
- [ ] Parameters have correct defaults
- [ ] MIDI input is processed correctly
- [ ] MIDI pass-through is correct
- [ ] No stuck notes on mode change
- [ ] No stuck notes on transport stop
- [ ] State recalls correctly after reload
- [ ] `abbrev` appears correctly in Shadow UI slot
- [ ] Chain editing works if `chainable` is true

## Fix Strategy
For each issue found, prefer the minimal fix:
- missing `api_version` → add `"api_version": 2`
- wrong `component_type` → correct to the nearest valid value
- missing `abbrev` → derive from module name (max 6 chars, uppercase)
- missing `description` → write one clear sentence from the module's purpose
- missing file reference → either add the file or remove the manifest key
- parameter mismatch → align the key name across manifest, UI, and engine
- file size over 8KB → split or trim non-essential content

Do not invent unsupported fields.
Do not remove capabilities that are genuinely needed.
Do not rename parameters without updating all consumers.

## Required Output Format

### Audit Report
List every issue found, grouped by severity:
- **Blocking** — module will not load or catalogue will reject it
- **Warning** — module loads but displays incorrectly or behaves poorly
- **Suggestion** — optional improvement for quality or UX

### Fixed `module.json`
Provide the corrected full `module.json` as a fenced `json` block.

### File Tree Delta
List files to add, remove, or rename to reach a clean drop-in state.

### Updated Code Snippets
If parameter keys changed, show the minimal diffs needed in `ui.js`, `ui_chain.js`, or `dsp.c`.

### Install Instructions
Exact steps to deploy the module:
1. Copy the module folder to `/data/UserData/schwung/modules/<module-id>/`
2. In Schwung, run `host_rescan_modules()` or restart
3. Verify the module appears under the correct category

### Smoke Test Checklist
Provide the completed checklist with expected results for this specific module.

### Catalogue Entry Preview
Write how the module will appear in the catalogue:
```
Name:        <name>
Category:    <component_type>
Abbrev:      <abbrev>
Version:     <version>
Description: <description>
Author:      <author>
```

## Guardrails
- Do not invent unsupported manifest fields.
- Do not set `component_type` to a value not in the official list.
- Do not remove capabilities the module genuinely uses.
- Do not claim the module is catalogue-ready until all Blocking issues are resolved.
- Do not package build artifacts or unrelated source files.
- Always verify referenced files exist before writing the final manifest.
