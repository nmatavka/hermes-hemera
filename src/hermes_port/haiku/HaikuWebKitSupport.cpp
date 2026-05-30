#include "HaikuWebKitSupport.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

#include <Entry.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <String.h>
#include <WebFrame.h>
#include <WebPage.h>
#include <WebSettings.h>
#include <WebView.h>
#include <JavaScriptCore/JavaScript.h>

#include "hermes/RichTextFormat.h"

namespace hemera::haiku {

namespace {

constexpr const char* kComposeRootId = "hemera-compose-root";
constexpr const char* kMessageRootId = "hemera-message-root";

std::string BStringToStd(const BString& value) {
    return value.String() != nullptr ? value.String() : "";
}

std::string EscapeHtmlAttribute(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
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
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

std::string EscapeHtmlText(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
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
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

std::string EscapeJsString(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '\'':
                escaped += "\\'";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

std::string SanitizePathComponent(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }
    if (sanitized.empty()) {
        sanitized = "item";
    }
    return sanitized;
}

std::string FileUrlFromPath(const std::filesystem::path& path) {
    auto percent_encode = [](std::string_view input) {
        std::ostringstream out;
        for (unsigned char ch : input) {
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '/' ||
                ch == '-' || ch == '_' || ch == '.' || ch == '~') {
                out << static_cast<char>(ch);
            } else {
                out << '%' << "0123456789ABCDEF"[ch >> 4] << "0123456789ABCDEF"[ch & 0x0F];
            }
        }
        return out.str();
    };

    const std::string generic = std::filesystem::absolute(path).generic_string();
    return "file://" + percent_encode(generic);
}

bool EnsureEmptyDirectory(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
    error.clear();
    std::filesystem::create_directories(path, error);
    return !error;
}

bool WriteWholeFile(const std::filesystem::path& path, std::string_view contents) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

std::string ReplaceAll(std::string value, std::string_view needle, std::string_view replacement) {
    if (needle.empty()) {
        return value;
    }
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
    return value;
}

std::string ReaderStyles() {
    return R"CSS(
body {
  margin: 0;
  background: #f5f2e8;
  color: #201a10;
  font: 15px/1.45 "Noto Serif", "DejaVu Serif", serif;
}
#hermes-message-root {
  max-width: 52rem;
  margin: 0 auto;
  padding: 18px 20px 28px;
  overflow-wrap: anywhere;
}
blockquote {
  margin: 0.75rem 0 0.75rem 1rem;
  padding-left: 0.85rem;
  border-left: 3px solid #c8bca5;
  color: #554735;
}
pre, code {
  font-family: "Noto Sans Mono", "DejaVu Sans Mono", monospace;
  white-space: pre-wrap;
}
img {
  max-width: 100%;
  height: auto;
}
a {
  color: #0d5f7a;
}
table {
  border-collapse: collapse;
}
td, th {
  border: 1px solid #d8d0c0;
  padding: 0.35rem 0.5rem;
}
)CSS";
}

std::string ComposeStyles() {
    return R"CSS(
html, body {
  margin: 0;
  padding: 0;
  min-height: 100%;
  background: #fffdf8;
  color: #1e1a14;
  font: 15px/1.45 "Noto Serif", "DejaVu Serif", serif;
}
body {
  padding: 12px 16px 48px;
}
#hermes-compose-root {
  min-height: calc(100vh - 28px);
  outline: none;
  white-space: normal;
  overflow-wrap: anywhere;
}
#hermes-compose-root:focus {
  outline: none;
}
blockquote {
  margin: 0.75rem 0 0.75rem 1rem;
  padding-left: 0.85rem;
  border-left: 3px solid #d9cdb6;
}
pre, code {
  font-family: "Noto Sans Mono", "DejaVu Sans Mono", monospace;
  white-space: pre-wrap;
}
img {
  max-width: 100%;
  height: auto;
}
)CSS";
}

