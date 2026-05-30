#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/GssapiAuthenticator.h"
#include "hermes/ImapActionStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailTransportCoordinator.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/OAuthSupport.h"
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
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
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

    void SetAccounts(std::vector<hermes::AccountProfile> accounts) override {
        accounts_ = std::move(accounts);
    }

    void AddOrReplace(const hermes::AccountProfile& account) override {
        for (auto& existing : accounts_) {
            if (existing.id == account.id) {
                existing = account;
                return;
            }
        }
        accounts_.push_back(account);
    }

    bool Remove(std::string_view id) override {
        const auto it = std::remove_if(accounts_.begin(), accounts_.end(), [&](const hermes::AccountProfile& account) {
            return account.id == id;
        });
        if (it == accounts_.end()) {
            return false;
        }
        accounts_.erase(it, accounts_.end());
        return true;
    }

    bool SaveToSettings(hermes::SettingsStore&, std::string* = nullptr) const override {
        return false;
    }

    bool SaveToIniFile(const std::filesystem::path&, std::string* = nullptr) const override {
        return false;
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

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

HttpRequest ReadHttpRequest(int fd) {
    HttpRequest request;
    const std::string request_line = ReadLine(fd);
    const std::size_t method_end = request_line.find(' ');
    const std::size_t path_end = request_line.find(' ', method_end + 1);
    request.method = method_end == std::string::npos ? request_line : request_line.substr(0, method_end);
    request.path =
        (method_end == std::string::npos || path_end == std::string::npos) ? std::string() : request_line.substr(method_end + 1, path_end - method_end - 1);

    std::size_t content_length = 0;
    while (true) {
        const std::string line = ReadLine(fd);
        if (line.empty()) {
            break;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') {
            value.erase(value.begin());
        }
        request.headers.emplace(key, value);
        if (key == "Content-Length") {
            content_length = static_cast<std::size_t>(std::strtoull(value.c_str(), nullptr, 10));
        }
    }

    request.body.resize(content_length);
    std::size_t received = 0;
    while (received < content_length) {
        const ssize_t count = ::recv(fd, request.body.data() + received, content_length - received, 0);
        if (count <= 0) {
            break;
        }
        received += static_cast<std::size_t>(count);
    }
    request.body.resize(received);
    return request;
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

std::string RtfFixtureMessage() {
    return "Subject: Rich body\r\n"
           "From: sender@example.com\r\n"
           "To: receiver@example.com\r\n"
           "Content-Type: application/rtf\r\n"
           "\r\n"
           "{\\rtf1\\ansi Rich \\b body\\b0}\r\n";
}

std::string Base64Encode(const std::string& input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((input.size() + 2) / 3) * 4);

    int val = 0;
    int valb = -6;
    for (unsigned char ch : input) {
        val = (val << 8) + ch;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(kAlphabet[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(kAlphabet[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded.size() % 4 != 0) {
        encoded.push_back('=');
    }
    return encoded;
}

class FakeGssapiConversation final : public hermes::GssapiConversation {
public:
    struct Config {
        std::vector<hermes::GssapiStepResult> steps;
        std::vector<std::string> expected_inputs;
        std::string unwrap_output = std::string("\1\0\0\0", 4);
        std::string wrap_output = "wrapped-final";
        hermes::MailTaskErrorKind step_failure_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
        std::string step_failure_message = "GSSAPI step failed.";
        hermes::MailTaskErrorKind unwrap_failure_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
        std::string unwrap_failure_message = "GSSAPI unwrap failed.";
        hermes::MailTaskErrorKind wrap_failure_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
        std::string wrap_failure_message = "GSSAPI wrap failed.";
        int fail_step_index = -1;
        bool fail_unwrap = false;
        bool fail_wrap = false;
    };

    explicit FakeGssapiConversation(Config config) : config_(std::move(config)) {}

    bool Step(std::string_view input_token,
              hermes::GssapiStepResult* result,
              hermes::MailTaskErrorKind* error_kind,
              std::string* error_message) override {
        step_inputs_.push_back(std::string(input_token));
        if (step_index_ < config_.expected_inputs.size() &&
            config_.expected_inputs[step_index_] != input_token) {
            if (error_kind) {
                *error_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = "Unexpected GSSAPI input token.";
            }
            return false;
        }
        if (config_.fail_step_index >= 0 && static_cast<int>(step_index_) == config_.fail_step_index) {
            if (error_kind) {
                *error_kind = config_.step_failure_kind;
            }
            if (error_message) {
                *error_message = config_.step_failure_message;
            }
            return false;
        }
        if (step_index_ >= config_.steps.size()) {
            if (error_kind) {
                *error_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = "Unexpected extra GSSAPI step.";
            }
            return false;
        }
        if (result) {
            *result = config_.steps[step_index_];
        }
        ++step_index_;
        return true;
    }

    bool Unwrap(std::string_view input_token,
                std::string* output_token,
                hermes::MailTaskErrorKind* error_kind,
                std::string* error_message) override {
        unwrap_inputs_.push_back(std::string(input_token));
        if (config_.fail_unwrap) {
            if (error_kind) {
                *error_kind = config_.unwrap_failure_kind;
            }
            if (error_message) {
                *error_message = config_.unwrap_failure_message;
            }
            return false;
        }
        if (output_token) {
            *output_token = config_.unwrap_output;
        }
        return true;
    }

    bool Wrap(std::string_view input_token,
              std::string* output_token,
              hermes::MailTaskErrorKind* error_kind,
              std::string* error_message) override {
        wrap_inputs_.push_back(std::string(input_token));
        if (config_.fail_wrap) {
            if (error_kind) {
                *error_kind = config_.wrap_failure_kind;
            }
            if (error_message) {
                *error_message = config_.wrap_failure_message;
            }
            return false;
        }
        if (output_token) {
            *output_token = config_.wrap_output;
        }
        return true;
    }

    const std::vector<std::string>& StepInputs() const { return step_inputs_; }

private:
    Config config_;
    std::size_t step_index_ = 0;
    std::vector<std::string> step_inputs_;
    std::vector<std::string> unwrap_inputs_;
    std::vector<std::string> wrap_inputs_;
};

class FakeGssapiEngine final : public hermes::GssapiEngine {
public:
    FakeGssapiConversation::Config config;
    bool available = true;
    hermes::MailTaskErrorKind unavailable_kind = hermes::MailTaskErrorKind::kKerberosUnavailable;
    std::string unavailable_message = "Kerberos support is unavailable in this build.";
    bool fail_create = false;
    hermes::MailTaskErrorKind create_failure_kind = hermes::MailTaskErrorKind::kServicePrincipalFailure;
    std::string create_failure_message = "Service principal creation failed.";
    mutable std::vector<std::string> principals;
    mutable std::size_t create_count = 0;

    bool IsAvailable(hermes::MailTaskErrorKind* error_kind, std::string* error_message) const override {
        if (available) {
            return true;
        }
        if (error_kind) {
            *error_kind = unavailable_kind;
        }
        if (error_message) {
            *error_message = unavailable_message;
        }
        return false;
    }

    std::unique_ptr<hermes::GssapiConversation> CreateConversation(
        std::string_view service_principal,
        hermes::MailTaskErrorKind* error_kind,
        std::string* error_message) const override {
        principals.push_back(std::string(service_principal));
        if (fail_create) {
            if (error_kind) {
                *error_kind = create_failure_kind;
            }
            if (error_message) {
                *error_message = create_failure_message;
            }
            return nullptr;
        }
        ++create_count;
        return std::make_unique<FakeGssapiConversation>(config);
    }
};

#endif

}  // namespace

HERMES_TEST(CredentialAndSyncStoresRoundTripState) {
    hermes::tests::ScopedTempDirectory temp("hemera-transport-state");

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
    pop_state.uidl_to_server_status["UIDL-1"] = hermes::PopServerStatus::kFetchDelete;
    HERMES_CHECK(sync_store.SavePopState(pop_state, &error_message));
    const auto loaded_pop = sync_store.LoadPopState("primary");
    HERMES_CHECK(static_cast<bool>(loaded_pop));
    HERMES_CHECK_EQ(loaded_pop->uidl_to_message_id.at("UIDL-1"), std::string("msg-1"));
    HERMES_CHECK_EQ(loaded_pop->uidl_to_server_status.at("UIDL-1"), hermes::PopServerStatus::kFetchDelete);

    hermes::ImapMailboxSyncState imap_state;
    imap_state.account_id = "primary";
    imap_state.mailbox_id = "primary:INBOX";
    imap_state.uid_validity = 777;
    imap_state.last_seen_uid = 42;
    imap_state.auto_sync = false;
    imap_state.show_deleted = true;
    HERMES_CHECK(sync_store.SaveImapState(imap_state, &error_message));
    const auto loaded_imap = sync_store.LoadImapState("primary", "primary:INBOX");
    HERMES_CHECK(static_cast<bool>(loaded_imap));
    HERMES_CHECK_EQ(loaded_imap->uid_validity, static_cast<std::uint64_t>(777));
    HERMES_CHECK_EQ(loaded_imap->last_seen_uid, static_cast<std::uint64_t>(42));
    HERMES_CHECK_EQ(loaded_imap->auto_sync, false);
    HERMES_CHECK_EQ(loaded_imap->show_deleted, true);
}

HERMES_TEST(InMemoryMailTaskModelTracksFailures) {
    hermes::InMemoryMailTaskModel tasks;
    tasks.UpsertTask({"task-1", "Primary", "Check mail", "Running", "Details", hermes::MailTaskKind::kReceiving,
                      hermes::MailTaskState::kRunning, 10, 2, true});
    HERMES_CHECK(tasks.FailTask("task-1",
                                "Failed",
                                "Bad password",
                                hermes::MailTaskErrorKind::kCredentialRejected,
                                "LOGIN"));
    const auto all_tasks = tasks.Tasks();
    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(all_tasks.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().message, std::string("Bad password"));
    HERMES_CHECK_EQ(errors.front().kind, hermes::MailTaskErrorKind::kCredentialRejected);
    HERMES_CHECK_EQ(errors.front().mechanism, std::string("LOGIN"));
}

HERMES_TEST(Krb5HeadersPreferProjectRootWhenAvailable) {
    const std::filesystem::path krb5_header =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "third_party" / "krb5" / "src" / "include" / "krb5.h";
    if (std::filesystem::exists(krb5_header)) {
        HERMES_CHECK_EQ(HERMES_KRB5_HEADERS_FROM_ROOT, 1);
    } else {
        HERMES_CHECK_EQ(HERMES_KRB5_HEADERS_FROM_ROOT, 0);
    }
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
        HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
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
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp");
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
    queued.plain_text_body = "Queued body = receipt";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    queued.compose_options.priority = hermes::ComposePriority::kHigh;
    queued.compose_options.request_read_receipt = true;
    queued.compose_options.quoted_printable = true;
    queued.use_legacy_return_receipt_header = true;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    try {
        const auto summary = coordinator.SendQueued();
        HERMES_CHECK(summary.success);
        HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
        HERMES_CHECK(captured_message.find("Subject: Hello transport") != std::string::npos);
        HERMES_CHECK(captured_message.find("X-Priority: 2 (High)") != std::string::npos);
        HERMES_CHECK(captured_message.find("Importance: high") != std::string::npos);
        HERMES_CHECK(captured_message.find("Disposition-Notification-To: sender@example.com") !=
                     std::string::npos);
        HERMES_CHECK(captured_message.find("Return-Receipt-To: sender@example.com") != std::string::npos);
        HERMES_CHECK(captured_message.find("Content-Transfer-Encoding: quoted-printable") !=
                     std::string::npos);
        HERMES_CHECK(captured_message.find("Queued=20body=20=3D=20receipt") != std::string::npos);
        const auto out_after = message_store.GetMessage("out", "queued-1");
        const auto sent = message_store.GetMessage("sent", "queued-1");
        HERMES_CHECK(!out_after);
        HERMES_CHECK(static_cast<bool>(sent));
        HERMES_CHECK_EQ(sent->delivery_state, hermes::MessageDeliveryState::kSent);
        server.join();
    } catch (...) {
        if (server.joinable()) {
            server.join();
        }
        throw;
    }
}

HERMES_TEST(MailTransportCoordinatorKeepsCopiesInOutWhenRequested) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "220 smtp.example.test ESMTP\r\n");
        HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
        WriteAll(client, "250-localhost\r\n250-AUTH LOGIN\r\n250 OK\r\n");
        HERMES_CHECK(ReadLine(client) == "AUTH LOGIN");
        WriteAll(client, "334 VXNlcm5hbWU6\r\n");
        HERMES_CHECK(!ReadLine(client).empty());
        WriteAll(client, "334 UGFzc3dvcmQ6\r\n");
        HERMES_CHECK(!ReadLine(client).empty());
        WriteAll(client, "235 Authentication successful\r\n");
        HERMES_CHECK(ReadLine(client).rfind("MAIL FROM:<sender@example.com>", 0) == 0);
        WriteAll(client, "250 Sender ok\r\n");
        HERMES_CHECK(ReadLine(client).rfind("RCPT TO:<receiver@example.com>", 0) == 0);
        WriteAll(client, "250 Recipient ok\r\n");
        HERMES_CHECK(ReadLine(client) == "DATA");
        WriteAll(client, "354 End with .\r\n");
        (void)ReadUntilDot(client);
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
    account.smtp_auth = hermes::SmtpAuthMode::kLogin;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-keep-copies");
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
    queued.id = "queued-keep";
    queued.mailbox_id = "out";
    queued.account_id = "primary";
    queued.subject = "Keep a copy";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "Keep this source in out.";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    queued.compose_options.keep_copies = true;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));

    const auto out = message_store.GetMessage("out", "queued-keep");
    const auto sent = message_store.GetMessage("sent", "queued-keep");
    HERMES_CHECK(static_cast<bool>(out));
    HERMES_CHECK(static_cast<bool>(sent));
    HERMES_CHECK_EQ(out->delivery_state, hermes::MessageDeliveryState::kSent);
    HERMES_CHECK_EQ(sent->delivery_state, hermes::MessageDeliveryState::kSent);

    server.join();
}

