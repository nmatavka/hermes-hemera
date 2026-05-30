#include "hermes/OAuthSupport.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "hermes/HemeraIdentity.h"
#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::int64_t NowUnixSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string JoinScopes(const std::vector<std::string>& scopes) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < scopes.size(); ++index) {
        if (index != 0) {
            stream << ' ';
        }
        stream << scopes[index];
    }
    return stream.str();
}

std::vector<std::string> SplitScopes(std::string_view value) {
    std::vector<std::string> scopes;
    std::istringstream stream{std::string(value)};
    std::string scope;
    while (stream >> scope) {
        scopes.push_back(scope);
    }
    return scopes;
}

std::string UrlEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            encoded.push_back('+');
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[(ch >> 4) & 0x0f]);
            encoded.push_back(kHex[ch & 0x0f]);
        }
    }
    return encoded;
}

std::string FormEncode(const std::map<std::string, std::string>& form_fields) {
    std::ostringstream stream;
    bool first = true;
    for (const auto& [key, value] : form_fields) {
        if (!first) {
            stream << '&';
        }
        first = false;
        stream << UrlEncode(key) << '=' << UrlEncode(value);
    }
    return stream.str();
}

struct ParsedUrl {
    TransportSecurity security = TransportSecurity::kPlaintext;
    std::string host;
    std::uint16_t port = 0;
    std::string path;
};

std::optional<ParsedUrl> ParseUrl(std::string_view url, std::string* error_message) {
    ParsedUrl parsed;
    std::size_t scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos) {
        if (error_message) {
            *error_message = "OAuth endpoint is missing a URL scheme.";
        }
        return std::nullopt;
    }

    const std::string scheme = ToLower(std::string(url.substr(0, scheme_end)));
    if (scheme == "https") {
        parsed.security = TransportSecurity::kImplicitTls;
        parsed.port = 443;
    } else if (scheme == "http") {
        parsed.security = TransportSecurity::kPlaintext;
        parsed.port = 80;
    } else {
        if (error_message) {
            *error_message = "OAuth endpoint uses an unsupported scheme: " + scheme;
        }
        return std::nullopt;
    }

    const std::size_t authority_start = scheme_end + 3;
    const std::size_t path_start = url.find('/', authority_start);
    const std::string_view authority =
        path_start == std::string_view::npos ? url.substr(authority_start) : url.substr(authority_start, path_start - authority_start);
    if (authority.empty()) {
        if (error_message) {
            *error_message = "OAuth endpoint is missing a host.";
        }
        return std::nullopt;
    }

    const std::size_t colon = authority.rfind(':');
    if (colon != std::string_view::npos && authority.find(']') == std::string_view::npos) {
        parsed.host = std::string(authority.substr(0, colon));
        const std::string port_text(authority.substr(colon + 1));
        unsigned int port = 0;
        const auto [ptr, ec] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
        if (ec != std::errc() || ptr != port_text.data() + port_text.size() || port == 0 || port > 65535) {
            if (error_message) {
                *error_message = "OAuth endpoint has an invalid port.";
            }
            return std::nullopt;
        }
        parsed.port = static_cast<std::uint16_t>(port);
    } else {
        parsed.host = std::string(authority);
    }

    if (parsed.host.empty()) {
        if (error_message) {
            *error_message = "OAuth endpoint is missing a host.";
        }
        return std::nullopt;
    }

    parsed.path = path_start == std::string_view::npos ? "/" : std::string(url.substr(path_start));
    if (parsed.path.empty()) {
        parsed.path = "/";
    }
    return parsed;
}

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<std::map<std::string, std::string>> ParseFlatJsonObject(std::string_view json,
                                                                      std::string* error_message) {
    std::size_t index = 0;
    auto skip_ws = [&]() {
        while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
            ++index;
        }
    };

    auto parse_string = [&](std::string* out) -> bool {
        if (index >= json.size() || json[index] != '"') {
            return false;
        }
        ++index;
        std::string value;
        while (index < json.size()) {
            const char ch = json[index++];
            if (ch == '"') {
                *out = std::move(value);
                return true;
            }
            if (ch != '\\') {
                value.push_back(ch);
                continue;
            }
            if (index >= json.size()) {
                return false;
            }
            const char escaped = json[index++];
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'u':
                    if (index + 4 > json.size()) {
                        return false;
                    }
                    value.append(json.substr(index - 2, 6));
                    index += 4;
                    break;
                default:
                    return false;
            }
        }
        return false;
    };

    auto parse_literal = [&]() -> std::optional<std::string> {
        if (index >= json.size()) {
            return std::nullopt;
        }
        if (json[index] == '"') {
            std::string value;
            if (!parse_string(&value)) {
                return std::nullopt;
            }
            return value;
        }
        const std::size_t start = index;
        if (json[index] == '-' || std::isdigit(static_cast<unsigned char>(json[index]))) {
            ++index;
            while (index < json.size() &&
                   (std::isdigit(static_cast<unsigned char>(json[index])) || json[index] == '.' ||
                    json[index] == 'e' || json[index] == 'E' || json[index] == '+' || json[index] == '-')) {
                ++index;
            }
            return std::string(json.substr(start, index - start));
        }
        if (json.compare(index, 4, "true") == 0) {
            index += 4;
            return std::string("true");
        }
        if (json.compare(index, 5, "false") == 0) {
            index += 5;
            return std::string("false");
        }
        if (json.compare(index, 4, "null") == 0) {
            index += 4;
            return std::string();
        }
        return std::nullopt;
    };

    skip_ws();
    if (index >= json.size() || json[index] != '{') {
        if (error_message) {
            *error_message = "OAuth response did not contain a JSON object.";
        }
        return std::nullopt;
    }
    ++index;

    std::map<std::string, std::string> values;
    while (true) {
        skip_ws();
        if (index >= json.size()) {
            break;
        }
        if (json[index] == '}') {
            ++index;
            return values;
        }

        std::string key;
        if (!parse_string(&key)) {
            break;
        }
        skip_ws();
        if (index >= json.size() || json[index] != ':') {
            break;
        }
        ++index;
        skip_ws();
        const auto value = parse_literal();
        if (!value.has_value()) {
            break;
        }
        values.emplace(std::move(key), *value);
        skip_ws();
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == '}') {
            ++index;
            return values;
        }
        break;
    }

    if (error_message) {
        *error_message = "OAuth response contained malformed JSON.";
    }
    return std::nullopt;
}

