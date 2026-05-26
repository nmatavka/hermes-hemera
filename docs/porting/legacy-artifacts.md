# Selective Legacy Transplants

This port keeps `legacy/eudora-drop/` out of version control and treats it as a read-only source quarry.
Only artifacts that are now actively consumed by the new Haiku-port tree are tracked here.

## Tracked fixture transplants

- `tests/fixtures/legacy/profile_snapshots/`
  - `Eudora.adr`
  - `Eudora.box`
  - `Eudora.fil`
  - `Eudora.mis`
  - `Eudora.sas`
  - `Eudora.tol`
  - `eudora.blk`
- `tests/fixtures/legacy/import/WBImport.INI`
- `tests/fixtures/legacy/help/`
  - `eudora.hh`
  - `Options.hh`
  - `Eudora.cnt`

## Tracked documentation transplants

- `docs/porting/ssl/SSL Notes.txt`
- `docs/porting/help/`
  - `eudora.hh`
  - `Options.hh`
  - `Eudora.cnt`

## Why these files are tracked

- The profile snapshots drive `LegacyAccountService` and settings-compatibility tests against real Eudora-era material.
- `WBImport.INI` drives the import discovery and destination-layout tests.
- The help topic maps drive `LegacyHelpCatalog` and the help-catalog parity tests.
- `SSL Notes.txt` is retained as an active porting reference for transport and TLS behavior while the native Haiku networking stack is brought up.
