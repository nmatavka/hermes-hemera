#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class LinkHistoryKind {
    kUrl,
    kAttachment,
    kFile,
};

struct LinkHistoryEntry {
    std::string id;
    LinkHistoryKind kind = LinkHistoryKind::kUrl;
    std::string title;
    std::string target;
    std::string source_context;
    bool launched = true;
    std::int64_t timestamp = 0;
};

class LinkHistoryStore {
public:
    virtual ~LinkHistoryStore() = default;

    virtual bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const = 0;
    virtual std::vector<LinkHistoryEntry> Entries() const = 0;
    virtual void AddEntry(const LinkHistoryEntry& entry) = 0;
    virtual void Clear() = 0;
};

class FilesystemLinkHistoryStore final : public LinkHistoryStore {
public:
    bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const override;
    std::vector<LinkHistoryEntry> Entries() const override;
    void AddEntry(const LinkHistoryEntry& entry) override;
    void Clear() override;

private:
    std::vector<LinkHistoryEntry> entries_;
};

}  // namespace hermes
