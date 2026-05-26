#include "hermes/PaigeRichTextSurface.h"

#include <algorithm>

namespace hermes {

PaigeRichTextSurface::PaigeRichTextSurface(PaigeRuntime& runtime) : runtime_(runtime) {}

bool PaigeRichTextSurface::Load(const RichTextDocument& document) {
    document_ = document;
    selection_ = {};
    return runtime_.IsAvailable();
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
    return runtime_.IsAvailable();
}

TextSelection PaigeRichTextSurface::Selection() const {
    return selection_;
}

bool PaigeRichTextSurface::ReplaceSelection(std::string_view replacement) {
    if (document_.read_only || selection_.start > document_.plain_text.size()) {
        return false;
    }

    document_.plain_text.replace(selection_.start, selection_.length, replacement);
    selection_.length = replacement.size();
    return runtime_.IsAvailable();
}

bool PaigeRichTextSurface::Undo() {
    return false;
}

bool PaigeRichTextSurface::Redo() {
    return false;
}

bool PaigeRichTextSurface::IsAvailable() const {
    return runtime_.IsAvailable();
}

}  // namespace hermes
