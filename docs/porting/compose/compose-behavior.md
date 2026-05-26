# Compose Behavior Notes

These notes are distilled from the legacy compose path in `compmsgd.cpp`, `PgCompMsgView.cpp`, `PaigeEdtView.h`, `headervw.cpp`, `BossProtector.cpp`, and `BeforeSending.txt`.

## Precedence

- New compose state starts with explicit `To`, `Cc`, `Bcc`, `Subject`, `Body`, and attachments.
- If no explicit stationery is supplied, the active personality's default `Stationery` setting is considered.
- Explicit stationery overrides the default stationery choice.
- Stationery can change the effective persona, signature, and send behavior for the message.
- Explicit persona selection wins over persona implied by stationery.

## Automatic Checks

- MoodWatch is driven by `DoMoodWatchCheck`, `MoodCheckBackground`, and `MoodWatchInterval`.
- Header/body spelling and Boss Protector are both timer-driven in the legacy client with a 500 ms cadence after edits settle.
- Compose warning state is dirty-tracked separately for spell, MoodWatch, and Boss Protector so the checks are rerun only after edits.

## Send-Time Validation

- MoodWatch warnings are surfaced at multiple thresholds: "might offend", "probably offensive", and "on fire".
- Boss Protector can warn from nickname BP lists and from inside/outside-domain rules.
- Styled send behavior is resolved at queue/send time:
  `WarnQueueStyledText` can force a user decision, otherwise `SendPlainAndStyled` and `SendStyledOnly` decide the send format.

## Scope In This Tranche

- MoodWatch, Boss Protector, default stationery discovery/application, draft persistence, and styled/plain send enforcement are part of the active compose port.
- Signature selection is preserved as model data and compose policy here; signature body injection is still a separate integration step.
