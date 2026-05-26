#pragma once

#include <vector>

#include "hermes/RichTextSurface.h"

namespace hermes {

class MemoryRichTextSurface final : public RichTextSurface {
public:
    bool Load(const RichTextDocument& document) override;
    RichTextDocument Snapshot() const override;
    bool SetSelection(const TextSelection& selection) override;
    TextSelection Selection() const override;
    bool ReplaceSelection(std::string_view replacement) override;
    bool Undo() override;
    bool Redo() override;

private:
    void PushUndoState();

    RichTextDocument document_;
    TextSelection selection_;
    std::vector<RichTextDocument> undo_stack_;
    std::vector<RichTextDocument> redo_stack_;
};

}  // namespace hermes
