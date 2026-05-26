#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "hermes/RendererMode.h"

namespace hermes {

struct MessageRenderRequest {
    RendererMode preferred_mode = RendererMode::kPlainText;
    std::string html_body;
    std::string plain_text_body;
    std::string source_uri;
    bool allow_remote_content = false;
    bool read_only = true;
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
    virtual bool PrintPreview() = 0;
};

}  // namespace hermes
