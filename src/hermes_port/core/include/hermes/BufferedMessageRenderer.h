#pragma once

#include "hermes/MessageRenderer.h"

namespace hermes {

class BufferedMessageRenderer final : public MessageRenderer {
public:
    enum class PrintOperation {
        kNone = 0,
        kPreview,
        kDirect,
    };

    bool Load(const MessageRenderRequest& request) override;
    RendererMode Mode() const override;
    FindResult Find(std::string_view needle, const FindOptions& options) override;
    std::string AllText() const override;
    std::string AllHtml() const override;
    bool CanPrint() const override;
    bool CanDirectPrint() const override;
    bool PrintPreview() override;
    bool DirectPrint() override;

    const MessageRenderRequest& Request() const;
    PrintOperation LastPrintOperation() const;

private:
    MessageRenderRequest request_;
    PrintOperation last_print_operation_ = PrintOperation::kNone;
};

}  // namespace hermes
