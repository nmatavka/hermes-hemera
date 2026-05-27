#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailTransportCoordinator.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/OpenSslTlsProvider.h"
#include "hermes/SyncStateStore.h"
#include "hermes/TransportService.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <thread>

namespace {

class FixedAccountService final : public hermes::AccountService {
public:
    explicit FixedAccountService(std::vector<hermes::AccountProfile> accounts) : accounts_(std::move(accounts)) {}

    std::vector<hermes::AccountProfile> Accounts() const override {
        return accounts_;
    }

    std::optional<hermes::AccountProfile> FindById(std::string_view id) const override {
        for (const auto& account : accounts_) {
            if (account.id == id) {
                return account;
            }
        }
        return std::nullopt;
    }

private:
    std::vector<hermes::AccountProfile> accounts_;
};

#if !defined(_WIN32)

int CreateListener(std::uint16_t* port) {
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        return -1;
    }

    int opt = 1;
    (void)::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(listener);
        return -1;
    }
    if (::listen(listener, 4) != 0) {
        ::close(listener);
        return -1;
    }

    socklen_t address_size = sizeof(address);
    if (::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size) != 0) {
        ::close(listener);
        return -1;
    }
    *port = ntohs(address.sin_port);
    return listener;
}

std::string ReadLine(int fd) {
    std::string line;
    char ch = '\0';
    while (::recv(fd, &ch, 1, 0) == 1) {
        if (ch == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        line.push_back(ch);
    }
    return line;
}

void WriteAll(int fd, std::string_view text) {
    const char* buffer = text.data();
    std::size_t remaining = text.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(fd, buffer, remaining, 0);
        if (sent <= 0) {
            return;
        }
        buffer += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

std::string ReadUntilDot(int fd) {
    std::string content;
    while (true) {
        const std::string line = ReadLine(fd);
        if (line == ".") {
            return content;
        }
        content += line;
        content += "\n";
    }
}

std::string MultipartFixtureMessage() {
    return "Subject: Attachment status\r\n"
           "From: sender@example.com\r\n"
           "To: receiver@example.com\r\n"
           "Content-Type: multipart/mixed; boundary=\"mix\"\r\n"
           "\r\n"
           "--mix\r\n"
           "Content-Type: text/plain\r\n"
           "\r\n"
           "Plain body\r\n"
           "--mix\r\n"
           "Content-Type: application/pdf; name=\"report.pdf\"\r\n"
           "Content-Disposition: attachment; filename=\"report.pdf\"\r\n"
           "\r\n"
           "PDFDATA\r\n"
           "--mix--\r\n";
}

#endif

}  // namespace

HERMES_TEST(CredentialAndSyncStoresRoundTripState) {
    hermes::tests::ScopedTempDirectory temp("hermes-transport-state");

    hermes::InMemoryCredentialStore memory_credentials;
    std::string error_message;
    HERMES_CHECK(memory_credentials.SaveCredential("primary", hermes::CredentialKind::kIncoming, "in-pass",
                                                   &error_message));
    HERMES_CHECK(memory_credentials.SaveCredential("primary", hermes::CredentialKind::kOutgoing, "out-pass",
                                                   &error_message));
    HERMES_CHECK_EQ(memory_credentials.LoadCredential("primary", hermes::CredentialKind::kIncoming).value_or(""),
                    std::string("in-pass"));
    HERMES_CHECK_EQ(memory_credentials.LoadCredential("primary", hermes::CredentialKind::kOutgoing).value_or(""),
                    std::string("out-pass"));

    hermes::FilesystemCredentialStore file_credentials(temp.Path() / "creds");
    HERMES_CHECK(file_credentials.SaveCredential("primary", hermes::CredentialKind::kIncoming, "disk-pass",
                                                 &error_message));
    HERMES_CHECK_EQ(file_credentials.LoadCredential("primary", hermes::CredentialKind::kIncoming).value_or(""),
                    std::string("disk-pass"));

    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::PopSyncState pop_state;
    pop_state.account_id = "primary";
    pop_state.uidl_to_message_id["UIDL-1"] = "msg-1";
    HERMES_CHECK(sync_store.SavePopState(pop_state, &error_message));
    const auto loaded_pop = sync_store.LoadPopState("primary");
    HERMES_CHECK(static_cast<bool>(loaded_pop));
    HERMES_CHECK_EQ(loaded_pop->uidl_to_message_id.at("UIDL-1"), std::string("msg-1"));

    hermes::ImapMailboxSyncState imap_state;
    imap_state.account_id = "primary";
    imap_state.mailbox_id = "primary:INBOX";
    imap_state.uid_validity = 777;
    imap_state.last_seen_uid = 42;
    HERMES_CHECK(sync_store.SaveImapState(imap_state, &error_message));
    const auto loaded_imap = sync_store.LoadImapState("primary", "primary:INBOX");
    HERMES_CHECK(static_cast<bool>(loaded_imap));
    HERMES_CHECK_EQ(loaded_imap->uid_validity, static_cast<std::uint64_t>(777));
    HERMES_CHECK_EQ(loaded_imap->last_seen_uid, static_cast<std::uint64_t>(42));
}

HERMES_TEST(InMemoryMailTaskModelTracksFailures) {
    hermes::InMemoryMailTaskModel tasks;
    tasks.UpsertTask({"task-1", "Primary", "Check mail", "Running", "Details", hermes::MailTaskKind::kReceiving,
                      hermes::MailTaskState::kRunning, 10, 2, true});
    HERMES_CHECK(tasks.FailTask("task-1", "Failed", "Bad password"));
    const auto all_tasks = tasks.Tasks();
    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(all_tasks.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().message, std::string("Bad password"));
}

#if !defined(_WIN32)

HERMES_TEST(MailTransportCoordinatorSendsQueuedMailAndMovesItToSent) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string captured_message;
    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "220 smtp.example.test ESMTP\r\n");
        HERMES_CHECK(ReadLine(client).rfind("EHLO hermes-hemera", 0) == 0);
        WriteAll(client, "250-localhost\r\n250-AUTH CRAM-MD5 PLAIN LOGIN\r\n250 OK\r\n");
        HERMES_CHECK(ReadLine(client) == "AUTH CRAM-MD5");
        WriteAll(client, "334 PDEyMzQ1QGV4YW1wbGUuY29tPg==\r\n");
        HERMES_CHECK(!ReadLine(client).empty());
        WriteAll(client, "235 Authentication successful\r\n");
        HERMES_CHECK(ReadLine(client).rfind("MAIL FROM:<sender@example.com>", 0) == 0);
        WriteAll(client, "250 Sender ok\r\n");
        HERMES_CHECK(ReadLine(client).rfind("RCPT TO:<receiver@example.com>", 0) == 0);
        WriteAll(client, "250 Recipient ok\r\n");
        HERMES_CHECK(ReadLine(client) == "DATA");
        WriteAll(client, "354 End with .\r\n");
        captured_message = ReadUntilDot(client);
        WriteAll(client, "250 Queued\r\n");
        HERMES_CHECK(ReadLine(client) == "QUIT");
        WriteAll(client, "221 Bye\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "primary";
    account.display_name = "Primary";
    account.login_name = "sender";
    account.email_address = "sender@example.com";
    account.outgoing_server = "127.0.0.1";
    account.outgoing_port = port;
    account.smtp_auth = hermes::SmtpAuthMode::kCramMd5;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hermes-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hermes-smtp");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("primary", hermes::CredentialKind::kOutgoing, "smtp-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "primary", hermes::MailboxProtocol::kSmtp, "", false, true, 0}, &error_message));

    hermes::MessageRecord queued;
    queued.id = "queued-1";
    queued.mailbox_id = "out";
    queued.account_id = "primary";
    queued.subject = "Hello transport";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "Queued body";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
    HERMES_CHECK(captured_message.find("Subject: Hello transport") != std::string::npos);
    HERMES_CHECK(!message_store.GetMessage("out", "queued-1"));
    const auto sent = message_store.GetMessage("sent", "queued-1");
    HERMES_CHECK(static_cast<bool>(sent));
    HERMES_CHECK_EQ(sent->delivery_state, hermes::MessageDeliveryState::kSent);

    server.join();
}