std::string GetJsonValue(const std::map<std::string, std::string>& values, std::string_view key) {
    const auto it = values.find(std::string(key));
    return it == values.end() ? std::string() : it->second;
}

std::string CredentialSection(std::string_view account_id) {
    return std::string(account_id);
}

std::string KindKey(std::string_view base, std::string_view suffix) {
    return std::string(base) + std::string(suffix);
}

std::optional<OAuthResolvedSettings> ResolveProviderSettings(const AccountProfile& account,
                                                             MailTaskErrorKind* error_kind,
                                                             std::string* error_message) {
    if ((account.pop_auth != PopAuthMode::kOAuth2) &&
        (account.imap_auth != ImapAuthMode::kOAuth2) &&
        (account.smtp_auth != SmtpAuthMode::kOAuth2)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth is not enabled for this account.";
        }
        return std::nullopt;
    }

    OAuthResolvedSettings resolved;
    resolved.provider_kind = account.oauth.provider_kind;
    resolved.auth_mechanism = account.oauth.auth_mechanism;
    resolved.client_id = account.oauth.client_id;
    resolved.tenant_or_domain = account.oauth.tenant_or_domain;
    resolved.client_secret_required = account.oauth.client_secret_required;
    resolved.scopes = account.oauth.scopes;

    switch (account.oauth.provider_kind) {
        case OAuthProviderKind::kMicrosoft365: {
            const std::string tenant = account.oauth.tenant_or_domain.empty() ? "common" : account.oauth.tenant_or_domain;
            resolved.device_authorization_endpoint =
                "https://login.microsoftonline.com/" + tenant + "/oauth2/v2.0/devicecode";
            resolved.token_endpoint =
                "https://login.microsoftonline.com/" + tenant + "/oauth2/v2.0/token";
            if (resolved.scopes.empty()) {
                resolved.scopes.push_back("offline_access");
                if (account.uses_imap && account.imap_auth == ImapAuthMode::kOAuth2) {
                    resolved.scopes.push_back("https://outlook.office.com/IMAP.AccessAsUser.All");
                }
                if (account.uses_pop && account.pop_auth == PopAuthMode::kOAuth2) {
                    resolved.scopes.push_back("https://outlook.office.com/POP.AccessAsUser.All");
                }
                if (account.smtp_auth == SmtpAuthMode::kOAuth2) {
                    resolved.scopes.push_back("https://outlook.office.com/SMTP.Send");
                }
            }
            break;
        }
        case OAuthProviderKind::kGoogle:
            resolved.device_authorization_endpoint = "https://oauth2.googleapis.com/device/code";
            resolved.token_endpoint = "https://oauth2.googleapis.com/token";
            if (resolved.scopes.empty()) {
                resolved.scopes.push_back("https://mail.google.com/");
            }
            break;
        case OAuthProviderKind::kCustom:
            resolved.device_authorization_endpoint = account.oauth.device_authorization_endpoint;
            resolved.token_endpoint = account.oauth.token_endpoint;
            break;
        case OAuthProviderKind::kNone:
            resolved.device_authorization_endpoint = account.oauth.device_authorization_endpoint;
            resolved.token_endpoint = account.oauth.token_endpoint;
            break;
    }

    if (resolved.client_id.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth client ID is not configured.";
        }
        return std::nullopt;
    }
    if (resolved.device_authorization_endpoint.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth device authorization endpoint is not configured.";
        }
        return std::nullopt;
    }
    if (resolved.token_endpoint.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth token endpoint is not configured.";
        }
        return std::nullopt;
    }
    if (resolved.scopes.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth scopes are not configured.";
        }
        return std::nullopt;
    }

    return resolved;
}

