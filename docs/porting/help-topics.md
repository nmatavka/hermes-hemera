# Help Topic Catalog Notes

The Haiku port now tracks legacy help topic maps as active compatibility fixtures and documentation sources.

## Current tracked maps

- `docs/porting/help/eudora.hh`
- `docs/porting/help/Options.hh`
- `tests/fixtures/legacy/help/eudora.hh`
- `tests/fixtures/legacy/help/Options.hh`

## Current parser behavior

- Plain topic-map rows such as `HIDD_SETTINGS_SPELL 0x1000F` are loaded as topic ids.
- `#define` rows such as `#define Include_signature_on_reply 10170` are also loaded as topic ids.
- The symbolic id is currently used as both the stable identifier and the default label.

This keeps help-topic resolution grounded in real historical identifiers while the larger Haiku-native help browser and rendering path are implemented.
