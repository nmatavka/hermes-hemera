#include "hermes/BufferedMessageRenderer.h"
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

bool IsWordBoundary(char ch) {
    return !std::isalnum(static_cast<unsigned char>(ch)) && ch != '_';
}

}  // namespace

bool BufferedMessageRenderer::Load(const MessageRenderRequest& request) {
    request_ = request;
    last_print_operation_ = PrintOperation::kNone;
    return true;
}

RendererMode BufferedMessageRenderer::Mode() const {
    return request_.preferred_mode;
}

FindResult BufferedMessageRenderer::Find(std::string_view needle, const FindOptions& options) {
    if (needle.empty()) {
        return {};
    }

    std::string haystack = AllText();
    std::string token(needle);
    if (!options.match_case) {
        haystack = ToLower(haystack);
        token = ToLower(token);
    }

    FindResult result;
    std::size_t pos = 0;
    while ((pos = haystack.find(token, pos)) != std::string::npos) {
        const bool left_ok = (pos == 0) || IsWordBoundary(haystack[pos - 1]);
        const std::size_t right_index = pos + token.size();
        const bool right_ok = (right_index >= haystack.size()) || IsWordBoundary(haystack[right_index]);

        if (!options.whole_word || (left_ok && right_ok)) {
            result.matched = true;
            ++result.match_count;
        }

        pos += token.size();
    }

    return result;
}

std::string BufferedMessageRenderer::AllText() const {
    if (!request_.plain_text_body.empty()) {
        return request_.plain_text_body;
    }
    if (!request_.html_body.empty()) {
        return StripHtml(request_.html_body);
    }
    if (!request_.rtf_body.empty()) {
        return StripRtf(request_.rtf_body);
    }
    return {};
}

std::string BufferedMessageRenderer::AllHtml() const {
    if (!request_.html_body.empty()) {
        return request_.html_body;
    }
    if (!request_.rtf_body.empty() || !request_.paige_native_body.empty() ||
        request_.styled_source != StyledDocumentSource::kPlainText) {
        if (request_.styled_fidelity == StyledDocumentFidelity::kRequiresHtmlSurface) {
            return {};
        }
        RichTextDocument document;
        document.plain_text = request_.plain_text_body;
        document.rtf_fragment = request_.rtf_body;
        document.paige_native_bytes = request_.paige_native_body;
        document.styled_source = request_.styled_source;
        document.fidelity = request_.styled_fidelity;
        const RichTextDocument prepared = PrepareRichTextDocumentForPersistence(document);
        return prepared.html_fragment;
    }
    if (!request_.plain_text_body.empty()) {
        return PlainTextToHtml(request_.plain_text_body);
    }
    return {};
}

bool BufferedMessageRenderer::CanPrint() const {
    return !request_.plain_text_body.empty() || !request_.html_body.empty() || !request_.rtf_body.empty();
}

bool BufferedMessageRenderer::CanDirectPrint() const {
    return CanPrint();
}

bool BufferedMessageRenderer::PrintPreview() {
    if (!CanPrint()) {
        return false;
    }
    last_print_operation_ = PrintOperation::kPreview;
    return true;
}

bool BufferedMessageRenderer::DirectPrint() {
    if (!CanDirectPrint()) {
        return false;
    }
    last_print_operation_ = PrintOperation::kDirect;
    return true;
}

const MessageRenderRequest& BufferedMessageRenderer::Request() const {
    return request_;
}

BufferedMessageRenderer::PrintOperation BufferedMessageRenderer::LastPrintOperation() const {
    return last_print_operation_;
}

}  // namespace hermes
