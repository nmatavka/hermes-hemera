#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/ComposeMessage.h"

namespace hermes {

class DraftStore {
public:
    virtual ~DraftStore() = default;

    virtual bool SaveDraft(const ComposeMessage& draft, std::string* error_message = nullptr) = 0;
    virtual std::optional<ComposeMessage> GetDraft(std::string_view draft_id) const = 0;
    virtual std::vector<ComposeMessage> ListDrafts() const = 0;
};

class FilesystemDraftStore final : public DraftStore {
public:
    explicit FilesystemDraftStore(std::filesystem::path root_directory);

    bool SaveDraft(const ComposeMessage& draft, std::string* error_message = nullptr) override;
    std::optional<ComposeMessage> GetDraft(std::string_view draft_id) const override;
    std::vector<ComposeMessage> ListDrafts() const override;

    std::filesystem::path RootDirectory() const;

private:
    std::filesystem::path DraftDirectory(std::string_view draft_id) const;
    std::filesystem::path MetadataPath(std::string_view draft_id) const;
    std::filesystem::path PlainBodyPath(std::string_view draft_id) const;
    std::filesystem::path HtmlBodyPath(std::string_view draft_id) const;
    std::filesystem::path AttachmentsDirectory(std::string_view draft_id) const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
