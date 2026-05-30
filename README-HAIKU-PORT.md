# Hemera Haiku Port

This repository root now contains the new Haiku-port scaffolding and build system.
The original Windows/MFC/Stingray-era source drop has been moved to
`legacy/eudora-drop/` and is intentionally excluded from version control so the new
port can evolve independently.

## Layout

- `src/hermes_port/core/`: portable app-owned interfaces and bootstrap implementations.
- `src/hermes_port/haiku/`: native Haiku shell skeleton and future Haiku adapters.
- `tests/`: lightweight regression coverage for the new portable seams.
- `third_party/`: dependency submodules/checkouts for `Hermes-Paige`, `OpenSSL`, and `Hunspell`.
- `legacy/eudora-drop/`: ignored archive of the original Eudora source/materials.

## Dependencies

The intended dependency model is Git submodules:

- `third_party/Hermes-Paige`
- `third_party/openssl`
- `third_party/hunspell`

Run [`scripts/bootstrap_dependencies.sh`](/Users/nick/hermes-hemera/scripts/bootstrap_dependencies.sh) after the repository is initialized to add or refresh those checkouts.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

On non-Haiku hosts this builds the portable core and tests. On Haiku, enabling
`HERMES_BUILD_HAIKU_SHELL` also builds the native shell scaffold.
