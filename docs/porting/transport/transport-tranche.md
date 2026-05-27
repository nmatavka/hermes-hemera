# Transport Tranche Notes

## Scope Landed
- Live queued SMTP send through `MailTransportCoordinator`
- Live POP retrieval with UIDL dedupe through `SyncStateStore`
- Live IMAP mailbox discovery plus incremental `UID FETCH` sync
- POSIX socket transport with line reads, implicit TLS, and STARTTLS upgrade seams
- OpenSSL-backed client TLS session use through `OpenSslTlsProvider`
- Filesystem-backed credentials, POP/IMAP sync state, and richer message/mailbox metadata
- Task tracking through `MailTaskModel`
- Attachment metadata extraction from multipart mail into tracked `MessageRecord` attachments

## Legacy Settings Projection
- Incoming/outgoing ports are sourced from `POPPort`, `IMAPPort`, and `SMTPPort`.
- Incoming/outgoing security is projected from `*SSLUse`, `*StartTls`, and `*Security`.
- POP auth is projected from `AuthenticatePassword`, `AuthenticateAPOP`, `AuthenticateKerberos`, and `AuthenticateRPA`.
- IMAP auth is projected from `AuthenticatePassword`, `AuthenticateCRAMMD5`, and `AuthenticateKerberos`.
- Mail-management and IMAP behavior are projected from:
  - `LeaveMailOnServer`
  - `DeleteMailFromServer`
  - `SkipBigMessages`
  - `BigMessageThreshold`
  - `IMAPMaxDownloadSize`
  - `IMAPOmitAttachments`
  - `ImapDirectoryPrefix`
  - `TrashMailboxName`
  - `TransferToTrashOnDelete`
  - `CheckMailByDefault`

## Attachment Handling
- The transport slice now parses multipart message structure and persists attachment metadata:
  - file name
  - content type
  - part size
  - omitted flag
- The current tracked model stores attachment metadata, not decoded binary payloads.
- IMAP attachment omission currently means “persist metadata but do not treat the message as a fully downloaded body payload.”

## Current Honest Boundary
- Kerberos is tracked as a dependency root and build define (`HERMES_HAS_KRB5`), but POP/IMAP Kerberos auth is still surfaced as unsupported rather than emulated.
- RPA is still surfaced as unsupported rather than emulated.
- SMTP/POP/IMAP live coverage in the portable suite currently exercises:
  - SMTP `CRAM-MD5`
  - POP password auth with UIDL dedupe
  - IMAP password auth with mailbox discovery and fetch
- The Haiku shell now has Mail-command hooks and task/error surface plumbing, but runtime validation of that UI still requires a Haiku build machine.