std::string ComposeBridgeScript() {
    return std::string(R"JS(
(function() {
  function root() {
    return document.getElementById(')JS") +
           kComposeRootId + R"JS(');
  }

  function textNodes(container) {
    const nodes = [];
    if (!container)
      return nodes;
    const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT, null);
    while (walker.nextNode())
      nodes.push(walker.currentNode);
    if (!nodes.length) {
      const fallback = document.createTextNode('');
      container.appendChild(fallback);
      nodes.push(fallback);
    }
    return nodes;
  }

  function totalTextLength(container) {
    return textNodes(container).reduce((sum, node) => sum + node.nodeValue.length, 0);
  }

  function offsetToPosition(container, offset) {
    const nodes = textNodes(container);
    let remaining = Math.max(0, offset);
    for (const node of nodes) {
      if (remaining <= node.nodeValue.length)
        return { node, offset: remaining };
      remaining -= node.nodeValue.length;
    }
    const last = nodes[nodes.length - 1];
    return { node: last, offset: last.nodeValue.length };
  }

  function positionToOffset(container, node, offset) {
    const nodes = textNodes(container);
    let total = 0;
    for (const current of nodes) {
      if (current === node)
        return total + offset;
      total += current.nodeValue.length;
    }
    return total;
  }

  window.HemeraCompose = {
    getHtml: function() {
      const container = root();
      return container ? container.innerHTML : '';
    },
    getText: function() {
      const container = root();
      return container ? container.textContent || '' : '';
    },
    getSelectedText: function() {
      const selection = window.getSelection();
      return selection ? selection.toString() : '';
    },
    getSelection: function() {
      const container = root();
      const selection = window.getSelection();
      if (!container || !selection || !selection.rangeCount)
        return '0:0';
      const range = selection.getRangeAt(0);
      if (!container.contains(range.startContainer) || !container.contains(range.endContainer))
        return '0:0';
      const start = positionToOffset(container, range.startContainer, range.startOffset);
      const end = positionToOffset(container, range.endContainer, range.endOffset);
      return start + ':' + Math.max(0, end - start);
    },
    setSelection: function(start, length) {
      const container = root();
      if (!container)
        return false;
      const begin = offsetToPosition(container, start);
      const end = offsetToPosition(container, start + length);
      const range = document.createRange();
      range.setStart(begin.node, begin.offset);
      range.setEnd(end.node, end.offset);
      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      return true;
    },
    revealSelection: function(start, length) {
      if (!this.setSelection(start, length))
        return false;
      const selection = window.getSelection();
      if (selection && selection.rangeCount) {
        const node = selection.getRangeAt(0).startContainer;
        if (node && node.parentElement)
          node.parentElement.scrollIntoView({ block: 'nearest' });
      }
      return true;
    },
    replaceSelection: function(text) {
      const container = root();
      const selection = window.getSelection();
      if (!container || !selection || !selection.rangeCount)
        return false;
      const range = selection.getRangeAt(0);
      if (!container.contains(range.startContainer) || !container.contains(range.endContainer))
        return false;
      range.deleteContents();
      const node = document.createTextNode(text);
      range.insertNode(node);
      range.setStartAfter(node);
      range.collapse(true);
      selection.removeAllRanges();
      selection.addRange(range);
      return true;
    },
    commandState: function(command) {
      try {
        return (document.queryCommandEnabled(command) ? '1' : '0') + ':' +
               (document.queryCommandState(command) ? '1' : '0') + ':' +
               (document.queryCommandIndeterm(command) ? '1' : '0');
      } catch (error) {
        return '0:0:0';
      }
    },
    commandValue: function(command) {
      try {
        const value = document.queryCommandValue(command);
        return value === undefined || value === null ? '' : String(value);
      } catch (error) {
        return '';
      }
    },
    execCommand: function(command, value) {
      const container = root();
      if (!container)
        return false;
      this.focusRoot();
      try {
        document.execCommand('styleWithCSS', false, true);
      } catch (error) {
      }
      try {
        return document.execCommand(command, false, value === undefined ? null : value);
      } catch (error) {
        return false;
      }
    },
    insertHtml: function(html) {
      return this.execCommand('insertHTML', html);
    },
    wrapSelection: function(columns) {
      const selection = this.getSelectedText();
      if (!selection)
        return false;
      const width = Math.max(1, columns || 72);
      const wrapped = selection.split(/\r?\n/).map(function(line) {
        let remaining = line;
        const output = [];
        while (remaining.length > width) {
          let breakAt = remaining.lastIndexOf(' ', width);
          if (breakAt <= 0)
            breakAt = width;
          output.push(remaining.slice(0, breakAt));
          remaining = remaining.slice(breakAt).replace(/^\s+/, '');
        }
        output.push(remaining);
        return output.join('\n');
      }).join('\n');
      return this.replaceSelection(wrapped);
    },
    selectAll: function() {
      const container = root();
      if (!container)
        return false;
      return this.setSelection(0, totalTextLength(container));
    },
    focusRoot: function() {
      const container = root();
      if (container)
        container.focus();
    }
  };

  window.addEventListener('load', function() {
    window.HemeraCompose.focusRoot();
  });
})();
)JS";
}

std::string BuildReaderDocument(std::string_view body_html, std::string_view base_uri) {
    std::ostringstream output;
    output << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    if (!base_uri.empty()) {
        output << "<base href=\"" << EscapeHtmlAttribute(base_uri) << "\">";
    }
    output << "<style>" << ReaderStyles() << "</style></head><body><div id=\"" << kMessageRootId << "\">"
           << body_html << "</div></body></html>";
    return output.str();
}

std::string BuildComposeDocument(std::string_view body_html) {
    std::ostringstream output;
    output << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" << ComposeStyles()
           << "</style><script>" << ComposeBridgeScript() << "</script></head><body><div id=\"" << kComposeRootId
           << "\" contenteditable=\"true\" spellcheck=\"true\">" << body_html << "</div></body></html>";
    return output.str();
}

std::optional<std::string> EvaluateScript(BWebView* web_view, std::string_view script) {
    if (web_view == nullptr || web_view->WebPage() == nullptr || web_view->WebPage()->MainFrame() == nullptr) {
        return std::nullopt;
    }

    JSGlobalContextRef context = web_view->WebPage()->MainFrame()->GlobalContext();
    if (context == nullptr) {
        return std::nullopt;
    }

    JSStringRef source = JSStringCreateWithUTF8CString(std::string(script).c_str());
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(context, source, nullptr, nullptr, 0, &exception);
    JSStringRelease(source);
    if (exception != nullptr || value == nullptr) {
        return std::nullopt;
    }

    JSStringRef string_ref = JSValueToStringCopy(context, value, &exception);
    if (exception != nullptr || string_ref == nullptr) {
        return std::nullopt;
    }

    const size_t maximum = JSStringGetMaximumUTF8CStringSize(string_ref);
    std::string result(maximum, '\0');
    const size_t written = JSStringGetUTF8CString(string_ref, result.data(), maximum);
    JSStringRelease(string_ref);
    if (written == 0) {
        return std::string();
    }
    result.resize(written - 1);
    return result;
}

bool EvaluateBooleanScript(BWebView* web_view, std::string_view script) {
    const auto result = EvaluateScript(web_view, script);
    return result && (*result == "true" || *result == "1");
}

