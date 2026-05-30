#pragma once

#include <string>
#include <vector>

#include "hermes/RichTextFormat.h"
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
    bool SelectAll() override;
    std::string CopySelection() const override;
    std::string CutSelection() override;
    bool Paste(std::string_view text = {}) override;
    void SetDiagnostics(std::vector<TextDiagnostic> diagnostics) override;
    void ClearDiagnostics() override;
    const std::vector<TextDiagnostic>& Diagnostics() const override;
    bool RevealSelection(const TextSelection& selection) override;

private:
    void PushUndoState();
    void SyncStyledBody();

    RichTextDocument document_;
    TextSelection selection_;
    std::vector<RichTextDocument> undo_stack_;
    std::vector<RichTextDocument> redo_stack_;
    std::string clipboard_;
    RichTextDocument clipboard_document_;
    std::vector<TextDiagnostic> diagnostics_;
};

}  // namespace hermes