HERMES_TEST(MailTransportCoordinatorExecutesFilteredSendQueuedRequests) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::atomic<int> accepted_messages = 0;
    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "220 smtp.example.test ESMTP\r\n");
        HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
        WriteAll(client, "250-localhost\r\n250-AUTH LOGIN\r\n250 OK\r\n");
        HERMES_CHECK(ReadLine(client) == "AUTH LOGIN");
        WriteAll(client, "334 VXNlcm5hbWU6\r\n");
        HERMES_CHECK(!ReadLine(client).empty());
        WriteAll(client, "334 UGFzc3dvcmQ6\r\n");
        HERMES_CHECK(!ReadLine(client).empty());
        WriteAll(client, "235 Authentication successful\r\n");
        HERMES_CHECK(ReadLine(client).rfind("MAIL FROM:<sender@example.com>", 0) == 0);
        WriteAll(client, "250 Sender ok\r\n");
        HERMES_CHECK(ReadLine(client).rfind("RCPT TO:<receiver@example.com>", 0) == 0);
        WriteAll(client, "250 Recipient ok\r\n");
        HERMES_CHECK(ReadLine(client) == "DATA");
        WriteAll(client, "354 End with .\r\n");
        (void)ReadUntilDot(client);
        ++accepted_messages;
        WriteAll(client, "250 Queued\r\n");
        HERMES_CHECK(ReadLine(client) == "QUIT");
        WriteAll(client, "221 Bye\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile primary;
    primary.id = "primary";
    primary.display_name = "Primary";
    primary.login_name = "sender";
    primary.email_address = "sender@example.com";
    primary.outgoing_server = "127.0.0.1";
    primary.outgoing_port = port;
    primary.smtp_auth = hermes::SmtpAuthMode::kLogin;

    hermes::AccountProfile secondary = primary;
    secondary.id = "secondary";
    secondary.display_name = "Secondary";

    FixedAccountService accounts({primary, secondary});
    hermes::InMemoryCredentialStore credentials;
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-filtered-send");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("primary", hermes::CredentialKind::kOutgoing, "smtp-pass",
                                            &error_message));
    HERMES_CHECK(credentials.SaveCredential("secondary",
                                            hermes::CredentialKind::kOutgoing,
                                            "smtp-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "primary", hermes::MailboxProtocol::kSmtp, "", false, true, 0},
        &error_message));

    hermes::MessageRecord queued_primary;
    queued_primary.id = "queued-primary";
    queued_primary.mailbox_id = "out";
    queued_primary.account_id = "primary";
    queued_primary.subject = "Primary queued";
    queued_primary.sender = "sender@example.com";
    queued_primary.recipients = "receiver@example.com";
    queued_primary.plain_text_body = "Primary body";
    queued_primary.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued_primary, &error_message));

    hermes::MessageRecord queued_secondary = queued_primary;
    queued_secondary.id = "queued-secondary";
    queued_secondary.account_id = "secondary";
    queued_secondary.subject = "Secondary queued";
    HERMES_CHECK(message_store.SaveMessage(queued_secondary, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    hermes::MailTransferRequest request;
    request.send_queued = true;
    request.selected_account_ids = {"primary"};
    const auto summary = coordinator.ExecuteMailTransfer(request);
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(accepted_messages.load(), 1);

    const auto sent_primary = message_store.GetMessage("sent", "queued-primary");
    const auto remaining_secondary = message_store.GetMessage("out", "queued-secondary");
    HERMES_CHECK(static_cast<bool>(sent_primary));
    HERMES_CHECK(static_cast<bool>(remaining_secondary));
    HERMES_CHECK_EQ(remaining_secondary->delivery_state, hermes::MessageDeliveryState::kQueued);

    server.join();
}

HERMES_TEST(MailTransportCoordinatorFailsQueuedMailWhenLegacyAttachmentEncodingIsRequested) {
    hermes::AccountProfile account;
    account.id = "primary";
    account.display_name = "Primary";
    account.login_name = "sender";
    account.email_address = "sender@example.com";
    account.outgoing_server = "127.0.0.1";
    account.outgoing_port = 2525;
    account.smtp_auth = hermes::SmtpAuthMode::kPlain;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-legacy-encoding");
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
    queued.id = "queued-legacy-encoding";
    queued.mailbox_id = "out";
    queued.account_id = "primary";
    queued.subject = "Legacy encoding";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "This should fail.";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    queued.compose_options.attachment_encoding = hermes::AttachmentEncodingMode::kBinHex;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(0));
    HERMES_CHECK_EQ(summary.warnings.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(summary.warnings.front().find("not implemented") != std::string::npos);

    const auto failed = message_store.GetMessage("out", "queued-legacy-encoding");
    HERMES_CHECK(static_cast<bool>(failed));
    HERMES_CHECK_EQ(failed->delivery_state, hermes::MessageDeliveryState::kFailed);
    HERMES_CHECK(failed->last_error.find("not implemented") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorSendsQueuedMailWithAttachmentsAsMultipartMixed) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string captured_message;
    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "220 smtp.example.test ESMTP\r\n");
        HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
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
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-unused"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-attachments");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    const auto attachment_path = temp.Path() / "report.txt";
    {
        std::ofstream output(attachment_path, std::ios::binary);
        output << "payload-data";
    }

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("primary", hermes::CredentialKind::kOutgoing, "smtp-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "primary", hermes::MailboxProtocol::kSmtp, "", false, true, 0}, &error_message));

    hermes::MessageRecord queued;
    queued.id = "queued-attachment";
    queued.mailbox_id = "out";
    queued.account_id = "primary";
    queued.subject = "Attachment transport";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "Plain body";
    queued.html_body = "<p>Styled body</p>";
    queued.styled_source = hermes::StyledDocumentSource::kHtml;
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    queued.attachments.push_back({"report.txt",
                                  "text/plain",
                                  12,
                                  false,
                                  attachment_path.string(),
                                  "cid-report",
                                  "attachment",
                                  true,
                                  ""});
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
    HERMES_CHECK(captured_message.find("Content-Type: multipart/mixed; boundary=\"hemera-mixed\"") !=
                 std::string::npos);
    HERMES_CHECK(captured_message.find("Content-Type: multipart/alternative; boundary=\"hemera-alternative\"") !=
                 std::string::npos);
    HERMES_CHECK(captured_message.find("Content-Disposition: attachment; filename=\"report.txt\"") !=
                 std::string::npos);
    HERMES_CHECK(captured_message.find("Content-ID: <cid-report>") != std::string::npos);
    HERMES_CHECK(captured_message.find("cGF5bG9hZC1kYXRh") != std::string::npos);

    server.join();
}

