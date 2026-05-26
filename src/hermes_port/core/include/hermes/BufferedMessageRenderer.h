#pragma once

#include "hermes/MessageRenderer.h"

namespace hermes {

class BufferedMessageRenderer final : public MessageRenderer {
public:
    bool Load(const MessageRenderRequest& request) override;
    RendererMode Mode() const override;
    FindResult Find(std::string_view needle, const FindOptions& options) override;
    std::string AllText() const override;
    std::string AllHtml() const override;
    bool CanPrint() const override;
    bool PrintPreview() override;

    const MessageRenderRequest& Request() const;

private:
    MessageRenderRequest request_;
};

}  // namespace hermes
