#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/RichTextSurface.h"

namespace hermes {

struct SignatureTemplate {
    std::string name;
    std::filesystem::path source_path;
    RichTextDocument body;
};

class SignatureStore {
public:
    virtual ~SignatureStore() = default;

    virtual void SetRootDirectory(std::filesystem::path directory) = 0;
    virtual std::filesystem::path RootDirectory() const = 0;
    virtual bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) = 0;
    virtual std::vector<SignatureTemplate> Templates() const = 0;
    virtual std::optional<SignatureTemplate> Find(std::string_view name) const = 0;
    virtual bool SaveTemplate(const SignatureTemplate& signature, std::string* error_message = nullptr) = 0;
    virtual bool DeleteTemplate(std::string_view name, std::string* error_message = nullptr) = 0;
};

class FilesystemSignatureStore final : public SignatureStore {
public:
    void SetRootDirectory(std::filesystem::path directory) override;
    std::filesystem::path RootDirectory() const override;
    bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) override;
    std::vector<SignatureTemplate> Templates() const override;
    std::optional<SignatureTemplate> Find(std::string_view name) const override;
    bool SaveTemplate(const SignatureTemplate& signature, std::string* error_message = nullptr) override;
    bool DeleteTemplate(std::string_view name, std::string* error_message = nullptr) override;

private:
    static std::optional<SignatureTemplate> ParseTemplate(const std::filesystem::path& path,
                                                          std::string* error_message);
    static std::string Normalize(std::string_view value);

    std::filesystem::path root_directory_;
    std::vector<SignatureTemplate> templates_;
};

}  // namespace hermes
