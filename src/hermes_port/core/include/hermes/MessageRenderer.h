#pragma once

#include <filesystem>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/RendererMode.h"
#include "hermes/RichTextSurface.h"

namespace hermes {

struct MessageRenderAttachment {
    std::string name;
    std::string content_type;
    std::filesystem::path payload_path;
    std::string content_id;
    std::string disposition;
    bool download_complete = true;
};

struct MessageRenderRequest {
    RendererMode preferred_mode = RendererMode::kPlainText;
    std::string mailbox_id;
    std::string message_id;
    std::string html_body;
    std::string plain_text_body;
    std::string rtf_body;
    std::string paige_native_body;
    StyledDocumentSource styled_source = StyledDocumentSource::kPlainText;
    StyledDocumentFidelity styled_fidelity = StyledDocumentFidelity::kLossless;
    std::string source_uri;
    bool allow_remote_content = false;
    bool read_only = true;
    std::vector<MessageRenderAttachment> attachments;
};

struct FindOptions {
    bool match_case = false;
    bool whole_word = false;
};

struct FindResult {
    bool matched = false;
    std::size_t match_count = 0;
};

class MessageRenderer {
public:
    virtual ~MessageRenderer() = default;

    virtual bool Load(const MessageRenderRequest& request) = 0;
    virtual RendererMode Mode() const = 0;
    virtual FindResult Find(std::string_view needle, const FindOptions& options) = 0;
    virtual std::string AllText() const = 0;
    virtual std::string AllHtml() const = 0;
    virtual bool CanPrint() const = 0;
    virtual bool CanDirectPrint() const = 0;
    virtual bool PrintPreview() = 0;
    virtual bool DirectPrint() = 0;
};

}  // namespace hermes