bool ParseTokenResponse(const std::map<std::string, std::string>& values,
                        std::string_view account_id,
                        OAuthTokenRecord* token_record,
                        MailTaskErrorKind* error_kind,
                        std::string* error_message) {
    const std::string access_token = GetJsonValue(values, "access_token");
    if (access_token.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        if (error_message) {
            *error_message = "OAuth token response did not include an access token.";
        }
        return false;
    }

    const std::string expires_in_text = GetJsonValue(values, "expires_in");
    std::int64_t expires_in = 3600;
    if (!expires_in_text.empty()) {
        expires_in = std::strtoll(expires_in_text.c_str(), nullptr, 10);
    }

    OAuthTokenRecord updated = token_record ? *token_record : OAuthTokenRecord{};
    updated.account_id = std::string(account_id);
    updated.access_token = access_token;
    if (!GetJsonValue(values, "refresh_token").empty()) {
        updated.refresh_token = GetJsonValue(values, "refresh_token");
    }
    updated.issued_at_unix = NowUnixSeconds();
    updated.expires_at_unix = updated.issued_at_unix + std::max<std::int64_t>(expires_in, 0);
    updated.granted_scopes = SplitScopes(GetJsonValue(values, "scope"));
    updated.token_type = GetJsonValue(values, "token_type");
    if (updated.token_type.empty()) {
        updated.token_type = "Bearer";
    }
    updated.last_auth_account_hint = GetJsonValue(values, "id_token");

    if (token_record) {
        *token_record = std::move(updated);
    }
    return true;
}

}  // namespace

bool InMemoryOAuthTokenStore::SaveToken(const OAuthTokenRecord& record, std::string* error_message) {
    (void)error_message;
    records_[record.account_id] = record;
    return true;
}