HERMES_TEST(MailTransportCoordinatorFetchesPopMailOnceUsingUidlState) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::atomic<int> retr_count{0};
    std::thread server([&]() {
        for (int session = 0; session < 2; ++session) {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-1\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 256\r\n.\r\n");
            const std::string maybe_retr = ReadLine(client);
            if (maybe_retr == "RETR 1") {
                ++retr_count;
                WriteAll(client, "+OK retr\r\n");
                WriteAll(client, MultipartFixtureMessage());
                WriteAll(client, ".\r\n");
                HERMES_CHECK(ReadLine(client) == "DELE 1");
                WriteAll(client, "+OK deleted\r\n");
                HERMES_CHECK(ReadLine(client) == "QUIT");
            } else {
                HERMES_CHECK(maybe_retr == "QUIT");
            }
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        }
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "pop";
    account.display_name = "POP";
    account.login_name = "pop-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hermes-pop");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop", hermes::CredentialKind::kIncoming, "pop-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto first = coordinator.CheckMail();
    HERMES_CHECK(first.success);
    HERMES_CHECK_EQ(first.messages_received, static_cast<std::size_t>(1));
    const auto inbox_messages = message_store.ListMessages("inbox");
    HERMES_CHECK_EQ(inbox_messages.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(inbox_messages.front().attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(inbox_messages.front().attachments.front().name, std::string("report.pdf"));

    const auto second = coordinator.CheckMail();
    HERMES_CHECK(second.success);
    HERMES_CHECK_EQ(second.messages_received, static_cast<std::size_t>(0));
    HERMES_CHECK_EQ(retr_count.load(), 1);

    server.join();
}

HERMES_TEST(MailTransportCoordinatorDiscoversImapMailboxesAndDownloadsMessages) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::thread server([&]() {
        for (int session = 0; session < 3; ++session) {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "* OK imap.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
            WriteAll(client, "A1 OK LOGIN completed\r\n");
            if (session == 0) {
                HERMES_CHECK(ReadLine(client).find("LIST \"\" \"*\"") != std::string::npos);
                WriteAll(client, "* LIST () \"/\" \"INBOX\"\r\n");
                WriteAll(client, "* LIST () \"/\" \"Projects\"\r\n");
                WriteAll(client, "A2 OK LIST completed\r\n");
            } else if (session == 1) {
                HERMES_CHECK(ReadLine(client).find("SELECT \"INBOX\"") != std::string::npos);
                WriteAll(client, "* 1 EXISTS\r\n");
                WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
                WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
                HERMES_CHECK(ReadLine(client).find("UID FETCH 1:*") != std::string::npos);
                const std::string payload = MultipartFixtureMessage();
                WriteAll(client, "* 1 FETCH (UID 101 FLAGS (\\Seen) BODY[] {" +
                                     std::to_string(payload.size()) + "}\r\n");
                WriteAll(client, payload);
                WriteAll(client, ")\r\n");
                WriteAll(client, "A3 OK UID FETCH completed\r\n");
            } else {
                HERMES_CHECK(ReadLine(client).find("SELECT \"Projects\"") != std::string::npos);
                WriteAll(client, "* 0 EXISTS\r\n");
                WriteAll(client, "* OK [UIDVALIDITY 888] UIDs valid\r\n");
                WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
                HERMES_CHECK(ReadLine(client).find("UID FETCH 1:*") != std::string::npos);
                WriteAll(client, "A3 OK UID FETCH completed\r\n");
            }
            ::close(client);
        }
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = true;
    account.imap_auth = hermes::ImapAuthMode::kPassword;
    account.imap_omit_attachments = true;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hermes-imap");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.mailboxes_discovered, static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));

    const auto mailboxes = mailbox_store.ListMailboxes();
    HERMES_CHECK(mailbox_store.GetMailbox("imap:INBOX").has_value());
    HERMES_CHECK(mailbox_store.GetMailbox("imap:Projects").has_value());
    const auto inbox_message = message_store.GetMessage("imap:INBOX", "imap-imap-imap-INBOX-101");
    HERMES_CHECK(static_cast<bool>(inbox_message));
    HERMES_CHECK_EQ(inbox_message->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(inbox_message->attachments_omitted);

    server.join();
}

#endif
