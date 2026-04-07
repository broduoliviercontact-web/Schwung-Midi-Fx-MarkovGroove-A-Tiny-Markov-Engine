# Publish Schwung Custom Module

## Purpose
Use this skill when a Schwung module is already implemented and tested, and must now be published as an installable custom module.


This skill covers packaging, release metadata, GitHub release automation, and final publication checks.

Do not use this skill at the start of a project.
Use it only after the module works locally and the release version is intentional.

## Goal
Produce a valid Schwung custom-module release with:

- `<module_id>-module.tar.gz`
- a tarball that extracts to `<module_id>/`
- `release.json` on the default branch
- a GitHub Actions release workflow
- a Git tag matching the module version
- release URLs ready to share

## Required Inputs
Before starting, confirm all of the following:

- the module already builds
- native tests already pass
- `src/module.json` exists
- the release version is decided
- the GitHub repo remote is configured correctly

Read first:

- `src/module.json`
- `README.md`
- `scripts/build.sh`
- `scripts/install.sh`
- `.github/workflows/` if it already exists

If present, also read:

- `scripts/package.sh`
- `release.json`
- `.gitignore`

## Step 1 — Read Release Identity From `src/module.json`

Extract and verify:

- `id`
- `name`
- `version`
- `component_type`

Rules:

- The tarball filename must be based on `id`, not the repository name
- The extracted folder name must exactly match `id`
- The Git tag must be `v<version>`

Example:
- `id`: `branchage`
- `version`: `0.2.0`
- tag: `v0.2.0`
- asset: `branchage-module.tar.gz`

If the module `id` and packaging assumptions disagree, stop and fix that first.

## Step 2 — Verify The Module Is Release-Ready

Run in this order:

```bash
make test
./scripts/build.sh
```

If tests fail, do not continue to release work.

Confirm the Move build artifact exists:

```bash
ls -la build/aarch64/dsp.so
```

If the repo uses a custom UI, confirm those files also exist before packaging:
- `ui.js`
- `ui_chain.js`
- any required static assets

## Step 3 — Create Or Review Packaging Script

Preferred path: create `scripts/package.sh`.

The script should:

1. read `id` and `version` from `src/module.json`
2. build for aarch64 if needed
3. stage files into `dist/<id>/`
4. copy required files:
   - `module.json`
   - `dsp.so` if present
   - `ui.js` if present
   - `ui_chain.js` if present
   - other runtime files if required
5. create `dist/<id>-module.tar.gz`

Expected tarball structure:

```text
<id>-module.tar.gz
└── <id>/
    ├── module.json
    ├── dsp.so
    ├── ui.js
    ├── ui_chain.js
    └── ...
```

Critical rule:
- never package files at tarball root
- always package under a top-level `<id>/` folder

## Step 4 — Verify Tarball Structure

Always verify the archive contents:

```bash
tar -tzf dist/<id>-module.tar.gz
```

The result must begin with:

```text
<id>/
<id>/module.json
```

If files are at the root, the release is invalid.

## Step 5 — Create Or Update `release.json`

`release.json` must live on the default branch, usually `main`.

Minimum shape:

```json
{
  "version": "0.2.0",
  "download_url": "https://github.com/<owner>/<repo>/releases/download/v0.2.0/<id>-module.tar.gz"
}
```

Optional fields are allowed only when justified:
- `install_path`
- `name`
- `description`
- `requires`
- `post_install`
- `repo_url`

Rules:
- `version` must match `src/module.json`
- `download_url` must point to the GitHub release asset for the matching tag
- do not invent a custom asset name unrelated to `id`

## Step 6 — Create Or Update GitHub Release Workflow

Preferred file:

```text
.github/workflows/release.yml
```

The workflow should trigger on tag push:

```yaml
on:
  push:
    tags:
      - "v*"
```

The workflow must:

1. checkout the repo with full history
2. build/package the module
3. upload `dist/<id>-module.tar.gz` to the GitHub release
4. update `release.json` on `main` so it points to the new tag asset

Minimum expectations:
- `contents: write` permission
- release asset upload
- bot commit for `release.json`
- no hardcoded asset name unless it truly matches module `id`

## Step 7 — Ignore Build Output

If packaging creates `dist/`, ensure it is ignored:

```text
dist/
```

Do not commit generated tarballs into the repo unless explicitly requested.

## Step 8 — Final Publication Commands

After files are ready, the release flow is:

```bash
git add .
git commit -m "Release v<version>"
git tag v<version>
git push origin main
git push origin v<version>
```

If the tag already exists remotely:
- do not assume the workflow reran
- prefer bumping to the next patch version instead of force-reusing the same tag

## Step 9 — Final Verification

After pushing, verify all of the following:

1. GitHub release exists for `v<version>`
2. asset `<id>-module.tar.gz` is attached
3. `release.json` on `main` returns the same version and correct URL
4. the asset URL responds successfully
5. the tarball still extracts to `<id>/`

Useful checks:

```bash
curl -fsSL https://raw.githubusercontent.com/<owner>/<repo>/main/release.json
curl -I -L https://github.com/<owner>/<repo>/releases/download/v<version>/<id>-module.tar.gz
```

## Common Failures

**Asset name does not match module id**
- Fix the package script
- The correct asset name is `<id>-module.tar.gz`

**Tarball extracts incorrectly**
- Rebuild using `tar -C dist -czf dist/<id>-module.tar.gz <id>`

**`release.json` exists but points to the wrong repo or tag**
- Update it immediately
- Version, tag, and asset URL must agree exactly

**Workflow exists but the release was not created**
- Check whether the tag existed before the workflow was added
- Prefer a new patch release instead of trying to reuse the old tag

**Remote is not `origin`**
- Check `git remote -v`
- Push to the actual configured remote, or rename it intentionally

**Files missing from tarball**
- Confirm the packaging step copies all runtime files
- Re-run `tar -tzf` before publishing

## Output Contract

When this skill is used, produce:

1. A short release-readiness summary
2. The exact files to create or modify
3. The expected tarball name and structure
4. The exact `release.json` content
5. The GitHub Actions workflow recommendation
6. The final git/tag/push commands
7. The final verification URLs to share

## Preferred Mindset

This is not a generic release process.

The objective is:
- valid Schwung custom module packaging
- stable GitHub-hosted distribution
- no ambiguity between module id, version, tag, asset name, and `release.json`

Prefer a boring, deterministic release over a clever one.