HERMES_TEST(MailTransportCoordinatorDoesNotSendHelperHtmlForPlainMessages) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string captured_message;
    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "220 smtp.example.test ESMTP\r\n");
            HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
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
        } catch (const std::exception& ex) {
            server_error = ex.what();
        }
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
    hermes::FilesystemSyncStateStore sync_store(hermes::tests::UniqueTempPath("hemera-sync-helper-html"));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-helper-html");
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
    queued.id = "queued-helper-html";
    queued.mailbox_id = "out";
    queued.account_id = "primary";
    queued.subject = "Plain helper";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "Plain body";
    queued.html_body = "<p>Helper only</p>";
    queued.rtf_body = "{\\rtf1\\ansi Plain body}";
    queued.styled_source = hermes::StyledDocumentSource::kPlainText;
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    try {
        const auto summary = coordinator.SendQueued();
        HERMES_CHECK(summary.success);
        HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
        server.join();
        HERMES_CHECK(server_error.empty());
        HERMES_CHECK(captured_message.find("Content-Type: multipart/alternative; boundary=\"hemera-alternative\"") ==
                     std::string::npos);
        HERMES_CHECK(captured_message.find("Content-Type: text/plain; charset=UTF-8") != std::string::npos);
        HERMES_CHECK(captured_message.find("Content-Type: text/html; charset=UTF-8") == std::string::npos);
    } catch (...) {
        if (server.joinable()) {
            server.join();
        }
        throw;
    }
}

HERMES_TEST(MailTransportCoordinatorFetchesPopMailOnceUsingUidlState) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::atomic<int> retr_count{0};
    std::string server_error;
    std::thread server([&]() {
        try {
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
                    const std::string maybe_dele = ReadLine(client);
                    if (maybe_dele != "DELE 1") {
                        throw std::runtime_error("Expected DELE 1, received: " + maybe_dele);
                    }
                    WriteAll(client, "+OK deleted\r\n");
                    HERMES_CHECK(ReadLine(client) == "QUIT");
                } else {
                    HERMES_CHECK(maybe_retr == "QUIT");
                }
                WriteAll(client, "+OK bye\r\n");
                ::close(client);
            }
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    hermes::tests::ScopedTempDirectory temp("hemera-pop");
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
    try {
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
        HERMES_CHECK(server_error.empty());
    } catch (...) {
        if (server.joinable()) {
            server.join();
        }
        throw;
    }
}

HERMES_TEST(MailTransportCoordinatorFetchesPopHeadersWithoutRetrievingBodies) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-HEADERS\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 256\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "TOP 1 0");
            WriteAll(client,
                     "+OK headers\r\n"
                     "Subject: Header only\r\n"
                     "From: sender@example.com\r\n"
                     "To: receiver@example.com\r\n"
                     "\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    account.leave_mail_on_server = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-headers");
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
    hermes::MailTransferRequest request;
    request.fetch_headers = true;
    const auto summary = coordinator.ExecuteMailTransfer(request);
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));

    const auto inbox_messages = message_store.ListMessages("inbox");
    HERMES_CHECK_EQ(inbox_messages.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(inbox_messages.front().subject, std::string("Header only"));
    HERMES_CHECK_EQ(inbox_messages.front().download_complete, false);
    HERMES_CHECK_EQ(inbox_messages.front().plain_text_body, std::string());
    HERMES_CHECK_EQ(inbox_messages.front().pop_server_status, hermes::PopServerStatus::kLeave);

    const auto pop_state = sync_store.LoadPopState("pop");
    HERMES_CHECK(static_cast<bool>(pop_state));
    HERMES_CHECK_EQ(pop_state->uidl_to_server_status.at("UIDL-HEADERS"), hermes::PopServerStatus::kLeave);

    server.join();
    HERMES_CHECK(server_error.empty());
}

HERMES_TEST(MailTransportCoordinatorExecutesRetrieveMarkedAndDeleteMarkedPopRequests) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-A-FETCH\r\n2 UIDL-B-DELETE\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "RETR 1");
            WriteAll(client,
                     "+OK retr\r\n"
                     "Subject: Retrieved marked\r\n"
                     "From: sender@example.com\r\n"
                     "To: receiver@example.com\r\n"
                     "\r\n"
                     "Retrieved body\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "DELE 2");
            WriteAll(client, "+OK deleted\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 128\r\n2 64\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    account.leave_mail_on_server = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-marked");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop", hermes::CredentialKind::kIncoming, "pop-pass",
                                            &error_message));

    hermes::MessageRecord fetch_stub;
    fetch_stub.id = "pop-pop-UIDL-A-FETCH";
    fetch_stub.mailbox_id = "inbox";
    fetch_stub.account_id = "pop";
    fetch_stub.remote_id = "UIDL-A-FETCH";
    fetch_stub.remote_mailbox = "INBOX";
    fetch_stub.subject = "Old";
    fetch_stub.pop_server_status = hermes::PopServerStatus::kFetch;
    HERMES_CHECK(message_store.SaveMessage(fetch_stub, &error_message));

    hermes::MessageRecord delete_stub = fetch_stub;
    delete_stub.id = "pop-pop-UIDL-B-DELETE";
    delete_stub.remote_id = "UIDL-B-DELETE";
    delete_stub.subject = "Delete me";
    delete_stub.pop_server_status = hermes::PopServerStatus::kDelete;
    HERMES_CHECK(message_store.SaveMessage(delete_stub, &error_message));

    hermes::PopSyncState state;
    state.account_id = "pop";
    state.uidl_to_message_id["UIDL-A-FETCH"] = fetch_stub.id;
    state.uidl_to_message_id["UIDL-B-DELETE"] = delete_stub.id;
    state.uidl_to_server_status["UIDL-A-FETCH"] = hermes::PopServerStatus::kFetch;
    state.uidl_to_server_status["UIDL-B-DELETE"] = hermes::PopServerStatus::kDelete;
    HERMES_CHECK(sync_store.SavePopState(state, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    hermes::MailTransferRequest request;
    request.retrieve_marked = true;
    request.delete_marked = true;
    const auto summary = coordinator.ExecuteMailTransfer(request);
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));

    const auto fetched = message_store.GetMessage("inbox", fetch_stub.id);
    HERMES_CHECK(static_cast<bool>(fetched));
    HERMES_CHECK_EQ(fetched->subject, std::string("Retrieved marked"));
    HERMES_CHECK_EQ(fetched->plain_text_body.find("Retrieved body") != std::string::npos, true);
    HERMES_CHECK_EQ(fetched->pop_server_status, hermes::PopServerStatus::kLeave);

    const auto deleted = message_store.GetMessage("inbox", delete_stub.id);
    HERMES_CHECK(static_cast<bool>(deleted));
    HERMES_CHECK_EQ(deleted->pop_server_status, hermes::PopServerStatus::kNone);

    const auto updated_state = sync_store.LoadPopState("pop");
    HERMES_CHECK(static_cast<bool>(updated_state));
    HERMES_CHECK_EQ(updated_state->uidl_to_server_status.at("UIDL-A-FETCH"), hermes::PopServerStatus::kLeave);
    HERMES_CHECK(updated_state->uidl_to_server_status.find("UIDL-B-DELETE") ==
                 updated_state->uidl_to_server_status.end());
    HERMES_CHECK(updated_state->uidl_to_message_id.find("UIDL-B-DELETE") ==
                 updated_state->uidl_to_message_id.end());

    server.join();
    HERMES_CHECK(server_error.empty());
}

HERMES_TEST(MailTransportCoordinatorDeletesRetrievedPopMailWhenRequested) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-DELETE-AFTER-FETCH\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 128\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "RETR 1");
            WriteAll(client,
                     "+OK retr\r\n"
                     "Subject: Delete after fetch\r\n"
                     "From: sender@example.com\r\n"
                     "To: receiver@example.com\r\n"
                     "\r\n"
                     "Body\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "DELE 1");
            WriteAll(client, "+OK deleted\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    account.leave_mail_on_server = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-delete-retrieved");
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
    hermes::MailTransferRequest request;
    request.retrieve_new = true;
    request.delete_retrieved = true;
    const auto summary = coordinator.ExecuteMailTransfer(request);
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));

    const auto inbox_messages = message_store.ListMessages("inbox");
    HERMES_CHECK_EQ(inbox_messages.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(inbox_messages.front().pop_server_status, hermes::PopServerStatus::kNone);

    const auto pop_state = sync_store.LoadPopState("pop");
    HERMES_CHECK(static_cast<bool>(pop_state));
    HERMES_CHECK(pop_state->uidl_to_server_status.find("UIDL-DELETE-AFTER-FETCH") ==
                 pop_state->uidl_to_server_status.end());

    server.join();
    HERMES_CHECK(server_error.empty());
}

