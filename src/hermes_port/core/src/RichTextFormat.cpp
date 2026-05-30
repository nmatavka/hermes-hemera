#include "hermes/RichTextFormat.h"

#include <algorithm>
#include <cctype>

namespace hermes {

namespace {

std::string ToLower(std::string_view value) {
    std::string lowered(value);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

bool ContainsUnsupportedHtmlConstructs(std::string_view html) {
    const std::string lowered = ToLower(html);
    static constexpr std::string_view kUnsupported[] = {
        "<script",
        "<style",
        "<svg",
        "<canvas",
        "<video",
        "<audio",
        "<iframe",
        "<object",
        "<embed",
        "<form",
        "<input",
        "<textarea",
        "<select",
        "<button",
        "<math",
    };
    for (const auto tag : kUnsupported) {
        if (lowered.find(tag) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ContainsUnsupportedRtfConstructs(std::string_view rtf) {
    const std::string lowered = ToLower(rtf);
    return lowered.find("\\object") != std::string::npos || lowered.find("\\objdata") != std::string::npos;
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

std::string EscapeRtf(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 32);
    for (char ch : text) {
        switch (ch) {
            case '\\':
            case '{':
            case '}':
                escaped.push_back('\\');
                escaped.push_back(ch);
                break;
            case '\n':
                escaped += "\\par\n";
                break;
            case '\r':
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

StyledDocumentSource MergeSource(const RichTextDocument& base, const RichTextDocument& addition) {
    if (base.styled_source == StyledDocumentSource::kPaigeNative ||
        addition.styled_source == StyledDocumentSource::kPaigeNative) {
        return StyledDocumentSource::kHtml;
    }
    if (base.styled_source == StyledDocumentSource::kHtml || addition.styled_source == StyledDocumentSource::kHtml) {
        return StyledDocumentSource::kHtml;
    }
    if (base.styled_source == StyledDocumentSource::kRtf || addition.styled_source == StyledDocumentSource::kRtf) {
        return StyledDocumentSource::kRtf;
    }
    return StyledDocumentSource::kPlainText;
}

StyledDocumentFidelity MergeFidelity(const RichTextDocument& base, const RichTextDocument& addition) {
    if (base.fidelity == StyledDocumentFidelity::kRequiresHtmlSurface ||
        addition.fidelity == StyledDocumentFidelity::kRequiresHtmlSurface) {
        return StyledDocumentFidelity::kRequiresHtmlSurface;
    }
    if (!base.paige_native_bytes.empty() || !addition.paige_native_bytes.empty() ||
        base.fidelity == StyledDocumentFidelity::kLossy || addition.fidelity == StyledDocumentFidelity::kLossy) {
        return StyledDocumentFidelity::kLossy;
    }
    return StyledDocumentFidelity::kLossless;
}

std::string JoinFragments(std::string left, std::string_view separator, const std::string& right) {
    if (right.empty()) {
        return left;
    }
    if (!left.empty()) {
        left.append(separator.data(), separator.size());
    }
    left += right;
    return left;
}

}  // namespace

bool HasAuthenticStyledContent(const RichTextDocument& document) {
    return document.styled_source != StyledDocumentSource::kPlainText || !document.paige_native_bytes.empty();
}

bool HasAnyStyledPayload(const RichTextDocument& document) {
    return !document.html_fragment.empty() || !document.rtf_fragment.empty() || !document.paige_native_bytes.empty();
}

bool RequiresHtmlSurface(const RichTextDocument& document) {
    return document.fidelity == StyledDocumentFidelity::kRequiresHtmlSurface;
}

std::string ToString(StyledDocumentSource source) {
    switch (source) {
        case StyledDocumentSource::kPlainText:
            return "plain";
        case StyledDocumentSource::kHtml:
            return "html";
        case StyledDocumentSource::kRtf:
            return "rtf";
        case StyledDocumentSource::kPaigeNative:
            return "paige-native";
    }
    return "plain";
}

std::string ToString(StyledDocumentFidelity fidelity) {
    switch (fidelity) {
        case StyledDocumentFidelity::kLossless:
            return "lossless";
        case StyledDocumentFidelity::kLossy:
            return "lossy";
        case StyledDocumentFidelity::kRequiresHtmlSurface:
            return "requires-html-surface";
    }
    return "lossless";
}

StyledDocumentSource ParseStyledDocumentSource(std::string_view value) {
    const std::string lowered = ToLower(value);
    if (lowered == "html") {
        return StyledDocumentSource::kHtml;
    }
    if (lowered == "rtf") {
        return StyledDocumentSource::kRtf;
    }
    if (lowered == "paige-native" || lowered == "paigenative" || lowered == "native") {
        return StyledDocumentSource::kPaigeNative;
    }
    return StyledDocumentSource::kPlainText;
}

StyledDocumentFidelity ParseStyledDocumentFidelity(std::string_view value) {
    const std::string lowered = ToLower(value);
    if (lowered == "lossy") {
        return StyledDocumentFidelity::kLossy;
    }
    if (lowered == "requires-html-surface" || lowered == "requireshtmlsurface") {
        return StyledDocumentFidelity::kRequiresHtmlSurface;
    }
    return StyledDocumentFidelity::kLossless;
}

std::string StripHtml(std::string_view html) {
    std::string result;
    result.reserve(html.size());
    bool inside_tag = false;
    for (char ch : html) {
        if (ch == '<') {
            inside_tag = true;
            continue;
        }
        if (ch == '>') {
            inside_tag = false;
            continue;
        }
        if (!inside_tag) {
            result.push_back(ch);
        }
    }
    return result;
}

std::string StripRtf(std::string_view rtf) {
    std::string result;
    result.reserve(rtf.size());
    bool in_control = false;
    bool skipping_binary = false;
    for (std::size_t index = 0; index < rtf.size(); ++index) {
        const char ch = rtf[index];
        if (skipping_binary) {
            if (ch == ' ' || ch == '\n' || ch == '\r') {
                skipping_binary = false;
            }
            continue;
        }
        if (in_control) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                in_control = false;
            } else if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '*') {
                in_control = false;
            }
            continue;
        }
        if (ch == '\\') {
            in_control = true;
            if (index + 4 < rtf.size() && rtf.substr(index, 5) == "\\par ") {
                result.push_back('\n');
            }
            if (index + 4 < rtf.size() && rtf.substr(index, 5) == "\\bin ") {
                skipping_binary = true;
            }
            continue;
        }
        if (ch == '{' || ch == '}') {
            continue;
        }
        result.push_back(ch);
    }
    return result;
}

std::string PlainTextToHtml(std::string_view plain_text) {
    return "<div>" + EscapeHtml(plain_text) + "</div>";
}

std::string PlainTextToRtf(std::string_view plain_text) {
    return "{\\rtf1\\ansi\n" + EscapeRtf(plain_text) + "\n}";
}

bool LooksLikeHtmlDocument(std::string_view value) {
    const std::string lowered = ToLower(value);
    return lowered.find("<html") != std::string::npos || lowered.find("<body") != std::string::npos ||
           lowered.find("<div") != std::string::npos || lowered.find("<p") != std::string::npos ||
           lowered.find("<span") != std::string::npos;
}

bool LooksLikeRtfDocument(std::string_view value) {
    const std::string lowered = ToLower(value);
    return lowered.find("{\\rtf") != std::string::npos;
}

StyledDocumentSource BestAvailableSource(const RichTextDocument& document) {
    if (!document.paige_native_bytes.empty()) {
        return StyledDocumentSource::kPaigeNative;
    }
    if (!document.html_fragment.empty()) {
        return StyledDocumentSource::kHtml;
    }
    if (!document.rtf_fragment.empty()) {
        return StyledDocumentSource::kRtf;
    }
    return StyledDocumentSource::kPlainText;
}

StyledDocumentFidelity ClassifyStyledDocument(const RichTextDocument& document) {
    switch (document.styled_source) {
        case StyledDocumentSource::kPlainText:
            return StyledDocumentFidelity::kLossless;
        case StyledDocumentSource::kHtml:
            return ContainsUnsupportedHtmlConstructs(document.html_fragment)
                       ? StyledDocumentFidelity::kRequiresHtmlSurface
                       : StyledDocumentFidelity::kLossless;
        case StyledDocumentSource::kRtf:
            return ContainsUnsupportedRtfConstructs(document.rtf_fragment)
                       ? StyledDocumentFidelity::kRequiresHtmlSurface
                       : StyledDocumentFidelity::kLossless;
        case StyledDocumentSource::kPaigeNative:
            return StyledDocumentFidelity::kLossless;
    }
    return StyledDocumentFidelity::kLossless;
}

RichTextDocument NormalizeRichTextDocument(const RichTextDocument& document) {
    RichTextDocument normalized = document;
    if (normalized.styled_source == StyledDocumentSource::kPlainText && normalized.plain_text.empty() &&
        !normalized.paige_native_bytes.empty()) {
        normalized.styled_source = BestAvailableSource(normalized);
    }
    if (normalized.plain_text.empty()) {
        if (!normalized.html_fragment.empty()) {
            normalized.plain_text = StripHtml(normalized.html_fragment);
        } else if (!normalized.rtf_fragment.empty()) {
            normalized.plain_text = StripRtf(normalized.rtf_fragment);
        }
    }
    normalized.fidelity = normalized.fidelity == StyledDocumentFidelity::kRequiresHtmlSurface
                              ? normalized.fidelity
                              : ClassifyStyledDocument(normalized);
    return normalized;
}

RichTextDocument PrepareRichTextDocumentForPersistence(const RichTextDocument& document) {
    RichTextDocument prepared = NormalizeRichTextDocument(document);
    if (prepared.html_fragment.empty()) {
        prepared.html_fragment = PlainTextToHtml(prepared.plain_text);
    }
    if (prepared.rtf_fragment.empty()) {
        prepared.rtf_fragment = PlainTextToRtf(prepared.plain_text);
    }
    return prepared;
}

RichTextDocument MergeRichTextDocuments(const RichTextDocument& base,
                                       const RichTextDocument& addition,
                                       std::string_view separator) {
    const RichTextDocument left = NormalizeRichTextDocument(base);
    const RichTextDocument right = NormalizeRichTextDocument(addition);

    RichTextDocument merged;
    merged.plain_text = JoinFragments(left.plain_text, separator, right.plain_text);
    merged.read_only = left.read_only;
    merged.styled_source = MergeSource(left, right);
    merged.fidelity = MergeFidelity(left, right);

    const bool needs_structured = HasAuthenticStyledContent(left) || HasAuthenticStyledContent(right);
    if (needs_structured) {
        const std::string left_html =
            left.html_fragment.empty() ? PlainTextToHtml(left.plain_text) : left.html_fragment;
        const std::string right_html =
            right.html_fragment.empty() ? PlainTextToHtml(right.plain_text) : right.html_fragment;
        merged.html_fragment = JoinFragments(left_html, separator, right_html);
    }

    if (needs_structured) {
        const std::string left_rtf = left.rtf_fragment.empty() ? PlainTextToRtf(left.plain_text) : left.rtf_fragment;
        const std::string right_rtf =
            right.rtf_fragment.empty() ? PlainTextToRtf(right.plain_text) : right.rtf_fragment;
        merged.rtf_fragment = JoinFragments(left_rtf, separator, right_rtf);
    }

    return merged;
}

}  // namespace hermes