std::optional<OAuthTokenRecord> InMemoryOAuthTokenStore::LoadToken(std::string_view account_id) const {
    const auto it = records_.find(std::string(account_id));
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryOAuthTokenStore::DeleteToken(std::string_view account_id, std::string* error_message) {
    (void)error_message;
    return records_.erase(std::string(account_id)) > 0;
}

FilesystemOAuthTokenStore::FilesystemOAuthTokenStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemOAuthTokenStore::SaveToken(const OAuthTokenRecord& record, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(root_directory_, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create OAuth token directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore settings;
    if (std::filesystem::exists(TokensPath())) {
        std::string ignored;
        settings.LoadFromFile(TokensPath(), &ignored);
    }

    const std::string section = CredentialSection(record.account_id);
    settings.SetString(section, "RefreshToken", record.refresh_token);
    settings.SetString(section, "AccessToken", record.access_token);
    settings.SetString(section, "ExpiresAtUnix", std::to_string(static_cast<long long>(record.expires_at_unix)));
    settings.SetString(section, "IssuedAtUnix", std::to_string(static_cast<long long>(record.issued_at_unix)));
    settings.SetString(section, "GrantedScopes", JoinScopes(record.granted_scopes));
    settings.SetString(section, "TokenType", record.token_type);
    settings.SetString(section, "LastAuthAccountHint", record.last_auth_account_hint);
    return settings.SaveToFile(TokensPath(), error_message);
}

std::optional<OAuthTokenRecord> FilesystemOAuthTokenStore::LoadToken(std::string_view account_id) const {
    if (!std::filesystem::exists(TokensPath())) {
        return std::nullopt;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(TokensPath(), &ignored)) {
        return std::nullopt;
    }

    const auto access_token = settings.GetString(account_id, "AccessToken");
    const auto refresh_token = settings.GetString(account_id, "RefreshToken");
    if (!access_token && !refresh_token) {
        return std::nullopt;
    }

    OAuthTokenRecord record;
    record.account_id = std::string(account_id);
    record.access_token = access_token.value_or("");
    record.refresh_token = refresh_token.value_or("");
    record.expires_at_unix = std::strtoll(settings.GetString(account_id, "ExpiresAtUnix").value_or("0").c_str(), nullptr, 10);
    record.issued_at_unix = std::strtoll(settings.GetString(account_id, "IssuedAtUnix").value_or("0").c_str(), nullptr, 10);
    record.granted_scopes = SplitScopes(settings.GetString(account_id, "GrantedScopes").value_or(""));
    record.token_type = settings.GetString(account_id, "TokenType").value_or("Bearer");
    record.last_auth_account_hint = settings.GetString(account_id, "LastAuthAccountHint").value_or("");
    return record;
}

bool FilesystemOAuthTokenStore::DeleteToken(std::string_view account_id, std::string* error_message) {
    if (!std::filesystem::exists(TokensPath())) {
        return true;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(TokensPath(), &ignored)) {
        if (error_message) {
            *error_message = "Unable to load OAuth token store.";
        }
        return false;
    }

    static constexpr const char* kKeys[] = {
        "RefreshToken",
        "AccessToken",
        "ExpiresAtUnix",
        "IssuedAtUnix",
        "GrantedScopes",
        "TokenType",
        "LastAuthAccountHint",
    };
    for (const char* key : kKeys) {
        settings.RemoveValue(account_id, key);
    }
    return settings.SaveToFile(TokensPath(), error_message);
}

std::filesystem::path FilesystemOAuthTokenStore::TokensPath() const {
    return root_directory_ / "oauth_tokens.ini";
}

TransportOAuthHttpClient::TransportOAuthHttpClient(TransportService& transport_service, const TlsProvider& tls_provider)
    : transport_service_(transport_service), tls_provider_(tls_provider) {}

bool TransportOAuthHttpClient::PostForm(std::string_view url,
                                        const std::map<std::string, std::string>& form_fields,
                                        OAuthHttpResponse* response,
                                        std::string* error_message) const {
    const auto parsed = ParseUrl(url, error_message);
    if (!parsed) {
        return false;
    }

    auto connection = transport_service_.Connect(
        {parsed->host, parsed->port, parsed->security, 5000},
        error_message);
    if (!connection) {
        return false;
    }

    const std::string body = FormEncode(form_fields);
    std::ostringstream request;
    request << "POST " << parsed->path << " HTTP/1.1\r\n"
            << "Host: " << parsed->host << "\r\n"
            << "User-Agent: " << kHemeraOAuthUserAgent << "\r\n"
            << "Accept: application/json\r\n"
            << "Content-Type: application/x-www-form-urlencoded\r\n"
            << "Connection: close\r\n"
            << "Content-Length: " << body.size() << "\r\n\r\n"
            << body;
    if (!connection->Send(request.str(), error_message)) {
        return false;
    }

    std::string status_line;
    if (!connection->ReceiveLine(&status_line, error_message)) {
        return false;
    }
    std::istringstream status_stream(status_line);
    std::string http_version;
    int status_code = 0;
    status_stream >> http_version >> status_code;
    if (status_code <= 0) {
        if (error_message) {
            *error_message = "OAuth endpoint returned an invalid HTTP status line.";
        }
        return false;
    }

    std::map<std::string, std::string> headers;
    std::string line;
    while (connection->ReceiveLine(&line, error_message)) {
        if (line.empty()) {
            break;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        headers.emplace(ToLower(Trim(line.substr(0, colon))), Trim(line.substr(colon + 1)));
    }

    std::string response_body;
    const auto transfer_encoding = headers.find("transfer-encoding");
    if (transfer_encoding != headers.end() && ToLower(transfer_encoding->second).find("chunked") != std::string::npos) {
        while (true) {
            std::string size_line;
            if (!connection->ReceiveLine(&size_line, error_message)) {
                return false;
            }
            const std::size_t semicolon = size_line.find(';');
            const std::string hex_size = semicolon == std::string::npos ? size_line : size_line.substr(0, semicolon);
            const std::size_t chunk_size = static_cast<std::size_t>(std::stoull(hex_size, nullptr, 16));
            if (chunk_size == 0) {
                connection->ReceiveLine(&line, nullptr);
                break;
            }
            std::size_t remaining = chunk_size;
            while (remaining > 0) {
                const std::string chunk = connection->Receive(remaining, error_message);
                if (chunk.empty()) {
                    return false;
                }
                response_body += chunk;
                remaining -= std::min(remaining, chunk.size());
            }
            connection->ReceiveLine(&line, nullptr);
        }
    } else if (const auto length_it = headers.find("content-length"); length_it != headers.end()) {
        std::size_t remaining = static_cast<std::size_t>(std::stoull(length_it->second));
        while (remaining > 0) {
            const std::string chunk = connection->Receive(remaining, error_message);
            if (chunk.empty()) {
                return false;
            }
            response_body += chunk;
            remaining -= chunk.size();
        }
    } else {
        while (true) {
            const std::string chunk = connection->Receive(4096, nullptr);
            if (chunk.empty()) {
                break;
            }
            response_body += chunk;
        }
    }

    if (response) {
        response->status_code = status_code;
        response->headers = std::move(headers);
        response->body = std::move(response_body);
    }
    return true;
}

OAuthDeviceFlowService::OAuthDeviceFlowService(OAuthHttpClient& http_client,
                                               OAuthTokenStore& token_store,
                                               CredentialStore& credential_store)
    : http_client_(http_client),
      token_store_(token_store),
      credential_store_(credential_store) {}

std::optional<OAuthResolvedSettings> OAuthDeviceFlowService::ResolveSettings(const AccountProfile& account,
                                                                             MailTaskErrorKind* error_kind,
                                                                             std::string* error_message) const {
    return ResolveProviderSettings(account, error_kind, error_message);
}

OAuthTokenStatus OAuthDeviceFlowService::TokenStatus(std::string_view account_id) const {
    OAuthTokenStatus status;
    const auto record = token_store_.LoadToken(account_id);
    if (!record) {
        return status;
    }

    status.authorized = !record->access_token.empty() || !record->refresh_token.empty();
    status.has_refresh_token = !record->refresh_token.empty();
    status.access_token_valid = !record->access_token.empty() && record->expires_at_unix > (NowUnixSeconds() + 60);
    status.expires_at_unix = record->expires_at_unix;
    status.last_auth_account_hint = record->last_auth_account_hint;
    return status;
}

bool OAuthDeviceFlowService::ForgetTokens(std::string_view account_id, std::string* error_message) {
    return token_store_.DeleteToken(account_id, error_message);
}

bool OAuthDeviceFlowService::BeginAuthorization(const AccountProfile& account,
                                                OAuthDeviceAuthorization* authorization,
                                                MailTaskErrorKind* error_kind,
                                                std::string* error_message) const {
    if (authorization == nullptr) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth authorization state is unavailable.";
        }
        return false;
    }

    const auto resolved = ResolveProviderSettings(account, error_kind, error_message);
    if (!resolved) {
        return false;
    }

    std::map<std::string, std::string> form_fields{
        {"client_id", resolved->client_id},
        {"scope", JoinScopes(resolved->scopes)},
    };
    OAuthHttpResponse response;
    if (!http_client_.PostForm(resolved->device_authorization_endpoint, form_fields, &response, error_message)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        return false;
    }

    const auto values = ParseFlatJsonObject(response.body, error_message);
    if (!values) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        return false;
    }

    if (!GetJsonValue(*values, "error").empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = GetJsonValue(*values, "error_description");
            if (error_message->empty()) {
                *error_message = GetJsonValue(*values, "error");
            }
        }
        return false;
    }

    authorization->device_code = GetJsonValue(*values, "device_code");
    authorization->user_code = GetJsonValue(*values, "user_code");
    authorization->verification_uri = GetJsonValue(*values, "verification_uri");
    if (authorization->verification_uri.empty()) {
        authorization->verification_uri = GetJsonValue(*values, "verification_url");
    }
    authorization->verification_uri_complete = GetJsonValue(*values, "verification_uri_complete");
    authorization->message = GetJsonValue(*values, "message");
    authorization->interval_seconds = std::max(1, std::atoi(GetJsonValue(*values, "interval").c_str()));
    authorization->expires_in_seconds = std::max(1, std::atoi(GetJsonValue(*values, "expires_in").c_str()));
    authorization->requested_at_unix = NowUnixSeconds();

    if (authorization->device_code.empty() || authorization->user_code.empty() || authorization->verification_uri.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        if (error_message) {
            *error_message = "OAuth device-code response did not include the expected fields.";
        }
        return false;
    }
    return true;
}

