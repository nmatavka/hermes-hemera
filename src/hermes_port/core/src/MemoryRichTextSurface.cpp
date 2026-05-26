#include "hermes/MemoryRichTextSurface.h"

#include <algorithm>

namespace hermes {

bool MemoryRichTextSurface::Load(const RichTextDocument& document) {
    document_ = document;
    selection_ = {};
    undo_stack_.clear();
    redo_stack_.clear();
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

void MemoryRichTextSurface::PushUndoState() {
    undo_stack_.push_back(document_);
}

}  // namespace hermes
