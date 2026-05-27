#include "hermes/PaigeRichTextSurface.h"

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

std::string EscapeHtml(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 32);

    for (char ch : text) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            case '\n':
                escaped += "<br/>\n";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

}  // namespace

PaigeRichTextSurface::PaigeRichTextSurface(PaigeRuntime& runtime) : runtime_(runtime) {}

bool PaigeRichTextSurface::Load(const RichTextDocument& document) {
    document_ = document;
    selection_ = {};
    undo_stack_.clear();
    redo_stack_.clear();
    diagnostics_.clear();
    return true;
}

RichTextDocument PaigeRichTextSurface::Snapshot() const {
    return document_;
}

bool PaigeRichTextSurface::SetSelection(const TextSelection& selection) {
    if (selection.start > document_.plain_text.size()) {
        return false;
    }

    const std::size_t max_length = document_.plain_text.size() - selection.start;
    selection_.start = selection.start;
    selection_.length = std::min(selection.length, max_length);
    return true;
}

TextSelection PaigeRichTextSurface::Selection() const {
    return selection_;
}

bool PaigeRichTextSurface::ReplaceSelection(std::string_view replacement) {
    if (document_.read_only || selection_.start > document_.plain_text.size()) {
        return false;
    }

    undo_stack_.push_back(document_);
    redo_stack_.clear();
    document_.plain_text.replace(selection_.start, selection_.length, replacement);
    selection_.length = replacement.size();
    SyncStyledBody();
    return true;
}

bool PaigeRichTextSurface::Undo() {
    if (undo_stack_.empty()) {
        return false;
    }

    redo_stack_.push_back(document_);
    document_ = undo_stack_.back();
    undo_stack_.pop_back();
    selection_ = {};
    return true;
}

bool PaigeRichTextSurface::Redo() {
    if (redo_stack_.empty()) {
        return false;
    }

    undo_stack_.push_back(document_);
    document_ = redo_stack_.back();
    redo_stack_.pop_back();
    selection_ = {};
    return true;
}

bool PaigeRichTextSurface::SelectAll() {
    selection_ = {0, document_.plain_text.size()};
    return true;
}

std::string PaigeRichTextSurface::CopySelection() const {
    return CopySelectedText(document_, selection_);
}

std::string PaigeRichTextSurface::CutSelection() {
    if (document_.read_only) {
        return {};
    }

    clipboard_ = CopySelection();
    if (!clipboard_.empty()) {
        (void)ReplaceSelection("");
    }
    return clipboard_;
}

bool PaigeRichTextSurface::Paste(std::string_view text) {
    const std::string replacement = text.empty() ? clipboard_ : std::string(text);
    if (replacement.empty()) {
        return false;
    }
    clipboard_ = replacement;
    return ReplaceSelection(replacement);
}

void PaigeRichTextSurface::SetDiagnostics(std::vector<TextDiagnostic> diagnostics) {
    diagnostics_ = std::move(diagnostics);
}

void PaigeRichTextSurface::ClearDiagnostics() {
    diagnostics_.clear();
}

const std::vector<TextDiagnostic>& PaigeRichTextSurface::Diagnostics() const {
    return diagnostics_;
}

bool PaigeRichTextSurface::RevealSelection(const TextSelection& selection) {
    return SetSelection(selection);
}

bool PaigeRichTextSurface::IsAvailable() const {
    return runtime_.IsAvailable();
}

void PaigeRichTextSurface::SyncStyledBody() {
    if (document_.html_fragment.empty()) {
        return;
    }

    document_.html_fragment = "<div>" + EscapeHtml(document_.plain_text) + "</div>";
}

}  // namespace hermes