OAuthPollState OAuthDeviceFlowService::PollAuthorization(const AccountProfile& account,
                                                         const OAuthDeviceAuthorization& authorization,
                                                         int* next_interval_seconds,
                                                         OAuthTokenRecord* token_record,
                                                         MailTaskErrorKind* error_kind,
                                                         std::string* error_message) {
    const auto resolved = ResolveProviderSettings(account, error_kind, error_message);
    if (!resolved) {
        return OAuthPollState::kAuthorizationDenied;
    }

    std::map<std::string, std::string> form_fields{
        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
        {"device_code", authorization.device_code},
        {"client_id", resolved->client_id},
    };
    if (resolved->client_secret_required) {
        const auto secret = credential_store_.LoadCredential(account.id, CredentialKind::kOAuthClientSecret);
        if (!secret || secret->empty()) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kProviderConfiguration;
            }
            if (error_message) {
                *error_message = "OAuth client secret is required for this provider.";
            }
            return OAuthPollState::kAuthorizationDenied;
        }
        form_fields["client_secret"] = *secret;
    }

    OAuthHttpResponse response;
    if (!http_client_.PostForm(resolved->token_endpoint, form_fields, &response, error_message)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        return OAuthPollState::kAuthorizationDenied;
    }

    const auto values = ParseFlatJsonObject(response.body, error_message);
    if (!values) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kProviderConfiguration;
        }
        return OAuthPollState::kAuthorizationDenied;
    }

    const std::string error = GetJsonValue(*values, "error");
    if (!error.empty()) {
        const std::string description = GetJsonValue(*values, "error_description");
        if (error == "authorization_pending") {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kAuthorizationPending;
            }
            if (error_message) {
                *error_message = description.empty() ? "Authorization is still pending." : description;
            }
            if (next_interval_seconds) {
                *next_interval_seconds = authorization.interval_seconds;
            }
            return OAuthPollState::kAuthorizationPending;
        }
        if (error == "slow_down") {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kAuthorizationPending;
            }
            if (error_message) {
                *error_message = description.empty() ? "OAuth provider requested slower polling." : description;
            }
            if (next_interval_seconds) {
                *next_interval_seconds = authorization.interval_seconds + 5;
            }
            return OAuthPollState::kSlowDown;
        }
        if (error == "access_denied") {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kAuthorizationDenied;
            }
            if (error_message) {
                *error_message = description.empty() ? "Authorization was denied." : description;
            }
            return OAuthPollState::kAuthorizationDenied;
        }
        if (error == "expired_token") {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kAuthorizationDenied;
            }
            if (error_message) {
                *error_message = description.empty() ? "The device authorization code expired." : description;
            }
            return OAuthPollState::kExpiredToken;
        }
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kAuthorizationDenied;
        }
        if (error_message) {
            *error_message = description.empty() ? error : description;
        }
        return OAuthPollState::kAuthorizationDenied;
    }

    OAuthTokenRecord updated;
    if (!ParseTokenResponse(*values, account.id, &updated, error_kind, error_message)) {
        return OAuthPollState::kAuthorizationDenied;
    }
    if (!token_store_.SaveToken(updated, error_message)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kCredentialAcquisitionFailed;
        }
        return OAuthPollState::kAuthorizationDenied;
    }
    if (token_record) {
        *token_record = std::move(updated);
    }
    if (next_interval_seconds) {
        *next_interval_seconds = authorization.interval_seconds;
    }
    return OAuthPollState::kSucceeded;
}

