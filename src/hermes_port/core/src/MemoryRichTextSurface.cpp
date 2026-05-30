#include "hermes/MemoryRichTextSurface.h"

#include <algorithm>

namespace hermes {

namespace {

std::string CopySelectedText(const RichTextDocument& document, const TextSelection& selection) {
    if (selection.start > document.plain_text.size()) {
        return {};
    }

    const std::size_t max_length = document.plain_text.size() - selection.start;
    return document.plain_text.substr(selection.start, std::min(selection.length, max_length));
}

}  // namespace

bool MemoryRichTextSurface::Load(const RichTextDocument& document) {
    document_ = NormalizeRichTextDocument(document);
    if (RequiresHtmlSurface(document_)) {
        document_.read_only = true;
    }
    selection_ = {};
    undo_stack_.clear();
    redo_stack_.clear();
    diagnostics_.clear();
    return true;
}

RichTextDocument MemoryRichTextSurface::Snapshot() const {
    return document_;
}

bool MemoryRichTextSurface::SetSelection(const TextSelection& selection) {
    if (selection.start > document_.plain_text.size()) {
        return false;
    }

    const std::size_t max_length = document_.plain_text.size() - selection.start;
    selection_.start = selection.start;
    selection_.length = std::min(selection.length, max_length);
    return true;
}

TextSelection MemoryRichTextSurface::Selection() const {
    return selection_;
}

bool MemoryRichTextSurface::ReplaceSelection(std::string_view replacement) {
    if (document_.read_only || selection_.start > document_.plain_text.size()) {
        return false;
    }

    PushUndoState();
    redo_stack_.clear();
    document_.plain_text.replace(selection_.start, selection_.length, replacement);
    selection_.length = replacement.size();
    SyncStyledBody();
    return true;
}

bool MemoryRichTextSurface::Undo() {
    if (undo_stack_.empty()) {
        return false;
    }

    redo_stack_.push_back(document_);
    document_ = undo_stack_.back();
    undo_stack_.pop_back();
    selection_ = {};
    return true;
}

bool MemoryRichTextSurface::Redo() {
    if (redo_stack_.empty()) {
        return false;
    }

    undo_stack_.push_back(document_);
    document_ = redo_stack_.back();
    redo_stack_.pop_back();
    selection_ = {};
    return true;
}

bool MemoryRichTextSurface::SelectAll() {
    selection_ = {0, document_.plain_text.size()};
    return true;
}

std::string MemoryRichTextSurface::CopySelection() const {
    return CopySelectedText(document_, selection_);
}

std::string MemoryRichTextSurface::CutSelection() {
    if (document_.read_only) {
        return {};
    }

    clipboard_ = CopySelection();
    clipboard_document_ = {};
    clipboard_document_.plain_text = clipboard_;
    clipboard_document_.html_fragment = PlainTextToHtml(clipboard_);
    clipboard_document_.rtf_fragment = PlainTextToRtf(clipboard_);
    if (!clipboard_.empty()) {
        (void)ReplaceSelection("");
    }
    return clipboard_;
}

bool MemoryRichTextSurface::Paste(std::string_view text) {
    const std::string replacement = text.empty() ? clipboard_ : std::string(text);
    if (replacement.empty()) {
        return false;
    }
    clipboard_ = replacement;
    if (!text.empty()) {
        clipboard_document_ = {};
        clipboard_document_.plain_text = replacement;
        clipboard_document_.html_fragment = PlainTextToHtml(replacement);
        clipboard_document_.rtf_fragment = PlainTextToRtf(replacement);
    }
    return ReplaceSelection(replacement);
}

void MemoryRichTextSurface::SetDiagnostics(std::vector<TextDiagnostic> diagnostics) {
    diagnostics_ = std::move(diagnostics);
}

void MemoryRichTextSurface::ClearDiagnostics() {
    diagnostics_.clear();
}

const std::vector<TextDiagnostic>& MemoryRichTextSurface::Diagnostics() const {
    return diagnostics_;
}

bool MemoryRichTextSurface::RevealSelection(const TextSelection& selection) {
    return SetSelection(selection);
}

void MemoryRichTextSurface::PushUndoState() {
    undo_stack_.push_back(document_);
}

void MemoryRichTextSurface::SyncStyledBody() {
    const bool had_structured_content = HasAuthenticStyledContent(document_);
    document_.paige_native_bytes.clear();
    if (had_structured_content) {
        document_.html_fragment = PlainTextToHtml(document_.plain_text);
        document_.rtf_fragment = PlainTextToRtf(document_.plain_text);
        if (document_.styled_source == StyledDocumentSource::kPaigeNative) {
            document_.styled_source = StyledDocumentSource::kHtml;
        }
        document_.fidelity = StyledDocumentFidelity::kLossy;
    } else {
        document_.html_fragment.clear();
        document_.rtf_fragment.clear();
        document_.styled_source = StyledDocumentSource::kPlainText;
        document_.fidelity = StyledDocumentFidelity::kLossless;
    }
}

}  // namespace hermes
