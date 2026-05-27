#include "hermes/CredentialStore.h"

#include <filesystem>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string MakeCredentialKey(std::string_view account_id, CredentialKind kind) {
    return std::string(account_id) + ":" + (kind == CredentialKind::kIncoming ? "incoming" : "outgoing");
}

std::string KindKey(CredentialKind kind) {
    return kind == CredentialKind::kIncoming ? "Incoming" : "Outgoing";
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

std::filesystem::path FilesystemCredentialStore::CredentialsPath() const {
    return root_directory_ / "credentials.ini";
}

}  // namespace hermes
