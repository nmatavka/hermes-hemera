#pragma once

#include "hermes/PaigeRuntime.h"
#include "hermes/RichTextSurface.h"

namespace hermes {

class PaigeRichTextSurface final : public RichTextSurface {
public:
    explicit PaigeRichTextSurface(PaigeRuntime& runtime);

    bool Load(const RichTextDocument& document) override;
    RichTextDocument Snapshot() const override;
    bool SetSelection(const TextSelection& selection) override;
    TextSelection Selection() const override;
    bool ReplaceSelection(std::string_view replacement) override;
    bool Undo() override;
    bool Redo() override;

    bool IsAvailable() const;

private:
    PaigeRuntime& runtime_;
    RichTextDocument document_;
    TextSelection selection_;
};

}  // namespace hermes