bool OAuthDeviceFlowService::AcquireAccessToken(const AccountProfile& account,
                                                bool force_refresh,
                                                std::string* access_token,
                                                MailTaskErrorKind* error_kind,
                                                std::string* error_message) {
    const auto resolved = ResolveProviderSettings(account, error_kind, error_message);
    if (!resolved) {
        return false;
    }

    auto token_record = token_store_.LoadToken(account.id);
    if (!token_record) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        if (error_message) {
            *error_message = "OAuth authorization has not been completed for this account.";
        }
        return false;
    }

    const std::int64_t refresh_threshold = NowUnixSeconds() + 60;
    if (!force_refresh && !token_record->access_token.empty() && token_record->expires_at_unix > refresh_threshold) {
        if (access_token) {
            *access_token = token_record->access_token;
        }
        return true;
    }

    if (!RefreshAccessToken(account, *resolved, &*token_record, error_kind, error_message)) {
        return false;
    }
    if (access_token) {
        *access_token = token_record->access_token;
    }
    return true;
}

bool OAuthDeviceFlowService::RefreshAccessToken(const AccountProfile& account,
                                                const OAuthResolvedSettings& settings,
                                                OAuthTokenRecord* token_record,
                                                MailTaskErrorKind* error_kind,
                                                std::string* error_message) {
    if (token_record == nullptr || token_record->refresh_token.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        if (error_message) {
            *error_message = "OAuth refresh token is unavailable for this account.";
        }
        return false;
    }

    std::map<std::string, std::string> form_fields{
        {"grant_type", "refresh_token"},
        {"refresh_token", token_record->refresh_token},
        {"client_id", settings.client_id},
    };
    if (!settings.scopes.empty()) {
        form_fields["scope"] = JoinScopes(settings.scopes);
    }
    if (settings.client_secret_required) {
        const auto secret = credential_store_.LoadCredential(account.id, CredentialKind::kOAuthClientSecret);
        if (!secret || secret->empty()) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kProviderConfiguration;
            }
            if (error_message) {
                *error_message = "OAuth client secret is required for this provider.";
            }
            return false;
        }
        form_fields["client_secret"] = *secret;
    }

    OAuthHttpResponse response;
    if (!http_client_.PostForm(settings.token_endpoint, form_fields, &response, error_message)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        return false;
    }

    const auto values = ParseFlatJsonObject(response.body, error_message);
    if (!values) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        return false;
    }

    const std::string error = GetJsonValue(*values, "error");
    if (!error.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        if (error_message) {
            *error_message = GetJsonValue(*values, "error_description");
            if (error_message->empty()) {
                *error_message = error;
            }
        }
        return false;
    }

    OAuthTokenRecord updated = *token_record;
    if (!ParseTokenResponse(*values, account.id, &updated, error_kind, error_message)) {
        if (error_kind && *error_kind == MailTaskErrorKind::kUnknown) {
            *error_kind = MailTaskErrorKind::kTokenRefreshFailed;
        }
        return false;
    }
    if (updated.refresh_token.empty()) {
        updated.refresh_token = token_record->refresh_token;
    }
    if (!token_store_.SaveToken(updated, error_message)) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kCredentialAcquisitionFailed;
        }
        return false;
    }
    *token_record = std::move(updated);
    return true;
}

}  // namespace hermes
