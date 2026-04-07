# BUILDING.md — Build and Deploy Guide

## Prerequisites

- C compiler (gcc or clang) for native builds and tests
- One of the following for Move (aarch64) cross-compilation:
  - `aarch64-linux-gnu-gcc` (preferred)
  - `aarch64-linux-musl-gcc`
  - `zig` with `zig cc -target aarch64-linux-gnu`
  - Docker (fallback — less reliable on macOS with disk issues)
- SSH access to Move: `http://move.local/development/ssh`

## Quick Start

```bash
make test              # run native tests
./scripts/build.sh     # cross-compile for Move
./scripts/install.sh   # deploy to move.local
```

---

## Makefile Targets

| Target | Description |
|---|---|
| `make test` | Build and run all native tests |
| `make native` | Build `build/native/dsp.so` for the host machine |
| `make aarch64` | Build `build/aarch64/dsp.so` for Move (requires cross-compiler) |
| `make check-symbols` | Verify `move_midi_fx_init` is exported in the native build |
| `make clean` | Remove all build artifacts |

## Build Flags

```
-std=c99 -Wall -Wextra -Werror -O2 -fPIC
-Isrc/dsp -Isrc/host
```

All warnings are errors. Fix every warning before deploying.

---

## Native Tests

Always run tests before building for Move:

```bash
make test
```

Test binaries are built from `tests/`. Each module has two test files:
- `tests/<module>_engine_test.c` — tests the portable engine in isolation
- `tests/<module>_midi_fx_test.c` — tests the host wrapper and MIDI dispatch

To add tests for new behavior: add cases to the existing test files before implementing the feature. Failing tests are the spec.

---

## Cross-Compilation for Move

### Option 1 — Local cross-compiler (preferred)

Install one of:

```bash
# macOS with Homebrew
brew install aarch64-linux-gnu-binutils

# or: install zig (recommended on macOS)
brew install zig
```

Then build:

```bash
./scripts/build.sh
# or explicitly:
./scripts/build.sh native
```

### Option 2 — Docker (fallback)

```bash
./scripts/build.sh docker
```

The Dockerfile is in `scripts/Dockerfile`. If Docker fails with I/O or metadata errors on macOS, fix Docker Desktop first or use `zig` instead.

### Auto mode (default)

```bash
./scripts/build.sh
```

Tries local compiler first, falls back to Docker if none is found.

### Verify the build artifact

```bash
ls -la build/aarch64/dsp.so
make check-symbols   # verifies move_midi_fx_init is exported
```

---

## Install to Move

### Default (move.local)

```bash
./scripts/install.sh
```

### Specific IP

```bash
./scripts/install.sh 192.168.x.x
```

The install script:
1. Creates the module directory on Move
2. Copies `src/module.json` → `<module_dir>/module.json`
3. Copies `build/aarch64/dsp.so` → `<module_dir>/dsp.so`

Install path:
```
/data/UserData/schwung/modules/midi_fx/<module_id>/
```

Verify after install:
```bash
ssh root@move.local 'ls -la /data/UserData/schwung/modules/midi_fx/<module_id>'
```

---

## Restart Schwung

After install, restart Schwung for the module to appear:

```bash
ssh root@move.local 'cd /data/UserData/schwung && nohup ./start.sh >/tmp/start_schwung.log 2>&1 </dev/null &'
```

Wait a few seconds, then check the log:

```bash
ssh root@move.local 'cat /tmp/start_schwung.log'
```

---

## Smoke Test

```bash
./scripts/smoke_test.sh
```

Runs the native test suite and verifies the build artifact is present.

---

## Adding a New Module

1. Add source files: `src/dsp/<module>_engine.h`, `src/dsp/<module>_engine.c`, `src/host/<module>_plugin.c`
2. Add test files: `tests/<module>_engine_test.c`, `tests/<module>_midi_fx_test.c`
3. Update the `Makefile`:
   - Add new source files to `DSP_SRCS` and `HOST_SRCS`
   - Add new test targets
4. Add `src/module.json`
5. Update `scripts/install.sh` with the correct `MODULE_ID`
6. Run `make test` to verify native builds pass

See `docs/MODULES.md` for the module structure and API reference.

---

## Common Errors

**`move_midi_fx_init` not found after install**
- Run `make check-symbols` to verify the entry point is exported
- Check that the wrapper file calls `midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host)`

**Cross-compiler not found**
- Install `zig` (`brew install zig`) or `aarch64-linux-gnu-gcc`
- If using Docker, ensure Docker Desktop is running and disk health is clean

**Build fails with `-Werror`**
- Fix all warnings — they are errors by design
- Common issues: unused variables, implicit function declarations, missing return values

**Module does not appear on Move after install**
- Verify install path matches `id` field in `module.json`
- Restart Schwung (see above)
- Check the log: `ssh root@move.local 'cat /tmp/start_schwung.log'`
