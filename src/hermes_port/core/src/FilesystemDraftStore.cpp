#include "hermes/DraftStore.h"

#include "EudoraStorage.h"

namespace hermes {

FilesystemDraftStore::FilesystemDraftStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemDraftStore::SaveDraft(const ComposeMessage& draft, std::string* error_message) {
    return eudora::SaveDraft(root_directory_, draft, error_message);
}

std::optional<ComposeMessage> FilesystemDraftStore::GetDraft(std::string_view draft_id) const {
    return eudora::GetDraft(root_directory_, draft_id);
}

std::vector<ComposeMessage> FilesystemDraftStore::ListDrafts() const {
    return eudora::ListDrafts(root_directory_);
}

std::filesystem::path FilesystemDraftStore::RootDirectory() const {
    return root_directory_;
}

std::filesystem::path FilesystemDraftStore::DraftDirectory(std::string_view draft_id) const {
    return root_directory_ / "DraftState" / std::string(draft_id);
}

std::filesystem::path FilesystemDraftStore::MetadataPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "draft.ini";
}

std::filesystem::path FilesystemDraftStore::PlainBodyPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "body.txt";
}

std::filesystem::path FilesystemDraftStore::HtmlBodyPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "body.html";
}

std::filesystem::path FilesystemDraftStore::AttachmentsDirectory(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "Attachments";
}

}  // namespace hermes
