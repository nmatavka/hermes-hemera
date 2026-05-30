#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/CredentialStore.h"
#include "hermes/OAuthSupport.h"
#include "hermes/OpenSslTlsProvider.h"
#include "hermes/TransportService.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <map>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

namespace {

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

#endif

}  // namespace

HERMES_TEST(OAuthTokenStoresRoundTripState) {
    hermes::OAuthTokenRecord record;
    record.account_id = "oauth";
    record.refresh_token = "refresh";
    record.access_token = "access";
    record.issued_at_unix = 100;
    record.expires_at_unix = 4600;
    record.granted_scopes = {"scope-a", "scope-b"};
    record.token_type = "Bearer";
    record.last_auth_account_hint = "user@example.com";

    hermes::InMemoryOAuthTokenStore memory_store;
    std::string error_message;
    HERMES_CHECK(memory_store.SaveToken(record, &error_message));
    const auto loaded_memory = memory_store.LoadToken("oauth");
    HERMES_CHECK(static_cast<bool>(loaded_memory));
    HERMES_CHECK_EQ(loaded_memory->refresh_token, std::string("refresh"));
    HERMES_CHECK_EQ(loaded_memory->granted_scopes.size(), static_cast<std::size_t>(2));

    hermes::tests::ScopedTempDirectory temp("hemera-oauth-token-store");
    hermes::FilesystemOAuthTokenStore file_store(temp.Path());
    HERMES_CHECK(file_store.SaveToken(record, &error_message));
    const auto loaded_file = file_store.LoadToken("oauth");
    HERMES_CHECK(static_cast<bool>(loaded_file));
    HERMES_CHECK_EQ(loaded_file->access_token, std::string("access"));
    HERMES_CHECK_EQ(loaded_file->last_auth_account_hint, std::string("user@example.com"));
    HERMES_CHECK(file_store.DeleteToken("oauth", &error_message));
    HERMES_CHECK(!file_store.LoadToken("oauth"));
}

HERMES_TEST(OAuthDeviceFlowServiceResolvesMicrosoftPresetScopes) {
    hermes::InMemoryOAuthTokenStore token_store;
    hermes::InMemoryCredentialStore credential_store;
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService service(http_client, token_store, credential_store);

    hermes::AccountProfile account;
    account.id = "oauth";
    account.email_address = "user@example.com";
    account.login_name = "user@example.com";
    account.uses_imap = true;
    account.pop_auth = hermes::PopAuthMode::kPassword;
    account.imap_auth = hermes::ImapAuthMode::kOAuth2;
    account.smtp_auth = hermes::SmtpAuthMode::kOAuth2;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kMicrosoft365;
    account.oauth.client_id = "client-id";
    account.oauth.tenant_or_domain = "contoso.com";

    hermes::MailTaskErrorKind error_kind = hermes::MailTaskErrorKind::kUnknown;
    std::string error_message;
    const auto resolved = service.ResolveSettings(account, &error_kind, &error_message);
    HERMES_CHECK(static_cast<bool>(resolved));
    HERMES_CHECK_EQ(resolved->device_authorization_endpoint,
                    std::string("https://login.microsoftonline.com/contoso.com/oauth2/v2.0/devicecode"));
    HERMES_CHECK_EQ(resolved->token_endpoint,
                    std::string("https://login.microsoftonline.com/contoso.com/oauth2/v2.0/token"));
    HERMES_CHECK_EQ(resolved->scopes.size(), static_cast<std::size_t>(3));
}

#if !defined(_WIN32)

