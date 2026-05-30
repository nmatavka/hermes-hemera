#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/TlsProvider.h"
#include "hermes/TransportService.h"

namespace hermes {

struct OAuthResolvedSettings {
    OAuthProviderKind provider_kind = OAuthProviderKind::kNone;
    std::string device_authorization_endpoint;
    std::string token_endpoint;
    OAuthAuthMechanism auth_mechanism = OAuthAuthMechanism::kXOAUTH2;
    std::string client_id;
    std::string tenant_or_domain;
    std::vector<std::string> scopes;
    bool client_secret_required = false;
};

struct OAuthTokenRecord {
    std::string account_id;
    std::string refresh_token;
    std::string access_token;
    std::int64_t expires_at_unix = 0;
    std::int64_t issued_at_unix = 0;
    std::vector<std::string> granted_scopes;
    std::string token_type;
    std::string last_auth_account_hint;
};

struct OAuthTokenStatus {
    bool authorized = false;
    bool has_refresh_token = false;
    bool access_token_valid = false;
    std::int64_t expires_at_unix = 0;
    std::string last_auth_account_hint;
};

struct OAuthDeviceAuthorization {
    std::string device_code;
    std::string user_code;
    std::string verification_uri;
    std::string verification_uri_complete;
    std::string message;
    int interval_seconds = 5;
    int expires_in_seconds = 900;
    std::int64_t requested_at_unix = 0;
};

enum class OAuthPollState {
    kSucceeded,
    kAuthorizationPending,
    kSlowDown,
    kAuthorizationDenied,
    kExpiredToken,
};

struct OAuthHttpResponse {
    int status_code = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

class OAuthTokenStore {
public:
    virtual ~OAuthTokenStore() = default;

    virtual bool SaveToken(const OAuthTokenRecord& record, std::string* error_message = nullptr) = 0;
    virtual std::optional<OAuthTokenRecord> LoadToken(std::string_view account_id) const = 0;
    virtual bool DeleteToken(std::string_view account_id, std::string* error_message = nullptr) = 0;
};

class InMemoryOAuthTokenStore final : public OAuthTokenStore {
public:
    bool SaveToken(const OAuthTokenRecord& record, std::string* error_message = nullptr) override;
    std::optional<OAuthTokenRecord> LoadToken(std::string_view account_id) const override;
    bool DeleteToken(std::string_view account_id, std::string* error_message = nullptr) override;

private:
    std::map<std::string, OAuthTokenRecord> records_;
};

class FilesystemOAuthTokenStore final : public OAuthTokenStore {
public:
    explicit FilesystemOAuthTokenStore(std::filesystem::path root_directory);

    bool SaveToken(const OAuthTokenRecord& record, std::string* error_message = nullptr) override;
    std::optional<OAuthTokenRecord> LoadToken(std::string_view account_id) const override;
    bool DeleteToken(std::string_view account_id, std::string* error_message = nullptr) override;

private:
    std::filesystem::path TokensPath() const;

    std::filesystem::path root_directory_;
};

class OAuthHttpClient {
public:
    virtual ~OAuthHttpClient() = default;

    virtual bool PostForm(std::string_view url,
                          const std::map<std::string, std::string>& form_fields,
                          OAuthHttpResponse* response,
                          std::string* error_message = nullptr) const = 0;
};

class TransportOAuthHttpClient final : public OAuthHttpClient {
public:
    TransportOAuthHttpClient(TransportService& transport_service, const TlsProvider& tls_provider);

    bool PostForm(std::string_view url,
                  const std::map<std::string, std::string>& form_fields,
                  OAuthHttpResponse* response,
                  std::string* error_message = nullptr) const override;

private:
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
};

class OAuthDeviceFlowService {
public:
    OAuthDeviceFlowService(OAuthHttpClient& http_client,
                           OAuthTokenStore& token_store,
                           CredentialStore& credential_store);

    std::optional<OAuthResolvedSettings> ResolveSettings(const AccountProfile& account,
                                                         MailTaskErrorKind* error_kind = nullptr,
                                                         std::string* error_message = nullptr) const;
    OAuthTokenStatus TokenStatus(std::string_view account_id) const;
    bool ForgetTokens(std::string_view account_id, std::string* error_message = nullptr);
    bool BeginAuthorization(const AccountProfile& account,
                            OAuthDeviceAuthorization* authorization,
                            MailTaskErrorKind* error_kind = nullptr,
                            std::string* error_message = nullptr) const;
    OAuthPollState PollAuthorization(const AccountProfile& account,
                                     const OAuthDeviceAuthorization& authorization,
                                     int* next_interval_seconds,
                                     OAuthTokenRecord* token_record,
                                     MailTaskErrorKind* error_kind = nullptr,
                                     std::string* error_message = nullptr);
    bool AcquireAccessToken(const AccountProfile& account,
                            bool force_refresh,
                            std::string* access_token,
                            MailTaskErrorKind* error_kind = nullptr,
                            std::string* error_message = nullptr);

private:
    bool RefreshAccessToken(const AccountProfile& account,
                            const OAuthResolvedSettings& settings,
                            OAuthTokenRecord* token_record,
                            MailTaskErrorKind* error_kind,
                            std::string* error_message);

    OAuthHttpClient& http_client_;
    OAuthTokenStore& token_store_;
    CredentialStore& credential_store_;
};

}  // namespace hermes