HERMES_TEST(MailTransportCoordinatorDeletesAllTrackedPopMailWhenRequested) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-ONE\r\n2 UIDL-TWO\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "DELE 1");
            WriteAll(client, "+OK deleted\r\n");
            HERMES_CHECK(ReadLine(client) == "DELE 2");
            WriteAll(client, "+OK deleted\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 128\r\n2 64\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    account.leave_mail_on_server = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-delete-all");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop", hermes::CredentialKind::kIncoming, "pop-pass",
                                            &error_message));

    hermes::MessageRecord local_record;
    local_record.id = "pop-pop-UIDL-ONE";
    local_record.mailbox_id = "inbox";
    local_record.account_id = "pop";
    local_record.remote_id = "UIDL-ONE";
    local_record.remote_mailbox = "INBOX";
    local_record.pop_server_status = hermes::PopServerStatus::kLeave;
    HERMES_CHECK(message_store.SaveMessage(local_record, &error_message));

    hermes::PopSyncState state;
    state.account_id = "pop";
    state.uidl_to_message_id["UIDL-ONE"] = local_record.id;
    state.uidl_to_server_status["UIDL-ONE"] = hermes::PopServerStatus::kLeave;
    HERMES_CHECK(sync_store.SavePopState(state, &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    hermes::MailTransferRequest request;
    request.delete_all = true;
    const auto summary = coordinator.ExecuteMailTransfer(request);
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(0));

    const auto updated_local = message_store.GetMessage("inbox", local_record.id);
    HERMES_CHECK(static_cast<bool>(updated_local));
    HERMES_CHECK_EQ(updated_local->pop_server_status, hermes::PopServerStatus::kNone);

    const auto updated_state = sync_store.LoadPopState("pop");
    HERMES_CHECK(static_cast<bool>(updated_state));
    HERMES_CHECK(updated_state->uidl_to_message_id.empty());
    HERMES_CHECK(updated_state->uidl_to_server_status.empty());

    server.join();
    HERMES_CHECK(server_error.empty());
}

HERMES_TEST(MailTransportCoordinatorPreservesRtfBodiesFromPopMail) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).rfind("USER pop-user", 0) == 0);
            WriteAll(client, "+OK user\r\n");
            HERMES_CHECK(ReadLine(client).rfind("PASS pop-pass", 0) == 0);
            WriteAll(client, "+OK pass\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n1 UIDL-RTF\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n1 128\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "RETR 1");
            WriteAll(client, "+OK retr\r\n");
            WriteAll(client, RtfFixtureMessage());
            WriteAll(client, ".\r\n");
            HERMES_CHECK(ReadLine(client) == "DELE 1");
            WriteAll(client, "+OK deleted\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
        } catch (const std::exception& ex) {
            server_error = ex.what();
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
    hermes::tests::ScopedTempDirectory temp("hemera-pop-rtf");
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
    try {
        const auto summary = coordinator.CheckMail();
        HERMES_CHECK(summary.success);
        HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));

        const auto inbox_messages = message_store.ListMessages("inbox");
        HERMES_CHECK_EQ(inbox_messages.size(), static_cast<std::size_t>(1));
        const auto loaded = message_store.GetMessage("inbox", inbox_messages.front().id);
        HERMES_CHECK(static_cast<bool>(loaded));
        server.join();
        HERMES_CHECK(server_error.empty());
        HERMES_CHECK_EQ(loaded->styled_source, hermes::StyledDocumentSource::kRtf);
        HERMES_CHECK(loaded->rtf_body.find("{\\rtf1\\ansi Rich \\b body\\b0}") != std::string::npos);
        HERMES_CHECK(loaded->plain_text_body.find("Rich ") != std::string::npos);
    } catch (...) {
        if (server.joinable()) {
            server.join();
        }
        throw;
    }
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
    hermes::tests::ScopedTempDirectory temp("hemera-imap");
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

HERMES_TEST(MailTransportCoordinatorReplaysQueuedImapAttachmentFetchWithPayloadDownload) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "* OK imap.example.test ready\r\n");
        HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
        WriteAll(client, "A1 OK LOGIN completed\r\n");
        HERMES_CHECK(ReadLine(client).find("SELECT \"INBOX\"") != std::string::npos);
        WriteAll(client, "* 1 EXISTS\r\n");
        WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
        WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
        HERMES_CHECK(ReadLine(client).find("UID FETCH 42:42") != std::string::npos);
        const std::string payload = MultipartFixtureMessage();
        WriteAll(client, "* 1 FETCH (UID 42 FLAGS (\\Seen) BODY[] {" + std::to_string(payload.size()) + "}\r\n");
        WriteAll(client, payload);
        WriteAll(client, ")\r\n");
        WriteAll(client, "A3 OK UID FETCH completed\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;
    account.imap_omit_attachments = true;
    account.imap_download_mode = hermes::ImapDownloadMode::kMinimalHeaders;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-action-fetch");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 0},
        &error_message));

    hermes::MessageRecord placeholder;
    placeholder.id = "imap-imap-imap-INBOX-42";
    placeholder.mailbox_id = "imap:INBOX";
    placeholder.account_id = "imap";
    placeholder.remote_id = "42";
    placeholder.remote_mailbox = "INBOX";
    placeholder.subject = "Placeholder";
    placeholder.attachments_omitted = true;
    placeholder.download_complete = false;
    placeholder.attachments.push_back({"report.pdf", "application/pdf", 7, true, "", "", "attachment", false, ""});
    HERMES_CHECK(message_store.SaveMessage(placeholder, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    HERMES_CHECK(coordinator.QueueFetchAttachment("imap:INBOX", "imap-imap-imap-INBOX-42", 0, &error_message));
    HERMES_CHECK_EQ(action_store.ListActions().size(), static_cast<std::size_t>(1));

    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK(action_store.ListActions().empty());

    const auto fetched = message_store.GetMessage("imap:INBOX", "imap-imap-imap-INBOX-42");
    HERMES_CHECK(static_cast<bool>(fetched));
    HERMES_CHECK_EQ(fetched->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(!fetched->attachments_omitted);
    HERMES_CHECK(!fetched->attachments.front().omitted);
    HERMES_CHECK(fetched->attachments.front().download_complete);
    HERMES_CHECK(message_store.LoadAttachmentPayload("imap:INBOX", "imap-imap-imap-INBOX-42", 0).has_value());

    server.join();
}

HERMES_TEST(MailTransportCoordinatorReplaysQueuedImapMailboxRenameAfterOptimisticLocalRename) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::string server_error;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "* OK imap.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
            WriteAll(client, "A1 OK LOGIN completed\r\n");
            HERMES_CHECK(ReadLine(client).find("RENAME \"Projects\" \"Archive\"") != std::string::npos);
            WriteAll(client, "A2 OK RENAME completed\r\n");
            ::close(client);
            ::close(listener);
        } catch (const std::exception& ex) {
            server_error = ex.what();
            ::close(listener);
        }
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-rename");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:Projects", "Projects", {}, "imap", hermes::MailboxProtocol::kImap, "Projects", true, false, 0},
        &error_message));

    hermes::MessageRecord cached;
    cached.id = "imap-imap-imap-Projects-7";
    cached.mailbox_id = "imap:Projects";
    cached.account_id = "imap";
    cached.remote_id = "7";
    cached.remote_mailbox = "Projects";
    cached.subject = "Cached project mail";
    HERMES_CHECK(message_store.SaveMessage(cached, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    try {
        HERMES_CHECK(coordinator.QueueRenameMailbox("imap:Projects", "Archive", &error_message));
        HERMES_CHECK(mailbox_store.GetMailbox("imap:Projects") == std::nullopt);
        HERMES_CHECK(mailbox_store.GetMailbox("imap:Archive").has_value());
        HERMES_CHECK(message_store.GetMessage("imap:Archive", "imap-imap-imap-Projects-7").has_value());

        const auto summary = coordinator.CheckMail();
        HERMES_CHECK(summary.success);
        HERMES_CHECK(action_store.ListActions().empty());
        HERMES_CHECK(mailbox_store.GetMailbox("imap:Archive").has_value());
        HERMES_CHECK(mailbox_store.GetMailbox("imap:Projects") == std::nullopt);
        HERMES_CHECK(message_store.GetMessage("imap:Archive", "imap-imap-imap-Projects-7").has_value());
    } catch (...) {
        ::close(listener);
        server.join();
        throw;
    }

    server.join();
    HERMES_CHECK(server_error.empty());
}

HERMES_TEST(MailTransportCoordinatorRefreshMailboxReplaysPendingActionsBeforeSync) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::thread server([&]() {
        {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "* OK imap.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
            WriteAll(client, "A1 OK LOGIN completed\r\n");
            HERMES_CHECK(ReadLine(client).find("RENAME \"Projects\" \"Archive\"") != std::string::npos);
            WriteAll(client, "A2 OK RENAME completed\r\n");
            ::close(client);
        }

        {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "* OK imap.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
            WriteAll(client, "A1 OK LOGIN completed\r\n");
            HERMES_CHECK(ReadLine(client).find("SELECT \"Archive\"") != std::string::npos);
            WriteAll(client, "* 0 EXISTS\r\n");
            WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
            WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
            HERMES_CHECK(ReadLine(client).find("UID FETCH 1:*") != std::string::npos);
            WriteAll(client, "A3 OK UID FETCH completed\r\n");
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
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-refresh-replay");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:Projects", "Projects", {}, "imap", hermes::MailboxProtocol::kImap, "Projects", true, false, 0},
        &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    HERMES_CHECK(coordinator.QueueRenameMailbox("imap:Projects", "Archive", &error_message));
    HERMES_CHECK_EQ(action_store.ListActions().size(), static_cast<std::size_t>(1));
    HERMES_CHECK(mailbox_store.GetMailbox("imap:Archive").has_value());

    const auto summary = coordinator.RefreshMailbox("imap:Archive", false);
    HERMES_CHECK(summary.success);
    HERMES_CHECK(action_store.ListActions().empty());
    const auto sync_state = sync_store.LoadImapState("imap", "imap:Archive");
    HERMES_CHECK(static_cast<bool>(sync_state));
    HERMES_CHECK_EQ(sync_state->uid_validity, static_cast<std::uint64_t>(777));

    server.join();
}

HERMES_TEST(MailTransportCoordinatorQueuesAndExecutesMailboxExpunge) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "* OK imap.example.test ready\r\n");
        HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
        WriteAll(client, "A1 OK LOGIN completed\r\n");
        HERMES_CHECK(ReadLine(client).find("SELECT \"INBOX\"") != std::string::npos);
        WriteAll(client, "* 1 EXISTS\r\n");
        WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
        HERMES_CHECK(ReadLine(client) == "A3 EXPUNGE");
        WriteAll(client, "* 1 EXPUNGE\r\n");
        WriteAll(client, "A3 OK EXPUNGE completed\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;
    account.mark_as_deleted = true;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-expunge");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 0},
        &error_message));

    hermes::MessageRecord deleted_message;
    deleted_message.id = "imap-message-1";
    deleted_message.mailbox_id = "imap:INBOX";
    deleted_message.account_id = "imap";
    deleted_message.subject = "Deleted";
    deleted_message.sender = "sender@example.com";
    deleted_message.remote_id = "1";
    deleted_message.remote_mailbox = "INBOX";
    deleted_message.plain_text_body = "Deleted body";
    deleted_message.deleted = true;
    HERMES_CHECK(message_store.SaveMessage(deleted_message, &error_message));

    hermes::MessageRecord kept_message = deleted_message;
    kept_message.id = "imap-message-2";
    kept_message.remote_id = "2";
    kept_message.subject = "Kept";
    kept_message.deleted = false;
    HERMES_CHECK(message_store.SaveMessage(kept_message, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);

    HERMES_CHECK(coordinator.QueueExpungeMailbox("imap:INBOX", &error_message));
    HERMES_CHECK(message_store.GetMessage("imap:INBOX", "imap-message-1") == std::nullopt);
    HERMES_CHECK(message_store.GetMessage("imap:INBOX", "imap-message-2").has_value());
    HERMES_CHECK_EQ(action_store.ListActions().size(), static_cast<std::size_t>(1));

    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK(action_store.ListActions().empty());
    HERMES_CHECK(message_store.GetMessage("imap:INBOX", "imap-message-2").has_value());

    server.join();
}