HERMES_TEST(OAuthDeviceFlowServiceBeginsPollsAndPersistsTokens) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client1 = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client1 >= 0);
            const HttpRequest request1 = ReadHttpRequest(client1);
            HERMES_CHECK_EQ(request1.method, std::string("POST"));
            HERMES_CHECK_EQ(request1.path, std::string("/device"));
            HERMES_CHECK(request1.body.find("client_id=client-id") != std::string::npos);
            const std::string response1 =
                "{\"device_code\":\"dev-code\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://example.com/verify\","
                "\"verification_uri_complete\":\"https://example.com/verify?code=ABCD-EFGH\",\"expires_in\":900,"
                "\"interval\":5,\"message\":\"Visit the provider site.\"}";
            WriteAll(client1,
                     "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                     "Content-Length: " + std::to_string(response1.size()) + "\r\n\r\n" + response1);
            ::close(client1);

            const int client2 = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client2 >= 0);
            const HttpRequest request2 = ReadHttpRequest(client2);
            HERMES_CHECK_EQ(request2.path, std::string("/token"));
            HERMES_CHECK(request2.body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code") !=
                         std::string::npos);
            const std::string response2 =
                "{\"error\":\"authorization_pending\",\"error_description\":\"Still waiting.\"}";
            WriteAll(client2,
                     "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nConnection: close\r\n"
                     "Content-Length: " + std::to_string(response2.size()) + "\r\n\r\n" + response2);
            ::close(client2);

            const int client3 = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client3 >= 0);
            const HttpRequest request3 = ReadHttpRequest(client3);
            HERMES_CHECK_EQ(request3.path, std::string("/token"));
            HERMES_CHECK(request3.body.find("device_code=dev-code") != std::string::npos);
            const std::string response3 =
                "{\"access_token\":\"access-1\",\"refresh_token\":\"refresh-1\",\"expires_in\":3600,"
                "\"scope\":\"scope.one scope.two\",\"token_type\":\"Bearer\"}";
            WriteAll(client3,
                     "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                     "Content-Length: " + std::to_string(response3.size()) + "\r\n\r\n" + response3);
            ::close(client3);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "oauth";
    account.email_address = "user@example.com";
    account.login_name = "user@example.com";
    account.uses_imap = true;
    account.imap_auth = hermes::ImapAuthMode::kOAuth2;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.device_authorization_endpoint = "http://127.0.0.1:" + std::to_string(port) + "/device";
    account.oauth.token_endpoint = "http://127.0.0.1:" + std::to_string(port) + "/token";
    account.oauth.scopes = {"scope.one", "scope.two"};

    hermes::InMemoryOAuthTokenStore token_store;
    hermes::InMemoryCredentialStore credential_store;
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService service(http_client, token_store, credential_store);

    hermes::OAuthDeviceAuthorization authorization;
    hermes::MailTaskErrorKind error_kind = hermes::MailTaskErrorKind::kUnknown;
    std::string error_message;
    HERMES_CHECK(service.BeginAuthorization(account, &authorization, &error_kind, &error_message));
    HERMES_CHECK_EQ(authorization.device_code, std::string("dev-code"));
    HERMES_CHECK_EQ(authorization.user_code, std::string("ABCD-EFGH"));

    int next_interval = 0;
    hermes::OAuthTokenRecord token_record;
    HERMES_CHECK_EQ(service.PollAuthorization(account,
                                              authorization,
                                              &next_interval,
                                              &token_record,
                                              &error_kind,
                                              &error_message),
                    hermes::OAuthPollState::kAuthorizationPending);
    HERMES_CHECK_EQ(error_kind, hermes::MailTaskErrorKind::kAuthorizationPending);
    HERMES_CHECK_EQ(next_interval, 5);

    error_kind = hermes::MailTaskErrorKind::kUnknown;
    error_message.clear();
    HERMES_CHECK_EQ(service.PollAuthorization(account,
                                              authorization,
                                              &next_interval,
                                              &token_record,
                                              &error_kind,
                                              &error_message),
                    hermes::OAuthPollState::kSucceeded);
    HERMES_CHECK_EQ(token_record.access_token, std::string("access-1"));
    const auto saved = token_store.LoadToken("oauth");
    HERMES_CHECK(static_cast<bool>(saved));
    HERMES_CHECK_EQ(saved->refresh_token, std::string("refresh-1"));

    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }
}

HERMES_TEST(OAuthDeviceFlowServiceRefreshesExpiredAccessToken) {
    std::uint16_t port = 0;
    const int listener = CreateListener(&port);
    HERMES_CHECK(listener >= 0);

    std::exception_ptr server_failure;
    std::thread server([&]() {
        try {
            const int client = ::accept(listener, nullptr, nullptr);
            HERMES_CHECK(client >= 0);
            const HttpRequest request = ReadHttpRequest(client);
            HERMES_CHECK_EQ(request.path, std::string("/token"));
            HERMES_CHECK(request.body.find("grant_type=refresh_token") != std::string::npos);
            HERMES_CHECK(request.body.find("refresh_token=refresh-1") != std::string::npos);
            const std::string response =
                "{\"access_token\":\"access-2\",\"expires_in\":1800,\"scope\":\"scope.one\",\"token_type\":\"Bearer\"}";
            WriteAll(client,
                     "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                     "Content-Length: " + std::to_string(response.size()) + "\r\n\r\n" + response);
            ::close(client);
            ::close(listener);
        } catch (...) {
            server_failure = std::current_exception();
        }
    });

    hermes::AccountProfile account;
    account.id = "oauth";
    account.email_address = "user@example.com";
    account.login_name = "user@example.com";
    account.uses_pop = true;
    account.pop_auth = hermes::PopAuthMode::kOAuth2;
    account.oauth.provider_kind = hermes::OAuthProviderKind::kCustom;
    account.oauth.client_id = "client-id";
    account.oauth.token_endpoint = "http://127.0.0.1:" + std::to_string(port) + "/token";
    account.oauth.device_authorization_endpoint = "http://127.0.0.1:" + std::to_string(port) + "/device";
    account.oauth.scopes = {"scope.one"};

    hermes::InMemoryOAuthTokenStore token_store;
    hermes::OAuthTokenRecord existing;
    existing.account_id = "oauth";
    existing.refresh_token = "refresh-1";
    existing.access_token = "expired";
    existing.expires_at_unix = 1;
    existing.issued_at_unix = 1;
    std::string error_message;
    HERMES_CHECK(token_store.SaveToken(existing, &error_message));

    hermes::InMemoryCredentialStore credential_store;
    hermes::OpenSslTlsProvider tls_provider;
    hermes::SocketTransportService transport(&tls_provider);
    hermes::TransportOAuthHttpClient http_client(transport, tls_provider);
    hermes::OAuthDeviceFlowService service(http_client, token_store, credential_store);

    hermes::MailTaskErrorKind error_kind = hermes::MailTaskErrorKind::kUnknown;
    std::string access_token;
    HERMES_CHECK(service.AcquireAccessToken(account, false, &access_token, &error_kind, &error_message));
    HERMES_CHECK_EQ(access_token, std::string("access-2"));
    const auto saved = token_store.LoadToken("oauth");
    HERMES_CHECK(static_cast<bool>(saved));
    HERMES_CHECK_EQ(saved->access_token, std::string("access-2"));

    server.join();
    if (server_failure) {
        std::rethrow_exception(server_failure);
    }
}

#endif
