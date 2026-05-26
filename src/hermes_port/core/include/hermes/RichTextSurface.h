#pragma once

#include <cstddef>
#include <string>

namespace hermes {

struct RichTextDocument {
    std::string plain_text;
    std::string html_fragment;
    bool read_only = false;
};

struct TextSelection {
    std::size_t start = 0;
    std::size_t length = 0;
};

class RichTextSurface {
public:
    virtual ~RichTextSurface() = default;

    virtual bool Load(const RichTextDocument& document) = 0;
    virtual RichTextDocument Snapshot() const = 0;
    virtual bool SetSelection(const TextSelection& selection) = 0;
    virtual TextSelection Selection() const = 0;
    virtual bool ReplaceSelection(std::string_view replacement) = 0;
    virtual bool Undo() = 0;
    virtual bool Redo() = 0;
};

}  // namespace hermes
