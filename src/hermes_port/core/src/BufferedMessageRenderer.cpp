#include "hermes/BufferedMessageRenderer.h"

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

std::string StripTags(std::string_view html) {
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

}  // namespace

bool BufferedMessageRenderer::Load(const MessageRenderRequest& request) {
    request_ = request;
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
        return StripTags(request_.html_body);
    }
    return {};
}

std::string BufferedMessageRenderer::AllHtml() const {
    return request_.html_body;
}

bool BufferedMessageRenderer::CanPrint() const {
    return !request_.plain_text_body.empty() || !request_.html_body.empty();
}

bool BufferedMessageRenderer::PrintPreview() {
    return CanPrint();
}

const MessageRenderRequest& BufferedMessageRenderer::Request() const {
    return request_;
}

}  // namespace hermes
