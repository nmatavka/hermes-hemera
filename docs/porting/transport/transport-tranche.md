# Transport Tranche Notes

## Scope Landed
- Live queued SMTP send through `MailTransportCoordinator`
- Live POP retrieval with UIDL dedupe through `SyncStateStore`
- Live IMAP mailbox discovery plus incremental `UID FETCH` sync
- Offline IMAP action journaling for delete, undelete, move, copy, mailbox create/rename/delete, and manual fetch
- POSIX socket transport with line reads, implicit TLS, and STARTTLS upgrade seams
- OpenSSL-backed client TLS session use through `OpenSslTlsProvider`
- Filesystem-backed credentials, POP/IMAP sync state, and richer message/mailbox metadata
- Task tracking through `MailTaskModel`
- Attachment payload persistence under app-local `Attachments/<message-id>/...`
- Compose, draft, queue, and SMTP attachment handling with nested multipart send

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
- The transport slice now parses multipart message structure and persists attachment metadata and payload paths:
  - file name
  - content type
  - part size
  - omitted flag
  - download state
  - fetch error state
- POP and IMAP save downloaded attachment payloads into the tracked message store.
- IMAP attachment omission now means “persist metadata, mark the attachment incomplete, and allow later fetch through the IMAP action journal.”

## Current Honest Boundary
- Kerberos is tracked as a dependency root and build define (`HERMES_HAS_KRB5`), and the POP/IMAP GSSAPI paths are now wired through tracked exchange code, but they still need live runtime validation against a real Kerberos environment.
- RPA is still surfaced as unsupported rather than emulated.
- SMTP/POP/IMAP live coverage in the portable suite currently exercises:
  - SMTP `CRAM-MD5`
  - POP password auth with UIDL dedupe
  - IMAP password auth with mailbox discovery and fetch
- The Haiku shell now has Mail-command hooks, task/error plumbing, and attachment-aware workflow scaffolding, but runtime validation of that UI still requires a Haiku build machine.