HERMES_TEST(MailTransportCoordinatorRetriesAndCancelsPersistedImapActions) {
    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-action-state");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    hermes::ImapActionRecord action;
    action.id = "action-1";
    action.kind = hermes::ImapActionKind::kFetchFullMessage;
    action.state = hermes::ImapActionState::kFailed;
    action.account_id = "imap";
    action.mailbox_id = "imap:INBOX";
    action.remote_mailbox = "INBOX";
    action.message_id = "msg-1";
    action.remote_message_id = "1";
    action.last_error = "network timeout";
    std::string error_message;
    HERMES_CHECK(action_store.SaveAction(action, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    HERMES_CHECK(coordinator.RetryImapAction("action-1", &error_message));
    const auto retried = action_store.GetAction("action-1");
    HERMES_CHECK(static_cast<bool>(retried));
    HERMES_CHECK_EQ(retried->state, hermes::ImapActionState::kPending);
    HERMES_CHECK(retried->last_error.empty());

    HERMES_CHECK(coordinator.CancelImapAction("action-1", &error_message));
    const auto cancelled = action_store.GetAction("action-1");
    HERMES_CHECK(static_cast<bool>(cancelled));
    HERMES_CHECK_EQ(cancelled->state, hermes::ImapActionState::kCancelled);
}

HERMES_TEST(MailTransportCoordinatorQueuesDistinctImapFetchAndRedownloadActions) {
    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-action-kinds");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 1},
        &error_message));

    hermes::MessageRecord message;
    message.id = "msg-1";
    message.mailbox_id = "imap:INBOX";
    message.account_id = "imap";
    message.subject = "Queued IMAP action";
    message.remote_id = "1";
    message.remote_mailbox = "INBOX";
    HERMES_CHECK(message_store.SaveMessage(message, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);

    HERMES_CHECK(coordinator.QueueFetchDefaultMessage("imap:INBOX", "msg-1", &error_message));
    HERMES_CHECK(coordinator.QueueFetchFullMessage("imap:INBOX", "msg-1", &error_message));
    HERMES_CHECK(coordinator.QueueRedownloadMessage("imap:INBOX", "msg-1", false, &error_message));
    HERMES_CHECK(coordinator.QueueRedownloadMessage("imap:INBOX", "msg-1", true, &error_message));

    const auto actions = action_store.ListActions();
    HERMES_CHECK_EQ(actions.size(), static_cast<std::size_t>(4));

    std::vector<hermes::ImapActionKind> kinds;
    kinds.reserve(actions.size());
    for (const auto& action : actions) {
        kinds.push_back(action.kind);
    }

    HERMES_CHECK(std::find(kinds.begin(), kinds.end(), hermes::ImapActionKind::kFetchDefaultMessage) !=
                 kinds.end());
    HERMES_CHECK(std::find(kinds.begin(), kinds.end(), hermes::ImapActionKind::kFetchFullMessage) !=
                 kinds.end());
    HERMES_CHECK(std::find(kinds.begin(), kinds.end(), hermes::ImapActionKind::kRedownloadDefaultMessage) !=
                 kinds.end());
    HERMES_CHECK(std::find(kinds.begin(), kinds.end(), hermes::ImapActionKind::kRedownloadFullMessage) !=
                 kinds.end());
}

HERMES_TEST(MailTransportCoordinatorReplaysDistinctImapFetchSelectorsForDefaultAndFullVariants) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::vector<std::string> fetch_commands;
    std::string server_error;
    std::thread server([&]() {
        try {
            for (int session = 0; session < 4; ++session) {
                const int client = ::accept(listener, nullptr, nullptr);
                HERMES_CHECK(client >= 0);
                WriteAll(client, "* OK imap.example.test ready\r\n");
                HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
                WriteAll(client, "A1 OK LOGIN completed\r\n");
                HERMES_CHECK(ReadLine(client).find("SELECT \"INBOX\"") != std::string::npos);
                WriteAll(client, "* 1 EXISTS\r\n");
                WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
                WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");

                const std::string fetch_command = ReadLine(client);
                fetch_commands.push_back(fetch_command);
                if (fetch_command.find("BODY.PEEK[TEXT]") != std::string::npos) {
                    const std::string payload = "\r\nFetched body text\r\n";
                    WriteAll(client,
                             "* 1 FETCH (UID 42 FLAGS (\\Seen) BODY[TEXT] {" +
                                 std::to_string(payload.size()) + "}\r\n");
                    WriteAll(client, payload);
                    WriteAll(client, ")\r\n");
                } else {
                    HERMES_CHECK(fetch_command.find("BODY.PEEK[]") != std::string::npos);
                    const std::string payload = MultipartFixtureMessage();
                    WriteAll(client,
                             "* 1 FETCH (UID 42 FLAGS (\\Seen) BODY[] {" +
                                 std::to_string(payload.size()) + "}\r\n");
                    WriteAll(client, payload);
                    WriteAll(client, ")\r\n");
                }
                WriteAll(client, "A3 OK UID FETCH completed\r\n");
                ::close(client);
            }
            ::close(listener);
        } catch (const std::exception& ex) {
            server_error = ex.what();
            ::close(listener);
        }
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;
    account.imap_omit_attachments = true;
    account.imap_download_mode = hermes::ImapDownloadMode::kMessageBody;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-fetch-selectors");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 0},
        &error_message));

    hermes::MessageRecord placeholder;
    placeholder.id = "imap-imap-imap-INBOX-42";
    placeholder.mailbox_id = "imap:INBOX";
    placeholder.account_id = "imap";
    placeholder.remote_id = "42";
    placeholder.remote_mailbox = "INBOX";
    placeholder.subject = "Placeholder";
    placeholder.sender = "sender@example.com";
    placeholder.recipients = "receiver@example.com";
    placeholder.attachments_omitted = true;
    placeholder.download_complete = false;
    placeholder.attachments.push_back(
        {"report.pdf", "application/pdf", 7, true, "", "", "attachment", false, ""});
    HERMES_CHECK(message_store.SaveMessage(placeholder, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    HERMES_CHECK(
        coordinator.QueueFetchDefaultMessage("imap:INBOX", "imap-imap-imap-INBOX-42", &error_message));
    HERMES_CHECK(
        coordinator.QueueFetchFullMessage("imap:INBOX", "imap-imap-imap-INBOX-42", &error_message));
    HERMES_CHECK(coordinator.QueueRedownloadMessage("imap:INBOX",
                                                    "imap-imap-imap-INBOX-42",
                                                    false,
                                                    &error_message));
    HERMES_CHECK(coordinator.QueueRedownloadMessage("imap:INBOX",
                                                    "imap-imap-imap-INBOX-42",
                                                    true,
                                                    &error_message));

    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK(action_store.ListActions().empty());

    server.join();
    HERMES_CHECK(server_error.empty());
    HERMES_CHECK_EQ(fetch_commands.size(), static_cast<std::size_t>(4));
    HERMES_CHECK(fetch_commands[0].find("BODY.PEEK[TEXT]") != std::string::npos);
    HERMES_CHECK(fetch_commands[1].find("BODY.PEEK[]") != std::string::npos);
    HERMES_CHECK(fetch_commands[2].find("BODY.PEEK[TEXT]") != std::string::npos);
    HERMES_CHECK(fetch_commands[3].find("BODY.PEEK[]") != std::string::npos);

    const auto fetched = message_store.GetMessage("imap:INBOX", "imap-imap-imap-INBOX-42");
    HERMES_CHECK(static_cast<bool>(fetched));
    HERMES_CHECK_EQ(fetched->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(!fetched->attachments_omitted);
    HERMES_CHECK(fetched->download_complete);
}

HERMES_TEST(MailTransportCoordinatorDefaultFetchHonorsMinimalHeaderDownloadMode) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::vector<std::string> fetch_commands;
    std::string server_error;
    std::thread server([&]() {
        try {
            for (int session = 0; session < 3; ++session) {
                const int client = ::accept(listener, nullptr, nullptr);
                HERMES_CHECK(client >= 0);
                WriteAll(client, "* OK imap.example.test ready\r\n");
                HERMES_CHECK(ReadLine(client).find("LOGIN") != std::string::npos);
                WriteAll(client, "A1 OK LOGIN completed\r\n");
                HERMES_CHECK(ReadLine(client).find("SELECT \"INBOX\"") != std::string::npos);
                WriteAll(client, "* 1 EXISTS\r\n");
                WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
                WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");

                const std::string fetch_command = ReadLine(client);
                fetch_commands.push_back(fetch_command);
                if (fetch_command.find("BODY.PEEK[HEADER]") != std::string::npos) {
                    const std::string payload =
                        "Subject: Header only\r\nFrom: sender@example.com\r\nTo: receiver@example.com\r\n\r\n";
                    WriteAll(client,
                             "* 1 FETCH (UID 42 FLAGS (\\Seen) BODY[HEADER] {" +
                                 std::to_string(payload.size()) + "}\r\n");
                    WriteAll(client, payload);
                    WriteAll(client, ")\r\n");
                } else {
                    HERMES_CHECK(fetch_command.find("BODY.PEEK[]") != std::string::npos);
                    const std::string payload = MultipartFixtureMessage();
                    WriteAll(client,
                             "* 1 FETCH (UID 42 FLAGS (\\Seen) BODY[] {" +
                                 std::to_string(payload.size()) + "}\r\n");
                    WriteAll(client, payload);
                    WriteAll(client, ")\r\n");
                }
                WriteAll(client, "A3 OK UID FETCH completed\r\n");
                ::close(client);
            }
            ::close(listener);
        } catch (const std::exception& ex) {
            server_error = ex.what();
            ::close(listener);
        }
    });

    hermes::AccountProfile account;
    account.id = "imap";
    account.display_name = "IMAP";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = false;
    account.imap_auth = hermes::ImapAuthMode::kPassword;
    account.imap_omit_attachments = true;
    account.imap_download_mode = hermes::ImapDownloadMode::kMinimalHeaders;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-header-default");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemImapActionStore action_store(temp.Path() / "actions");
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap", hermes::CredentialKind::kIncoming, "imap-pass",
                                            &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 0},
        &error_message));

    hermes::MessageRecord placeholder;
    placeholder.id = "imap-imap-imap-INBOX-42";
    placeholder.mailbox_id = "imap:INBOX";
    placeholder.account_id = "imap";
    placeholder.remote_id = "42";
    placeholder.remote_mailbox = "INBOX";
    placeholder.subject = "Placeholder";
    placeholder.attachments_omitted = true;
    placeholder.download_complete = false;
    placeholder.attachments.push_back(
        {"report.pdf", "application/pdf", 7, true, "", "", "attachment", false, ""});
    HERMES_CHECK(message_store.SaveMessage(placeholder, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 &action_store);
    HERMES_CHECK(
        coordinator.QueueFetchDefaultMessage("imap:INBOX", "imap-imap-imap-INBOX-42", &error_message));
    HERMES_CHECK(
        coordinator.QueueFetchFullMessage("imap:INBOX", "imap-imap-imap-INBOX-42", &error_message));
    HERMES_CHECK(coordinator.QueueRedownloadMessage("imap:INBOX",
                                                    "imap-imap-imap-INBOX-42",
                                                    false,
                                                    &error_message));

    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);

    server.join();
    HERMES_CHECK(server_error.empty());
    HERMES_CHECK_EQ(fetch_commands.size(), static_cast<std::size_t>(3));
    HERMES_CHECK(fetch_commands[0].find("BODY.PEEK[HEADER]") != std::string::npos);
    HERMES_CHECK(fetch_commands[1].find("BODY.PEEK[]") != std::string::npos);
    HERMES_CHECK(fetch_commands[2].find("BODY.PEEK[HEADER]") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorUsesSharedKerberosHelperForPopAndHonorsKerberosPortOverride) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client) == "AUTH GSSAPI");
            WriteAll(client, "+ " + Base64Encode("server-1") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-1"));
            WriteAll(client, "+ " + Base64Encode("server-2") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-2"));
            WriteAll(client, "+ " + Base64Encode(std::string("\1\0\0\0", 4)) + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("wrapped-final"));
            WriteAll(client, "+OK GSSAPI complete\r\n");
            HERMES_CHECK(ReadLine(client) == "UIDL");
            WriteAll(client, "+OK uidl\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "LIST");
            WriteAll(client, "+OK list\r\n.\r\n");
            HERMES_CHECK(ReadLine(client) == "QUIT");
            WriteAll(client, "+OK bye\r\n");
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "pop-krb";
    account.display_name = "POP Kerberos";
    account.login_name = "pop-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = static_cast<std::uint16_t>(port + 1);
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.pop_auth = hermes::PopAuthMode::kKerberos;
    account.kerberos.service_name = "pop";
    account.kerberos.realm = "EXAMPLE.TEST";
    account.kerberos.service_format = "%s/%h@%r";
    account.kerberos.pop_port = port;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-kerberos");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;
    FakeGssapiEngine fake_engine;
    fake_engine.config.expected_inputs = {"server-1", "server-2"};
    fake_engine.config.steps = {{true, "client-1"}, {false, "client-2"}};
    fake_engine.config.wrap_output = "wrapped-final";

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop-krb", hermes::CredentialKind::kIncoming, "krb-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 &fake_engine);
    const auto summary = coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(0));
    HERMES_CHECK(tasks.Errors().empty());
    HERMES_CHECK_EQ(fake_engine.principals.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(fake_engine.principals.front(), std::string("pop/127.0.0.1@EXAMPLE.TEST"));
}

HERMES_TEST(MailTransportCoordinatorClassifiesPopKerberosServerRejection) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client) == "AUTH GSSAPI");
            WriteAll(client, "+ " + Base64Encode("server-1") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-1"));
            WriteAll(client, "+ " + Base64Encode("server-2") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-2"));
            WriteAll(client, "+ " + Base64Encode(std::string("\1\0\0\0", 4)) + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("wrapped-final"));
            WriteAll(client, "-ERR rejected\r\n");
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "pop-krb";
    account.display_name = "POP Kerberos";
    account.login_name = "pop-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.pop_auth = hermes::PopAuthMode::kKerberos;
    account.kerberos.pop_port = port;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-kerberos-reject");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;
    FakeGssapiEngine fake_engine;
    fake_engine.config.expected_inputs = {"server-1", "server-2"};
    fake_engine.config.steps = {{true, "client-1"}, {false, "client-2"}};

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop-krb", hermes::CredentialKind::kIncoming, "krb-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 &fake_engine);
    const auto summary = coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    HERMES_CHECK(summary.warnings.size() == 1);
    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().kind, hermes::MailTaskErrorKind::kServerRejected);
    HERMES_CHECK_EQ(errors.front().mechanism, std::string("GSSAPI"));
    HERMES_CHECK(errors.front().message.find("POP GSSAPI authentication failed") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorClassifiesKerberosUnavailableWithoutFallback) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client) == "AUTH GSSAPI");
            HERMES_CHECK(ReadLine(client).empty());
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "pop-krb";
    account.display_name = "POP Kerberos";
    account.login_name = "pop-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.pop_auth = hermes::PopAuthMode::kKerberos;
    account.kerberos.pop_port = port;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-kerberos-unavailable");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;
    FakeGssapiEngine fake_engine;
    fake_engine.available = false;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop-krb", hermes::CredentialKind::kIncoming, "krb-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 &fake_engine);
    coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().kind, hermes::MailTaskErrorKind::kKerberosUnavailable);
    HERMES_CHECK_EQ(errors.front().mechanism, std::string("GSSAPI"));
    HERMES_CHECK(errors.front().message.find("Kerberos support is unavailable") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorDoesNotFallbackWhenRpaIsSelected) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "+OK pop.example.test ready\r\n");
            const std::string first_line = ReadLine(client);
            HERMES_CHECK(first_line.empty());
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "pop-rpa";
    account.display_name = "POP RPA";
    account.login_name = "pop-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.pop_auth = hermes::PopAuthMode::kRPA;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-pop-rpa");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("pop-rpa", hermes::CredentialKind::kIncoming, "rpa-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(
        accounts, credentials, sync_store, mailbox_store, message_store, transport, tls_provider, tasks);
    coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().kind, hermes::MailTaskErrorKind::kUnsupportedMechanism);
    HERMES_CHECK_EQ(errors.front().mechanism, std::string("RPA"));
    HERMES_CHECK(errors.front().message.find("not yet implemented") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorUsesSharedKerberosHelperForImap) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            for (int session = 0; session < 2; ++session) {
                const int client = ::accept(listener, nullptr, nullptr);
                HERMES_CHECK(client >= 0);
                WriteAll(client, "* OK imap.example.test ready\r\n");
                HERMES_CHECK(ReadLine(client) == "A1 AUTHENTICATE GSSAPI");
                WriteAll(client, "+ " + Base64Encode("server-1") + "\r\n");
                HERMES_CHECK(ReadLine(client) == Base64Encode("client-1"));
                WriteAll(client, "+ " + Base64Encode("server-2") + "\r\n");
                HERMES_CHECK(ReadLine(client) == Base64Encode("client-2"));
                WriteAll(client, "+ " + Base64Encode(std::string("\1\0\0\0", 4)) + "\r\n");
                HERMES_CHECK(ReadLine(client) == Base64Encode("wrapped-final"));
                WriteAll(client, "A1 OK AUTHENTICATE completed\r\n");
                if (session == 0) {
                    HERMES_CHECK(ReadLine(client) == "A2 LIST \"\" \"*\"");
                    WriteAll(client, "* LIST () \"/\" \"INBOX\"\r\n");
                    WriteAll(client, "A2 OK LIST completed\r\n");
                } else {
                    HERMES_CHECK(ReadLine(client) == "A2 SELECT \"INBOX\"");
                    WriteAll(client, "* 0 EXISTS\r\n");
                    WriteAll(client, "* OK [UIDVALIDITY 777] UIDs valid\r\n");
                    WriteAll(client, "A2 OK [READ-WRITE] SELECT completed\r\n");
                    HERMES_CHECK(ReadLine(client) == "A3 UID FETCH 1:* (UID FLAGS BODY.PEEK[])");
                    WriteAll(client, "A3 OK UID FETCH completed\r\n");
                }
                ::close(client);
            }
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "imap-krb";
    account.display_name = "IMAP Kerberos";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = true;
    account.imap_auth = hermes::ImapAuthMode::kKerberos;
    account.kerberos.service_name = "imap";
    account.kerberos.realm = "EXAMPLE.TEST";
    account.kerberos.service_format = "%s/%h@%r";

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-kerberos");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;
    FakeGssapiEngine fake_engine;
    fake_engine.config.expected_inputs = {"server-1", "server-2"};
    fake_engine.config.steps = {{true, "client-1"}, {false, "client-2"}};

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap-krb", hermes::CredentialKind::kIncoming, "krb-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 &fake_engine);
    const auto summary = coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.mailboxes_discovered, static_cast<std::size_t>(1));
    HERMES_CHECK(tasks.Errors().empty());
    HERMES_CHECK_EQ(fake_engine.principals.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(fake_engine.principals.front(), std::string("imap/127.0.0.1@EXAMPLE.TEST"));
}