std::string MaterializeInlineAssets(const hermes::MessageRenderRequest& request,
                                    const std::filesystem::path& render_directory) {
    std::string html = request.html_body;
    if (html.empty()) {
        return html;
    }

    const auto asset_directory = render_directory / "assets";
    std::error_code error;
    std::filesystem::create_directories(asset_directory, error);
    for (std::size_t index = 0; index < request.attachments.size(); ++index) {
        const auto& attachment = request.attachments[index];
        if (attachment.content_id.empty() || attachment.payload_path.empty() || !std::filesystem::exists(attachment.payload_path)) {
            continue;
        }
        std::filesystem::path destination =
            asset_directory /
            (std::to_string(index) + "-" +
             SanitizePathComponent(attachment.name.empty() ? attachment.content_id : attachment.name));
        std::filesystem::copy_file(attachment.payload_path,
                                   destination,
                                   std::filesystem::copy_options::overwrite_existing,
                                   error);
        if (error) {
            error.clear();
            continue;
        }
        html = ReplaceAll(html, "cid:" + attachment.content_id, FileUrlFromPath(destination));
        html = ReplaceAll(html, "CID:" + attachment.content_id, FileUrlFromPath(destination));
    }
    return html;
}

bool ShouldTreatAsStyled(const hermes::MessageRenderRequest& request) {
    return !request.html_body.empty() || !request.rtf_body.empty() || !request.paige_native_body.empty() ||
           request.styled_source != hermes::StyledDocumentSource::kPlainText;
}

