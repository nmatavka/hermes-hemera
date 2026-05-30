#include "hermes/CredentialStore.h"

#include <filesystem>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string MakeCredentialKey(std::string_view account_id, CredentialKind kind) {
    switch (kind) {
        case CredentialKind::kIncoming:
            return std::string(account_id) + ":incoming";
        case CredentialKind::kOutgoing:
            return std::string(account_id) + ":outgoing";
        case CredentialKind::kOAuthClientSecret:
            return std::string(account_id) + ":oauth-client-secret";
    }
    return std::string(account_id) + ":incoming";
}

std::string KindKey(CredentialKind kind) {
    switch (kind) {
        case CredentialKind::kIncoming:
            return "Incoming";
        case CredentialKind::kOutgoing:
            return "Outgoing";
        case CredentialKind::kOAuthClientSecret:
            return "OAuthClientSecret";
    }
    return "Incoming";
}

}  // namespace

bool InMemoryCredentialStore::SaveCredential(std::string_view account_id,
                                             CredentialKind kind,
                                             std::string_view value,
                                             std::string* error_message) {
    (void)error_message;
    credentials_[MakeCredentialKey(account_id, kind)] = std::string(value);
    return true;
}

std::optional<std::string> InMemoryCredentialStore::LoadCredential(std::string_view account_id,
                                                                   CredentialKind kind) const {
    const auto it = credentials_.find(MakeCredentialKey(account_id, kind));
    if (it == credentials_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryCredentialStore::DeleteCredential(std::string_view account_id,
                                               CredentialKind kind,
                                               std::string* error_message) {
    (void)error_message;
    credentials_.erase(MakeCredentialKey(account_id, kind));
    return true;
}

bool InMemoryCredentialStore::ClearAllCredentials(std::string* error_message) {
    (void)error_message;
    credentials_.clear();
    return true;
}

FilesystemCredentialStore::FilesystemCredentialStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemCredentialStore::SaveCredential(std::string_view account_id,
                                               CredentialKind kind,
                                               std::string_view value,
                                               std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(root_directory_, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create credential directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore settings;
    if (std::filesystem::exists(CredentialsPath())) {
        std::string ignored;
        settings.LoadFromFile(CredentialsPath(), &ignored);
    }

    settings.SetString(account_id, KindKey(kind), value);
    return settings.SaveToFile(CredentialsPath(), error_message);
}

std::optional<std::string> FilesystemCredentialStore::LoadCredential(std::string_view account_id,
                                                                     CredentialKind kind) const {
    if (!std::filesystem::exists(CredentialsPath())) {
        return std::nullopt;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(CredentialsPath(), &ignored)) {
        return std::nullopt;
    }
    return settings.GetString(account_id, KindKey(kind));
}

bool FilesystemCredentialStore::DeleteCredential(std::string_view account_id,
                                                 CredentialKind kind,
                                                 std::string* error_message) {
    if (!std::filesystem::exists(CredentialsPath())) {
        return true;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(CredentialsPath(), &ignored)) {
        if (error_message) {
            *error_message = "Unable to load credential store.";
        }
        return false;
    }

    settings.RemoveValue(account_id, KindKey(kind));
    return settings.SaveToFile(CredentialsPath(), error_message);
}

bool FilesystemCredentialStore::ClearAllCredentials(std::string* error_message) {
    if (!std::filesystem::exists(CredentialsPath())) {
        return true;
    }

    std::error_code remove_error;
    std::filesystem::remove(CredentialsPath(), remove_error);
    if (remove_error) {
        if (error_message) {
            *error_message = "Unable to clear credential store: " + remove_error.message();
        }
        return false;
    }
    return true;
}

std::filesystem::path FilesystemCredentialStore::CredentialsPath() const {
    return root_directory_ / "credentials.ini";
}

}  // namespace hermes