HERMES_TEST(MailTransportCoordinatorClassifiesImapKerberosHandshakeFailures) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            WriteAll(client, "* OK imap.example.test ready\r\n");
            HERMES_CHECK(ReadLine(client) == "A1 AUTHENTICATE GSSAPI");
            WriteAll(client, "+ " + Base64Encode("server-1") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-1"));
            WriteAll(client, "+ " + Base64Encode("server-2") + "\r\n");
            HERMES_CHECK(ReadLine(client) == Base64Encode("client-2"));
            WriteAll(client, "+ " + Base64Encode("server-final") + "\r\n");
            HERMES_CHECK(ReadLine(client).empty());
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "imap-krb";
    account.display_name = "IMAP Kerberos";
    account.login_name = "imap-user";
    account.incoming_server = "127.0.0.1";
    account.incoming_port = port;
    account.uses_imap = true;
    account.check_mail_by_default = true;
    account.imap_auth = hermes::ImapAuthMode::kKerberos;

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::tests::ScopedTempDirectory temp("hemera-imap-kerberos-fail");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::InMemoryMailTaskModel tasks;
    FakeGssapiEngine fake_engine;
    fake_engine.config.expected_inputs = {"server-1", "server-2"};
    fake_engine.config.steps = {{true, "client-1"}, {false, "client-2"}};
    fake_engine.config.fail_unwrap = true;
    fake_engine.config.unwrap_failure_kind = hermes::MailTaskErrorKind::kHandshakeFailed;
    fake_engine.config.unwrap_failure_message = "Synthetic IMAP GSSAPI unwrap failure.";

    std::string error_message;
    HERMES_CHECK(credentials.SaveCredential("imap-krb", hermes::CredentialKind::kIncoming, "krb-pass",
                                            &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 &fake_engine);
    coordinator.CheckMail();
    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }

    const auto errors = tasks.Errors();
    HERMES_CHECK_EQ(errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(errors.front().kind, hermes::MailTaskErrorKind::kHandshakeFailed);
    HERMES_CHECK_EQ(errors.front().mechanism, std::string("GSSAPI"));
    HERMES_CHECK(errors.front().message.find("Synthetic IMAP GSSAPI unwrap failure.") != std::string::npos);
}

HERMES_TEST(MailTransportCoordinatorSendsQueuedMailUsingSmtpOAuth2) {
    std::uint16_t smtp_port = 0;
    const int listener = CreateListener(&smtp_port);
    HERMES_CHECK(listener >= 0);

    std::string captured_auth_line;
    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "220 smtp.example.test ESMTP\r\n");
        HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
        WriteAll(client, "250-localhost\r\n250-AUTH XOAUTH2\r\n250 OK\r\n");
        captured_auth_line = ReadLine(client);
        WriteAll(client, "235 Authentication successful\r\n");
        HERMES_CHECK(ReadLine(client).rfind("MAIL FROM:<sender@example.com>", 0) == 0);
        WriteAll(client, "250 Sender ok\r\n");
        HERMES_CHECK(ReadLine(client).rfind("RCPT TO:<receiver@example.com>", 0) == 0);
        WriteAll(client, "250 Recipient ok\r\n");
        HERMES_CHECK(ReadLine(client) == "DATA");
        WriteAll(client, "354 End with .\r\n");
        (void)ReadUntilDot(client);
        WriteAll(client, "250 Queued\r\n");
        HERMES_CHECK(ReadLine(client) == "QUIT");
        WriteAll(client, "221 Bye\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "oauth-smtp";
    account.display_name = "OAuth SMTP";
    account.login_name = "sender@example.com";
    account.email_address = "sender@example.com";
    account.outgoing_server = "127.0.0.1";
    account.outgoing_port = smtp_port;
    account.smtp_auth = hermes::SmtpAuthMode::kOAuth2;
    account.smtp_auth_allowed = true;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.token_endpoint = "http://unused/token";
    account.oauth.device_authorization_endpoint = "http://unused/device";
    account.oauth.scopes = {"scope.mail"};

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::OAuthTokenRecord token_record;
    token_record.account_id = "oauth-smtp";
    token_record.access_token = "access-1";
    token_record.refresh_token = "refresh-1";
    token_record.expires_at_unix = 4102444800;
    token_record.issued_at_unix = 4102441200;
    std::string error_message;
    HERMES_CHECK(token_store.SaveToken(token_record, &error_message));
    hermes::tests::ScopedTempDirectory temp("hemera-smtp-oauth");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService oauth_service(http_client, token_store, credentials);
    hermes::InMemoryMailTaskModel tasks;

    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "oauth-smtp", hermes::MailboxProtocol::kSmtp, "", false, true, 0}, &error_message));
    hermes::MessageRecord queued;
    queued.id = "queued-oauth";
    queued.mailbox_id = "out";
    queued.account_id = "oauth-smtp";
    queued.subject = "OAuth SMTP";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "OAuth body";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 &oauth_service,
                                                 &token_store);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(captured_auth_line,
                    std::string("AUTH XOAUTH2 ") +
                        Base64Encode(std::string("user=sender@example.com\1auth=Bearer access-1\1\1")));

    server.join();
}