std::optional<std::string> StyledHtmlForRequest(const hermes::MessageRenderRequest& request,
                                                const std::filesystem::path& render_directory) {
    hermes::MessageRenderRequest prepared_request = request;
    if (prepared_request.html_body.empty() &&
        (!prepared_request.rtf_body.empty() || !prepared_request.paige_native_body.empty() ||
         prepared_request.styled_source != hermes::StyledDocumentSource::kPlainText)) {
        if (prepared_request.styled_fidelity == hermes::StyledDocumentFidelity::kRequiresHtmlSurface) {
            return std::nullopt;
        }
        hermes::RichTextDocument document;
        document.plain_text = prepared_request.plain_text_body;
        document.rtf_fragment = prepared_request.rtf_body;
        document.paige_native_bytes = prepared_request.paige_native_body;
        document.styled_source = prepared_request.styled_source;
        document.fidelity = prepared_request.styled_fidelity;
        const hermes::RichTextDocument prepared = hermes::PrepareRichTextDocumentForPersistence(document);
        prepared_request.html_body = prepared.html_fragment;
    }

    if (!prepared_request.html_body.empty()) {
        return MaterializeInlineAssets(prepared_request, render_directory);
    }
    if (!prepared_request.rtf_body.empty() || !prepared_request.paige_native_body.empty()) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::pair<std::size_t, std::size_t> ParseSelection(std::string_view encoded) {
    const std::size_t separator = encoded.find(':');
    if (separator == std::string_view::npos) {
        return {0, 0};
    }
    const std::size_t start = static_cast<std::size_t>(std::max<long long>(0, std::atoll(std::string(encoded.substr(0, separator)).c_str())));
    const std::size_t length =
        static_cast<std::size_t>(std::max<long long>(0, std::atoll(std::string(encoded.substr(separator + 1)).c_str())));
    return {start, length};
}

ComposeEditorCommandState ParseCommandState(std::string_view encoded) {
    ComposeEditorCommandState state;
    std::size_t first = encoded.find(':');
    std::size_t second = first == std::string_view::npos ? std::string_view::npos : encoded.find(':', first + 1);
    if (first == std::string_view::npos || second == std::string_view::npos) {
        return state;
    }
    state.enabled = encoded.substr(0, first) == "1";
    state.checked = encoded.substr(first + 1, second - first - 1) == "1";
    state.indeterminate = encoded.substr(second + 1) == "1";
    return state;
}

bool LaunchPath(const std::filesystem::path& path) {
    if (be_roster == nullptr) {
        return false;
    }
    entry_ref ref;
    BEntry entry(path.c_str(), true);
    if (entry.InitCheck() != B_OK || entry.GetRef(&ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
}

std::string EscapeShellSingleQuoted(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

bool SendPathToPrinter(const std::filesystem::path& path) {
    const std::string escaped = EscapeShellSingleQuoted(path.string());
    const std::string command =
        "(command -v lpr >/dev/null 2>&1 && lpr '" + escaped + "') || "
        "(command -v lp >/dev/null 2>&1 && lp '" + escaped + "') >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool HasPrinterCommand() {
    return std::system("(command -v lpr >/dev/null 2>&1) || (command -v lp >/dev/null 2>&1)") == 0;
}

bool LooksLikeHtmlDocument(std::string_view html) {
    const std::string lowered = Lowercase(html);
    return lowered.find("<html") != std::string::npos || lowered.find("<!doctype") != std::string::npos;
}

std::string BuildPreviewDocumentFromHtmlBody(std::string_view title, std::string_view body_html) {
    return "<!doctype html><html><head><meta charset=\"utf-8\"><title>" + EscapeHtmlText(title) +
           "</title><style>body{font-family:sans-serif;margin:24px;line-height:1.5;}"
           "img{max-width:100%;}pre{white-space:pre-wrap;word-break:break-word;}</style></head><body>" +
           std::string(body_html) + "</body></html>";
}

bool MutateFallbackDocument(hermes::RichTextDocument& document,
                            hermes::TextSelection& selection,
                            std::string_view replacement) {
    if (selection.start > document.plain_text.size()) {
        return false;
    }
    const std::size_t safe_length = std::min(selection.length, document.plain_text.size() - selection.start);
    document.plain_text.replace(selection.start, safe_length, replacement);
    selection = {selection.start + replacement.size(), 0};
    document.html_fragment = hermes::PlainTextToHtml(document.plain_text);
    document.rtf_fragment = hermes::PlainTextToRtf(document.plain_text);
    document.paige_native_bytes.clear();
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    document.fidelity = hermes::ClassifyStyledDocument(document);
    return true;
}

class ComposeWebView final : public BWebView {
public:
    ComposeWebView() : BWebView("compose-web-view") {}

    void SetEditCallback(std::function<void()> callback) {
        callback_ = std::move(callback);
    }

    void SetTabNavigationCallback(std::function<bool(bool shift)> callback) {
        tab_navigation_callback_ = std::move(callback);
    }

    void SetSelectionChangeCallback(std::function<void()> callback) {
        selection_callback_ = std::move(callback);
    }

    void KeyDown(const char* bytes, int32 num_bytes) override {
        if (bytes != nullptr && num_bytes > 0 && bytes[0] == B_TAB && tab_navigation_callback_ &&
            tab_navigation_callback_((modifiers() & B_SHIFT_KEY) != 0)) {
            return;
        }
        BWebView::KeyDown(bytes, num_bytes);
        if (selection_callback_ && bytes != nullptr && num_bytes > 0) {
            switch (bytes[0]) {
                case B_LEFT_ARROW:
                case B_RIGHT_ARROW:
                case B_UP_ARROW:
                case B_DOWN_ARROW:
                case B_HOME:
                case B_END:
                case B_PAGE_UP:
                case B_PAGE_DOWN:
                    selection_callback_();
                    break;
                default:
                    break;
            }
        }
        if (callback_ && bytes != nullptr && num_bytes > 0) {
            switch (bytes[0]) {
                case B_LEFT_ARROW:
                case B_RIGHT_ARROW:
                case B_UP_ARROW:
                case B_DOWN_ARROW:
                case B_HOME:
                case B_END:
                case B_PAGE_UP:
                case B_PAGE_DOWN:
                    return;
                default:
                    callback_();
                    return;
            }
        }
    }

    void MouseUp(BPoint where) override {
        BWebView::MouseUp(where);
        if (selection_callback_) {
            selection_callback_();
        }
    }

private:
    std::function<void()> callback_;
    std::function<bool(bool shift)> tab_navigation_callback_;
    std::function<void()> selection_callback_;
};

class MessageWebView final : public BWebView {
public:
    MessageWebView() : BWebView("message-web-view") {}

    void SetContextMenuHandler(std::function<void(BPoint)> handler) {
        handler_ = std::move(handler);
    }

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0 && handler_) {
            handler_(where);
            return;
        }
        BWebView::MouseDown(where);
    }

private:
    std::function<void(BPoint)> handler_;
};

class WebKitComposeEditorHost final : public ComposeEditorHost {
public:
    explicit WebKitComposeEditorHost(HaikuWebKitRichTextSurface& surface)
        : surface_(surface),
          root_(new BGroupView(B_VERTICAL)),
          web_view_(new ComposeWebView()) {
        BLayoutBuilder::Group<>(root_, B_VERTICAL, 0).Add(web_view_);
        surface_.AttachWebView(web_view_);
    }

    ~WebKitComposeEditorHost() override {
        surface_.DetachWebView();
        if (web_view_ != nullptr) {
            if (web_view_->Parent() != nullptr) {
                web_view_->RemoveSelf();
            }
            web_view_->Shutdown();
            web_view_ = nullptr;
        }
    }

    BView* RootView() const override {
        return root_;
    }

    void SetChangeCallback(std::function<void()> callback) override {
        if (web_view_ != nullptr) {
            web_view_->SetEditCallback(std::move(callback));
        }
    }

    void SetSelectionChangeCallback(std::function<void()> callback) override {
        selection_callback_ = std::move(callback);
        if (web_view_ != nullptr) {
            web_view_->SetSelectionChangeCallback(selection_callback_);
        }
    }

    void ReloadFromSurface() override {}

    void ScrollSelectionIntoView() override {
        (void)surface_.RevealSelection(surface_.Selection());
    }

    void SetTabNavigationCallback(std::function<bool(bool shift)> callback) override {
        if (web_view_ != nullptr) {
            web_view_->SetTabNavigationCallback(std::move(callback));
        }
    }

    bool SelectAllText() override {
        return surface_.SelectAll();
    }

    bool CopySelection() override {
        return !surface_.CopySelection().empty();
    }

    bool CutSelection() override {
        return !surface_.CutSelection().empty();
    }

    bool Paste() override {
        return surface_.Paste();
    }

    bool SupportsCommand(ComposeEditorCommand command) const override {
        switch (command) {
            case ComposeEditorCommand::kPlain:
            case ComposeEditorCommand::kBold:
            case ComposeEditorCommand::kItalic:
            case ComposeEditorCommand::kUnderline:
            case ComposeEditorCommand::kStrikeout:
            case ComposeEditorCommand::kFixedWidth:
            case ComposeEditorCommand::kAddQuote:
            case ComposeEditorCommand::kRemoveQuote:
            case ComposeEditorCommand::kIndentIn:
            case ComposeEditorCommand::kIndentOut:
            case ComposeEditorCommand::kNormalMargins:
            case ComposeEditorCommand::kAlignLeft:
            case ComposeEditorCommand::kAlignCenter:
            case ComposeEditorCommand::kAlignRight:
            case ComposeEditorCommand::kBulletedList:
            case ComposeEditorCommand::kInsertLink:
            case ComposeEditorCommand::kClearFormatting:
            case ComposeEditorCommand::kTextColor:
            case ComposeEditorCommand::kTextSize:
            case ComposeEditorCommand::kInsertDownloadablePicture:
            case ComposeEditorCommand::kInsertHorizontalRule:
            case ComposeEditorCommand::kWrapSelection:
                return true;
            case ComposeEditorCommand::kFormatPainter:
                return false;
        }
        return false;
    }

    ComposeEditorCommandState CommandState(ComposeEditorCommand command) const override {
        ComposeEditorCommandState state;
        state.enabled = SupportsCommand(command);
        if (web_view_ == nullptr || !state.enabled) {
            return state;
        }
        const char* dom_command = DomCommandName(command);
        if (dom_command == nullptr) {
            return state;
        }
        if (const auto result = EvaluateScript(
                web_view_,
                "window.HemeraCompose && window.HemeraCompose.commandState('" +
                    std::string(dom_command) + "');")) {
            return ParseCommandState(*result);
        }
        return state;
    }

    bool ExecuteCommand(ComposeEditorCommand command) override {
        if (web_view_ == nullptr || !SupportsCommand(command)) {
            return false;
        }

        switch (command) {
            case ComposeEditorCommand::kAddQuote:
                return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('indent');");
            case ComposeEditorCommand::kRemoveQuote:
            case ComposeEditorCommand::kNormalMargins:
                return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('outdent');");
            case ComposeEditorCommand::kFixedWidth:
                return RunScriptCommand(
                    "window.HemeraCompose && window.HemeraCompose.execCommand('fontName','monospace');");
            case ComposeEditorCommand::kPlain:
                return RunScriptCommand(
                    "window.HemeraCompose && window.HemeraCompose.execCommand('removeFormat');");
            case ComposeEditorCommand::kInsertHorizontalRule:
                return InsertHorizontalRule();
            case ComposeEditorCommand::kWrapSelection:
                return WrapSelection(72);
            case ComposeEditorCommand::kFormatPainter:
                return false;
            default:
                break;
        }

        const char* dom_command = DomCommandName(command);
        if (dom_command == nullptr) {
            return false;
        }
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('" +
                                std::string(dom_command) + "');");
    }

    std::string SelectedText() const override {
        return surface_.CopySelection();
    }

    bool InsertLink(std::string_view url) override {
        if (url.empty()) {
            return false;
        }
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('createLink','" +
                                EscapeJsString(url) + "');");
    }

    bool ApplyTextColor(std::string_view color) override {
        if (color.empty()) {
            return false;
        }
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('foreColor','" +
                                EscapeJsString(color) + "');");
    }

    bool ApplyTextSize(std::string_view size) override {
        if (size.empty()) {
            return false;
        }
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.execCommand('fontSize','" +
                                EscapeJsString(size) + "');");
    }

    bool InsertDownloadablePicture(std::string_view url, std::string_view alt_text) override {
        if (url.empty()) {
            return false;
        }
        const std::string html = "<img src=\"" + EscapeHtmlAttribute(url) + "\" alt=\"" +
                                 EscapeHtmlAttribute(alt_text) + "\" />";
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.insertHtml('" +
                                EscapeJsString(html) + "');");
    }

    bool InsertHorizontalRule() override {
        return RunScriptCommand(
            "window.HemeraCompose && window.HemeraCompose.execCommand('insertHorizontalRule');");
    }

    bool WrapSelection(std::size_t columns) override {
        return RunScriptCommand("window.HemeraCompose && window.HemeraCompose.wrapSelection(" +
                                std::to_string(columns) + ");");
    }

    std::optional<ComposeEditorStyleSnapshot> CaptureStyleSnapshot() const override {
        if (web_view_ == nullptr) {
            return std::nullopt;
        }
        ComposeEditorStyleSnapshot snapshot;
        snapshot.bold = CommandState(ComposeEditorCommand::kBold).checked;
        snapshot.italic = CommandState(ComposeEditorCommand::kItalic).checked;
        snapshot.underline = CommandState(ComposeEditorCommand::kUnderline).checked;
        snapshot.strikeout = CommandState(ComposeEditorCommand::kStrikeout).checked;
        snapshot.fixed_width = CommandState(ComposeEditorCommand::kFixedWidth).checked;
        snapshot.bulleted_list = CommandState(ComposeEditorCommand::kBulletedList).checked;
        snapshot.include_paragraph_style = true;

        if (const auto centered = EvaluateScript(
                web_view_, "window.HemeraCompose && window.HemeraCompose.commandState('justifyCenter');");
            centered && *centered == "1:1:0") {
            snapshot.alignment = ComposeEditorAlignment::kCenter;
        } else if (const auto right = EvaluateScript(
                       web_view_, "window.HemeraCompose && window.HemeraCompose.commandState('justifyRight');");
                   right && *right == "1:1:0") {
            snapshot.alignment = ComposeEditorAlignment::kRight;
        } else {
            snapshot.alignment = ComposeEditorAlignment::kLeft;
        }

        if (const auto color = EvaluateScript(
                web_view_, "window.HemeraCompose && window.HemeraCompose.commandValue('foreColor');");
            color && !color->empty()) {
            snapshot.text_color = *color;
        }
        if (const auto size = EvaluateScript(
                web_view_, "window.HemeraCompose && window.HemeraCompose.commandValue('fontSize');");
            size && !size->empty()) {
            snapshot.text_size = *size;
        }
        return snapshot;
    }

    bool ApplyStyleSnapshot(const ComposeEditorStyleSnapshot& snapshot) override {
        if (web_view_ == nullptr) {
            return false;
        }
        bool applied = false;
        const auto apply_toggle = [this, &applied](ComposeEditorCommand command, bool desired) {
            const auto state = CommandState(command);
            if (state.enabled && state.checked != desired && ExecuteCommand(command)) {
                applied = true;
            }
        };

        apply_toggle(ComposeEditorCommand::kBold, snapshot.bold);
        apply_toggle(ComposeEditorCommand::kItalic, snapshot.italic);
        apply_toggle(ComposeEditorCommand::kUnderline, snapshot.underline);
        apply_toggle(ComposeEditorCommand::kStrikeout, snapshot.strikeout);
        apply_toggle(ComposeEditorCommand::kFixedWidth, snapshot.fixed_width);

        if (snapshot.include_paragraph_style) {
            apply_toggle(ComposeEditorCommand::kBulletedList, snapshot.bulleted_list);
            if (snapshot.alignment.has_value()) {
                switch (*snapshot.alignment) {
                    case ComposeEditorAlignment::kLeft:
                        applied = ExecuteCommand(ComposeEditorCommand::kAlignLeft) || applied;
                        break;
                    case ComposeEditorAlignment::kCenter:
                        applied = ExecuteCommand(ComposeEditorCommand::kAlignCenter) || applied;
                        break;
                    case ComposeEditorAlignment::kRight:
                        applied = ExecuteCommand(ComposeEditorCommand::kAlignRight) || applied;
                        break;
                }
            }
        }
        if (snapshot.text_color.has_value()) {
            applied = ApplyTextColor(*snapshot.text_color) || applied;
        }
        if (snapshot.text_size.has_value()) {
            applied = ApplyTextSize(*snapshot.text_size) || applied;
        }
        return applied;
    }

    void MakeEditorFocus() override {
        if (web_view_ != nullptr) {
            web_view_->MakeFocus(true);
        }
    }

    void InvalidateEditor() override {
        if (root_ != nullptr) {
            root_->Invalidate();
        }
    }

private:
    static const char* DomCommandName(ComposeEditorCommand command) {
        switch (command) {
            case ComposeEditorCommand::kBold:
                return "bold";
            case ComposeEditorCommand::kItalic:
                return "italic";
            case ComposeEditorCommand::kUnderline:
                return "underline";
            case ComposeEditorCommand::kStrikeout:
                return "strikeThrough";
            case ComposeEditorCommand::kIndentIn:
                return "indent";
            case ComposeEditorCommand::kIndentOut:
                return "outdent";
            case ComposeEditorCommand::kAlignLeft:
                return "justifyLeft";
            case ComposeEditorCommand::kAlignCenter:
                return "justifyCenter";
            case ComposeEditorCommand::kAlignRight:
                return "justifyRight";
            case ComposeEditorCommand::kBulletedList:
                return "insertUnorderedList";
            case ComposeEditorCommand::kClearFormatting:
                return "removeFormat";
            default:
                return nullptr;
        }
    }

    bool RunScriptCommand(std::string script) {
        return web_view_ != nullptr && EvaluateBooleanScript(web_view_, script);
    }

    HaikuWebKitRichTextSurface& surface_;
    BGroupView* root_ = nullptr;
    ComposeWebView* web_view_ = nullptr;
    std::function<void()> selection_callback_;
};

}  // namespace

HaikuWebKitRichTextSurface::HaikuWebKitRichTextSurface(hermes::PaigeRuntime& runtime,
                                                       std::filesystem::path cache_root,
                                                       std::string cache_key)
    : runtime_(runtime), cache_root_(std::move(cache_root)), cache_key_(std::move(cache_key)) {}

HaikuWebKitRichTextSurface::~HaikuWebKitRichTextSurface() {
    DetachWebView();
    std::error_code error;
    std::filesystem::remove_all(RenderDirectory(), error);
}

bool HaikuWebKitRichTextSurface::Load(const hermes::RichTextDocument& document) {
    document_ = hermes::PrepareRichTextDocumentForPersistence(document);
    if (document_.html_fragment.empty()) {
        document_.html_fragment = hermes::PlainTextToHtml(document_.plain_text);
    }
    document_.styled_source = hermes::StyledDocumentSource::kHtml;
    selection_ = {};
    return LoadIntoBoundView();
}

hermes::RichTextDocument HaikuWebKitRichTextSurface::Snapshot() const {
    hermes::RichTextDocument snapshot = document_;
    if (web_view_ != nullptr) {
        if (const auto html = EvaluateScript(web_view_, "window.HemeraCompose && window.HemeraCompose.getHtml();")) {
            snapshot.html_fragment = *html;
        }
        if (const auto text = EvaluateScript(web_view_, "window.HemeraCompose && window.HemeraCompose.getText();")) {
            snapshot.plain_text = *text;
        }
    }
    snapshot.read_only = false;
    snapshot.styled_source = hermes::StyledDocumentSource::kHtml;
    snapshot.paige_native_bytes.clear();
    snapshot = hermes::PrepareRichTextDocumentForPersistence(snapshot);
    snapshot.fidelity = hermes::ClassifyStyledDocument(snapshot);
    return snapshot;
}

bool HaikuWebKitRichTextSurface::SetSelection(const hermes::TextSelection& selection) {
    selection_ = selection;
    if (web_view_ == nullptr) {
        return selection.start <= document_.plain_text.size();
    }
    return EvaluateBooleanScript(
        web_view_,
        "window.HemeraCompose && window.HemeraCompose.setSelection(" + std::to_string(selection.start) + "," +
            std::to_string(selection.length) + ");");
}

hermes::TextSelection HaikuWebKitRichTextSurface::Selection() const {
    if (web_view_ != nullptr) {
        if (const auto selection = EvaluateScript(web_view_, "window.HemeraCompose && window.HemeraCompose.getSelection();")) {
            const auto parsed = ParseSelection(*selection);
            return {parsed.first, parsed.second};
        }
    }
    return selection_;
}

bool HaikuWebKitRichTextSurface::ReplaceSelection(std::string_view replacement) {
    if (document_.read_only) {
        return false;
    }
    if (web_view_ != nullptr) {
        const bool replaced = EvaluateBooleanScript(
            web_view_,
            "window.HemeraCompose && window.HemeraCompose.replaceSelection('" + EscapeJsString(replacement) + "');");
        if (!replaced) {
            return false;
        }
        document_ = Snapshot();
        selection_ = Selection();
        return true;
    }
    return MutateFallbackDocument(document_, selection_, replacement);
}

bool HaikuWebKitRichTextSurface::Undo() {
    if (web_view_ == nullptr || web_view_->WebPage() == nullptr || web_view_->WebPage()->MainFrame() == nullptr ||
        !web_view_->WebPage()->MainFrame()->CanUndo()) {
        return false;
    }
    web_view_->WebPage()->MainFrame()->Undo();
    document_ = Snapshot();
    return true;
}

bool HaikuWebKitRichTextSurface::Redo() {
    if (web_view_ == nullptr || web_view_->WebPage() == nullptr || web_view_->WebPage()->MainFrame() == nullptr ||
        !web_view_->WebPage()->MainFrame()->CanRedo()) {
        return false;
    }
    web_view_->WebPage()->MainFrame()->Redo();
    document_ = Snapshot();
    return true;
}

bool HaikuWebKitRichTextSurface::SelectAll() {
    if (web_view_ != nullptr) {
        return EvaluateBooleanScript(web_view_, "window.HemeraCompose && window.HemeraCompose.selectAll();");
    }
    selection_ = {0, document_.plain_text.size()};
    return true;
}

std::string HaikuWebKitRichTextSurface::CopySelection() const {
    std::string copied;
    if (web_view_ != nullptr) {
        if (const auto selected = EvaluateScript(web_view_, "window.HemeraCompose && window.HemeraCompose.getSelectedText();")) {
            copied = *selected;
        }
        if (web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr &&
            web_view_->WebPage()->MainFrame()->CanCopy()) {
            web_view_->WebPage()->MainFrame()->Copy();
        }
        return copied;
    }

    if (selection_.start > document_.plain_text.size()) {
        return {};
    }
    return document_.plain_text.substr(selection_.start,
                                       std::min(selection_.length, document_.plain_text.size() - selection_.start));
}

std::string HaikuWebKitRichTextSurface::CutSelection() {
    if (document_.read_only) {
        return {};
    }
    const std::string copied = CopySelection();
    if (copied.empty()) {
        return {};
    }
    if (web_view_ != nullptr && web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr &&
        web_view_->WebPage()->MainFrame()->CanCut()) {
        web_view_->WebPage()->MainFrame()->Cut();
        document_ = Snapshot();
        return copied;
    }
    return MutateFallbackDocument(document_, selection_, {}) ? copied : std::string();
}

bool HaikuWebKitRichTextSurface::Paste(std::string_view text) {
    if (document_.read_only) {
        return false;
    }
    if (!text.empty()) {
        return ReplaceSelection(text);
    }
    if (web_view_ == nullptr || web_view_->WebPage() == nullptr || web_view_->WebPage()->MainFrame() == nullptr ||
        !web_view_->WebPage()->MainFrame()->CanPaste()) {
        return false;
    }
    web_view_->WebPage()->MainFrame()->Paste();
    document_ = Snapshot();
    return true;
}

void HaikuWebKitRichTextSurface::SetDiagnostics(std::vector<hermes::TextDiagnostic> diagnostics) {
    diagnostics_ = std::move(diagnostics);
}

void HaikuWebKitRichTextSurface::ClearDiagnostics() {
    diagnostics_.clear();
}

const std::vector<hermes::TextDiagnostic>& HaikuWebKitRichTextSurface::Diagnostics() const {
    return diagnostics_;
}

bool HaikuWebKitRichTextSurface::RevealSelection(const hermes::TextSelection& selection) {
    selection_ = selection;
    if (web_view_ == nullptr) {
        return selection.start <= document_.plain_text.size();
    }
    return EvaluateBooleanScript(
        web_view_,
        "window.HemeraCompose && window.HemeraCompose.revealSelection(" + std::to_string(selection.start) + "," +
            std::to_string(selection.length) + ");");
}

void HaikuWebKitRichTextSurface::AttachWebView(BWebView* web_view) {
    web_view_ = web_view;
    (void)LoadIntoBoundView();
}

void HaikuWebKitRichTextSurface::DetachWebView() {
    web_view_ = nullptr;
}

bool HaikuWebKitRichTextSurface::LoadIntoBoundView() {
    if (web_view_ == nullptr) {
        return true;
    }

    const auto render_directory = RenderDirectory();
    if (!EnsureEmptyDirectory(render_directory)) {
        return false;
    }
    const auto document_path = render_directory / "index.html";
    if (!WriteWholeFile(document_path, BuildComposeDocument(document_.html_fragment))) {
        return false;
    }

    if (web_view_->WebPage() != nullptr && web_view_->WebPage()->Settings() != nullptr) {
        web_view_->WebPage()->Settings()->SetJavascriptEnabled(true);
        web_view_->WebPage()->Settings()->Apply();
    }
    web_view_->LoadURL(FileUrlFromPath(document_path).c_str());
    if (web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr) {
        web_view_->WebPage()->MainFrame()->SetEditable(true);
    }
    return true;
}

std::filesystem::path HaikuWebKitRichTextSurface::RenderDirectory() const {
    return cache_root_ / "compose" / SanitizePathComponent(cache_key_);
}

HaikuWebKitMessageView::HaikuWebKitMessageView(std::filesystem::path cache_root)
    : BGroupView(B_VERTICAL), cache_root_(std::move(cache_root)), web_view_(new MessageWebView()) {
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(web_view_);
}

HaikuWebKitMessageView::~HaikuWebKitMessageView() {
    if (web_view_ != nullptr) {
        if (web_view_->Parent() != nullptr) {
            web_view_->RemoveSelf();
        }
        web_view_->Shutdown();
        web_view_ = nullptr;
    }
}

bool HaikuWebKitMessageView::Load(const hermes::MessageRenderRequest& request) {
    request_ = request;
    if (!ShouldTreatAsStyled(request_)) {
        return false;
    }

    const auto render_directory = RenderDirectoryForRequest(request_);
    if (!EnsureEmptyDirectory(render_directory)) {
        return false;
    }

    const auto html_body = StyledHtmlForRequest(request_, render_directory);
    if (!html_body) {
        return false;
    }

    const auto document_path = render_directory / "index.html";
    if (!WriteWholeFile(document_path, BuildReaderDocument(*html_body, request_.source_uri))) {
        return false;
    }

    if (web_view_->WebPage() != nullptr && web_view_->WebPage()->Settings() != nullptr) {
        web_view_->WebPage()->Settings()->SetJavascriptEnabled(false);
        web_view_->WebPage()->Settings()->Apply();
    }
    web_view_->LoadURL(FileUrlFromPath(document_path).c_str());
    if (web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr) {
        web_view_->WebPage()->MainFrame()->SetEditable(false);
    }
    return true;
}

hermes::RendererMode HaikuWebKitMessageView::Mode() const {
    return hermes::RendererMode::kWebKit;
}

hermes::FindResult HaikuWebKitMessageView::Find(std::string_view needle, const hermes::FindOptions& options) {
    if (web_view_ == nullptr || needle.empty()) {
        return {};
    }
    web_view_->FindString(std::string(needle).c_str(), true, options.match_case, true, false);
    return {true, 0};
}

std::string HaikuWebKitMessageView::AllText() const {
    if (web_view_ != nullptr && web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr) {
        return BStringToStd(web_view_->WebPage()->MainFrame()->InnerText());
    }
    if (!request_.plain_text_body.empty()) {
        return request_.plain_text_body;
    }
    if (!request_.html_body.empty()) {
        return hermes::StripHtml(request_.html_body);
    }
    if (!request_.rtf_body.empty()) {
        return hermes::StripRtf(request_.rtf_body);
    }
    return {};
}

std::string HaikuWebKitMessageView::AllHtml() const {
    if (web_view_ != nullptr && web_view_->WebPage() != nullptr && web_view_->WebPage()->MainFrame() != nullptr) {
        return BStringToStd(web_view_->WebPage()->MainFrame()->AsMarkup());
    }
    return request_.html_body;
}

bool HaikuWebKitMessageView::CanPrint() const {
    return !AllText().empty() || !AllHtml().empty();
}

bool HaikuWebKitMessageView::CanDirectPrint() const {
    return CanPrint() && HasPrinterCommand();
}

bool HaikuWebKitMessageView::PrintPreview() {
    std::filesystem::path preview_path;
    std::filesystem::path printable_path;
    if (!EnsurePrintArtifacts(&preview_path, &printable_path)) {
        return false;
    }
    return LaunchPath(preview_path);
}

bool HaikuWebKitMessageView::DirectPrint() {
    if (!CanDirectPrint()) {
        return false;
    }
    std::filesystem::path preview_path;
    std::filesystem::path printable_path;
    if (!EnsurePrintArtifacts(&preview_path, &printable_path)) {
        return false;
    }
    return SendPathToPrinter(printable_path);
}

std::string HaikuWebKitMessageView::SelectedText() const {
    if (const auto selected = EvaluateScript(web_view_, "window.getSelection && window.getSelection().toString();")) {
        return *selected;
    }
    return {};
}

void HaikuWebKitMessageView::SetContextMenuHandler(std::function<void(BPoint)> handler) {
    if (auto* message_view = dynamic_cast<MessageWebView*>(web_view_)) {
        message_view->SetContextMenuHandler(std::move(handler));
    }
}

std::filesystem::path HaikuWebKitMessageView::RenderDirectoryForRequest(const hermes::MessageRenderRequest& request) const {
    return cache_root_ / "messages" / SanitizePathComponent(request.mailbox_id.empty() ? "mailbox" : request.mailbox_id) /
           SanitizePathComponent(request.message_id.empty() ? "message" : request.message_id);
}

bool HaikuWebKitMessageView::EnsurePrintArtifacts(std::filesystem::path* preview_path,
                                                  std::filesystem::path* printable_path) const {
    if (!CanPrint()) {
        return false;
    }
    const auto render_directory = RenderDirectoryForRequest(request_);

    std::string prepared_html = AllHtml();
    if (prepared_html.empty()) {
        const std::string printable_text = AllText();
        if (printable_text.empty()) {
            return false;
        }
        prepared_html = BuildPreviewDocumentFromHtmlBody("Print Preview",
                                                         "<pre>" + EscapeHtmlText(printable_text) + "</pre>");
    } else if (!LooksLikeHtmlDocument(prepared_html)) {
        prepared_html = BuildPreviewDocumentFromHtmlBody("Print Preview", prepared_html);
    }

    std::string printable_text = hermes::StripHtml(prepared_html);
    if (printable_text.empty()) {
        return false;
    }

    const auto print_path = render_directory / "print-message.txt";
    if (!WriteWholeFile(print_path, printable_text)) {
        return false;
    }
    const auto preview_document_path = render_directory / "print-preview.html";
    if (!WriteWholeFile(preview_document_path, prepared_html)) {
        return false;
    }

    if (preview_path != nullptr) {
        *preview_path = preview_document_path;
    }
    if (printable_path != nullptr) {
        *printable_path = print_path;
    }
    return true;
}

std::unique_ptr<ComposeEditorHost> CreateWebKitComposeEditorHost(HaikuWebKitRichTextSurface& surface) {
    return std::make_unique<WebKitComposeEditorHost>(surface);
}

}  // namespace hemera::haiku
