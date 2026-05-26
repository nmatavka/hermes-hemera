# Signature And Diagnostics Notes

These notes capture the compose behaviors that are now actively implemented in the portable compose controller.

## Signatures

- Signatures are discovered from tracked fixtures or runtime signature directories through `SignatureStore`.
- New compose state applies default stationery first, then the selected or default signature.
- Inserted signatures are tracked as managed content with a start offset, length, and original plain-text body.
- Switching signatures replaces the managed signature block when it is still intact.
- Editing inside the managed signature block detaches ownership so later signature changes do not rewrite user-edited text.

## Diagnostics

- Spell-check results and MoodWatch body matches are exposed as body diagnostics and mirrored into `RichTextSurface` for inline rendering.
- Subject-line spell issues, Boss Protector hits, and styled-send warnings stay as compose diagnostics rather than inline body marks.
- Compose status banners prefer blocking send errors first, then send-time warnings, then Boss Protector and MoodWatch warnings, and finally informational spelling status.

## Queue Behavior

- Queue/save behavior uses the portable compose snapshot rather than reading from legacy paths.
- Queue validation always runs styled-send, MoodWatch, and Boss Protector checks before persisting into the `out` mailbox.
- Warning-bearing queue actions require explicit confirmation from the shell layer before the message is written.