HERMES_TEST(MailTransportCoordinatorFetchesPopMailUsingOAuth2) {
    std::uint16_t pop_port = 0;
    const int listener = CreateListener(&pop_port);
    HERMES_CHECK(listener >= 0);

    std::string payload_line;
    std::thread server([&]() {
        const int client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(client >= 0);
        WriteAll(client, "+OK pop.example.test ready\r\n");
        HERMES_CHECK(ReadLine(client) == "AUTH XOAUTH2");
        WriteAll(client, "+ \r\n");
        payload_line = ReadLine(client);
        WriteAll(client, "+OK authenticated\r\n");
        HERMES_CHECK(ReadLine(client) == "UIDL");
        WriteAll(client, "+OK uidl follows\r\n1 UIDL-1\r\n.\r\n");
        HERMES_CHECK(ReadLine(client) == "LIST");
        WriteAll(client, "+OK list follows\r\n1 42\r\n.\r\n");
        HERMES_CHECK(ReadLine(client) == "RETR 1");
        WriteAll(client,
                 "+OK message follows\r\n"
                 "Subject: POP OAuth\r\n"
                 "From: sender@example.com\r\n"
                 "To: receiver@example.com\r\n"
                 "\r\n"
                 "Body\r\n.\r\n");
        HERMES_CHECK(ReadLine(client) == "QUIT");
        WriteAll(client, "+OK bye\r\n");
        ::close(client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "oauth-pop";
    account.display_name = "OAuth POP";
    account.login_name = "pop-user";
    account.email_address = "user@example.com";
    account.uses_pop = true;
    account.check_mail_by_default = true;
    account.leave_mail_on_server = true;
    account.incoming_server = "127.0.0.1";
    account.incoming_port = pop_port;
    account.pop_auth = hermes::PopAuthMode::kOAuth2;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.device_authorization_endpoint = "http://unused/device";
    account.oauth.token_endpoint = "http://unused/token";
    account.oauth.scopes = {"scope.mail"};

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::OAuthTokenRecord token_record;
    token_record.account_id = "oauth-pop";
    token_record.access_token = "access-2";
    token_record.refresh_token = "refresh-2";
    token_record.expires_at_unix = 4102444800;
    token_record.issued_at_unix = 4102441200;
    std::string error_message;
    HERMES_CHECK(token_store.SaveToken(token_record, &error_message));
    hermes::tests::ScopedTempDirectory temp("hemera-pop-oauth");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService oauth_service(http_client, token_store, credentials);
    hermes::InMemoryMailTaskModel tasks;

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 &oauth_service,
                                                 &token_store);
    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(payload_line,
                    Base64Encode(std::string("user=user@example.com\1auth=Bearer access-2\1\1")));

    server.join();
}

HERMES_TEST(MailTransportCoordinatorFetchesImapMailUsingOAuth2) {
    std::uint16_t imap_port = 0;
    const int listener = CreateListener(&imap_port);
    HERMES_CHECK(listener >= 0);

    std::string payload_line;
    std::thread server([&]() {
        const int discovery_client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(discovery_client >= 0);
        WriteAll(discovery_client, "* OK imap.example.test ready\r\n");
        HERMES_CHECK(ReadLine(discovery_client) == "A1 AUTHENTICATE XOAUTH2");
        WriteAll(discovery_client, "+ \r\n");
        payload_line = ReadLine(discovery_client);
        WriteAll(discovery_client, "A1 OK AUTHENTICATE completed\r\n");
        HERMES_CHECK(ReadLine(discovery_client) == "A2 LIST \"\" \"*\"");
        WriteAll(discovery_client, "* LIST () \"/\" \"INBOX\"\r\nA2 OK LIST completed\r\n");
        ::close(discovery_client);

        const int sync_client = ::accept(listener, nullptr, nullptr);
        HERMES_CHECK(sync_client >= 0);
        WriteAll(sync_client, "* OK imap.example.test ready\r\n");
        HERMES_CHECK(ReadLine(sync_client) == "A1 AUTHENTICATE XOAUTH2");
        WriteAll(sync_client, "+ \r\n");
        HERMES_CHECK_EQ(ReadLine(sync_client), payload_line);
        WriteAll(sync_client, "A1 OK AUTHENTICATE completed\r\n");
        HERMES_CHECK(ReadLine(sync_client).find("SELECT \"INBOX\"") != std::string::npos);
        WriteAll(sync_client, "* OK [UIDVALIDITY 1] stable\r\nA2 OK SELECT completed\r\n");
        HERMES_CHECK(ReadLine(sync_client).find("UID FETCH 1:* (UID FLAGS BODY.PEEK[])") != std::string::npos);
        const std::string raw_message =
            "Subject: IMAP OAuth\r\nFrom: sender@example.com\r\nTo: receiver@example.com\r\n\r\nBody";
        WriteAll(sync_client,
                 "* 1 FETCH (UID 1 FLAGS (\\Seen) BODY[] {" + std::to_string(raw_message.size()) + "}\r\n");
        WriteAll(sync_client, raw_message);
        WriteAll(sync_client, "\r\n)\r\nA3 OK FETCH completed\r\n");
        ::close(sync_client);
        ::close(listener);
    });

    hermes::AccountProfile account;
    account.id = "oauth-imap";
    account.display_name = "OAuth IMAP";
    account.login_name = "imap-user";
    account.email_address = "user@example.com";
    account.uses_imap = true;
    account.check_mail_by_default = true;
    account.incoming_server = "127.0.0.1";
    account.incoming_port = imap_port;
    account.imap_auth = hermes::ImapAuthMode::kOAuth2;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.device_authorization_endpoint = "http://unused/device";
    account.oauth.token_endpoint = "http://unused/token";
    account.oauth.scopes = {"scope.mail"};

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::OAuthTokenRecord token_record;
    token_record.account_id = "oauth-imap";
    token_record.access_token = "access-3";
    token_record.refresh_token = "refresh-3";
    token_record.expires_at_unix = 4102444800;
    token_record.issued_at_unix = 4102441200;
    std::string error_message;
    HERMES_CHECK(token_store.SaveToken(token_record, &error_message));
    hermes::tests::ScopedTempDirectory temp("hemera-imap-oauth");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService oauth_service(http_client, token_store, credentials);
    hermes::InMemoryMailTaskModel tasks;

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 &oauth_service,
                                                 &token_store);
    const auto summary = coordinator.CheckMail();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_received, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(payload_line,
                    Base64Encode(std::string("user=user@example.com\1auth=Bearer access-3\1\1")));

    server.join();
}

HERMES_TEST(MailTransportCoordinatorRefreshesOAuthTokenAndRetriesOnce) {
    std::uint16_t smtp_port = 0;
    const int smtp_listener = CreateListener(&smtp_port);
    HERMES_CHECK(smtp_listener >= 0);
    std::uint16_t oauth_port = 0;
    const int oauth_listener = CreateListener(&oauth_port);
    HERMES_CHECK(oauth_listener >= 0);

    std::atomic<int> smtp_connections{0};
    std::exception_ptr smtp_failure;
    std::thread smtp_server([&]() {
        try {
            for (int attempt = 0; attempt < 2; ++attempt) {
                const int client = ::accept(smtp_listener, nullptr, nullptr);
                HERMES_CHECK(client >= 0);
                ++smtp_connections;
                WriteAll(client, "220 smtp.example.test ESMTP\r\n");
                HERMES_CHECK(ReadLine(client).rfind("EHLO hemera", 0) == 0);
                WriteAll(client, "250-localhost\r\n250-AUTH XOAUTH2\r\n250 OK\r\n");
                const std::string auth_line = ReadLine(client);
                if (attempt == 0) {
                    WriteAll(client, "535 5.7.3 Authentication unsuccessful\r\n");
                    ::close(client);
                    continue;
                }
                HERMES_CHECK_EQ(auth_line,
                                std::string("AUTH XOAUTH2 ") +
                                    Base64Encode(std::string("user=sender@example.com\1auth=Bearer fresh-access\1\1")));
                WriteAll(client, "235 Authentication successful\r\n");
                HERMES_CHECK(ReadLine(client).rfind("MAIL FROM:<sender@example.com>", 0) == 0);
                WriteAll(client, "250 Sender ok\r\n");
                HERMES_CHECK(ReadLine(client).rfind("RCPT TO:<receiver@example.com>", 0) == 0);
                WriteAll(client, "250 Recipient ok\r\n");
                HERMES_CHECK(ReadLine(client) == "DATA");
                WriteAll(client, "354 End with .\r\n");
                (void)ReadUntilDot(client);
                WriteAll(client, "250 Queued\r\n");
                HERMES_CHECK(ReadLine(client) == "QUIT");
                WriteAll(client, "221 Bye\r\n");
                ::close(client);
            }
            ::close(smtp_listener);
        } catch (...) {
            smtp_failure = std::current_exception();
        }
    });

    std::exception_ptr oauth_failure;
    std::thread oauth_server([&]() {
        try {
            const int client = ::accept(oauth_listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            const HttpRequest request = ReadHttpRequest(client);
            HERMES_CHECK_EQ(request.path, std::string("/token"));
            HERMES_CHECK(request.body.find("grant_type=refresh_token") != std::string::npos);
            HERMES_CHECK(request.body.find("refresh_token=refresh-4") != std::string::npos);
            const std::string response =
                "{\"access_token\":\"fresh-access\",\"refresh_token\":\"refresh-4\",\"expires_in\":3600,"
                "\"scope\":\"scope.mail\",\"token_type\":\"Bearer\"}";
            WriteAll(client,
                     "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                     "Content-Length: " + std::to_string(response.size()) + "\r\n\r\n" + response);
            ::close(client);
            ::close(oauth_listener);
        } catch (...) {
            oauth_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "oauth-retry";
    account.display_name = "OAuth Retry";
    account.login_name = "sender@example.com";
    account.email_address = "sender@example.com";
    account.outgoing_server = "127.0.0.1";
    account.outgoing_port = smtp_port;
    account.smtp_auth = hermes::SmtpAuthMode::kOAuth2;
    account.smtp_auth_allowed = true;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.device_authorization_endpoint = "http://127.0.0.1:" + std::to_string(oauth_port) + "/device";
    account.oauth.token_endpoint = "http://127.0.0.1:" + std::to_string(oauth_port) + "/token";
    account.oauth.scopes = {"scope.mail"};

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::OAuthTokenRecord token_record;
    token_record.account_id = "oauth-retry";
    token_record.access_token = "stale-access";
    token_record.refresh_token = "refresh-4";
    token_record.expires_at_unix = 4102444800;
    token_record.issued_at_unix = 4102441200;
    std::string error_message;
    HERMES_CHECK(token_store.SaveToken(token_record, &error_message));
    hermes::tests::ScopedTempDirectory temp("hemera-oauth-retry");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService oauth_service(http_client, token_store, credentials);
    hermes::InMemoryMailTaskModel tasks;

    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "oauth-retry", hermes::MailboxProtocol::kSmtp, "", false, true, 0}, &error_message));
    hermes::MessageRecord queued;
    queued.id = "queued-retry";
    queued.mailbox_id = "out";
    queued.account_id = "oauth-retry";
    queued.subject = "Retry";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "Retry body";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 &oauth_service,
                                                 &token_store);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success);
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(1));
    const auto saved = token_store.LoadToken("oauth-retry");
    HERMES_CHECK(static_cast<bool>(saved));
    HERMES_CHECK_EQ(saved->access_token, std::string("fresh-access"));

    smtp_server.join();
    oauth_server.join();
    if (smtp_failure) {
        std::rethrow_exception(smtp_failure);
    }
    if (oauth_failure) {
        std::rethrow_exception(oauth_failure);
    }
}

