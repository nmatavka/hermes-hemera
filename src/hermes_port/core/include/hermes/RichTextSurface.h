#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class StyledDocumentSource {
    kPlainText,
    kHtml,
    kRtf,
    kPaigeNative,
};

enum class StyledDocumentFidelity {
    kLossless,
    kLossy,
    kRequiresHtmlSurface,
};

struct RichTextDocument {
    std::string plain_text;
    std::string html_fragment;
    bool read_only = false;
    std::string rtf_fragment;
    std::string paige_native_bytes;
    StyledDocumentSource styled_source = StyledDocumentSource::kPlainText;
    StyledDocumentFidelity fidelity = StyledDocumentFidelity::kLossless;
};

struct TextSelection {
    std::size_t start = 0;
    std::size_t length = 0;
};

enum class TextDiagnosticKind {
    kSpell,
    kMoodWatch,
    kStyledContent,
};

enum class TextDiagnosticSeverity {
    kInfo,
    kWarning,
    kError,
};

struct TextDiagnostic {
    TextDiagnosticKind kind = TextDiagnosticKind::kSpell;
    TextDiagnosticSeverity severity = TextDiagnosticSeverity::kInfo;
    std::size_t start = 0;
    std::size_t length = 0;
    std::string message;
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
    virtual bool SelectAll() = 0;
    virtual std::string CopySelection() const = 0;
    virtual std::string CutSelection() = 0;
    virtual bool Paste(std::string_view text = {}) = 0;
    virtual void SetDiagnostics(std::vector<TextDiagnostic> diagnostics) = 0;
    virtual void ClearDiagnostics() = 0;
    virtual const std::vector<TextDiagnostic>& Diagnostics() const = 0;
    virtual bool RevealSelection(const TextSelection& selection) = 0;
};

}  // namespace hermes
