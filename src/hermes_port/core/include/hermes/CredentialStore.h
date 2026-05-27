#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace hermes {

enum class CredentialKind {
    kIncoming,
    kOutgoing,
};

class CredentialStore {
public:
    virtual ~CredentialStore() = default;

    virtual bool SaveCredential(std::string_view account_id,
                                CredentialKind kind,
                                std::string_view value,
                                std::string* error_message = nullptr) = 0;
    virtual std::optional<std::string> LoadCredential(std::string_view account_id,
                                                      CredentialKind kind) const = 0;
};

class InMemoryCredentialStore final : public CredentialStore {
public:
    bool SaveCredential(std::string_view account_id,
                        CredentialKind kind,
                        std::string_view value,
                        std::string* error_message = nullptr) override;
    std::optional<std::string> LoadCredential(std::string_view account_id,
                                              CredentialKind kind) const override;

private:
    std::map<std::string, std::string> credentials_;
};

class FilesystemCredentialStore final : public CredentialStore {
public:
    explicit FilesystemCredentialStore(std::filesystem::path root_directory);

    bool SaveCredential(std::string_view account_id,
                        CredentialKind kind,
                        std::string_view value,
                        std::string* error_message = nullptr) override;
    std::optional<std::string> LoadCredential(std::string_view account_id,
                                              CredentialKind kind) const override;

private:
    std::filesystem::path CredentialsPath() const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