HERMES_TEST(MailTransportCoordinatorDoesNotFallbackWhenOAuthAuthorizationIsMissing) {
    hermes::AccountProfile account;
    account.id = "oauth-missing";
    account.display_name = "OAuth Missing";
    account.login_name = "sender@example.com";
    account.email_address = "sender@example.com";
    account.outgoing_server = "127.0.0.1";
    account.outgoing_port = 2525;
    account.smtp_auth = hermes::SmtpAuthMode::kOAuth2;
    account.smtp_auth_allowed = true;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.device_authorization_endpoint = "http://unused/device";
    account.oauth.token_endpoint = "http://unused/token";
    account.oauth.scopes = {"scope.mail"};

    FixedAccountService accounts({account});
    hermes::InMemoryCredentialStore credentials;
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::tests::ScopedTempDirectory temp("hemera-oauth-missing");
    hermes::FilesystemSyncStateStore sync_store(temp.Path() / "sync");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService oauth_service(http_client, token_store, credentials);
    hermes::InMemoryMailTaskModel tasks;

    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(
        {"out", "Out", {}, "oauth-missing", hermes::MailboxProtocol::kSmtp, "", false, true, 0}, &error_message));
    hermes::MessageRecord queued;
    queued.id = "queued-missing";
    queued.mailbox_id = "out";
    queued.account_id = "oauth-missing";
    queued.subject = "Missing OAuth";
    queued.sender = "sender@example.com";
    queued.recipients = "receiver@example.com";
    queued.plain_text_body = "No token";
    queued.delivery_state = hermes::MessageDeliveryState::kQueued;
    HERMES_CHECK(message_store.SaveMessage(queued, &error_message));

    hermes::MailTransportCoordinator coordinator(accounts,
                                                 credentials,
                                                 sync_store,
                                                 mailbox_store,
                                                 message_store,
                                                 transport,
                                                 tls_provider,
                                                 tasks,
                                                 &oauth_service,
                                                 &token_store);
    const auto summary = coordinator.SendQueued();
    HERMES_CHECK(summary.success == false || !summary.warnings.empty());
    HERMES_CHECK_EQ(summary.messages_sent, static_cast<std::size_t>(0));
    const auto failed = message_store.GetMessage("out", "queued-missing");
    HERMES_CHECK(static_cast<bool>(failed));
}

#endif
