#if !HERMES_IS_HAIKU
#error "Haiku WebKit smoke target should only be built on Haiku."
#endif

#if !HERMES_HAS_HAIKU_WEBKIT
#error "Haiku WebKit smoke target requires the legacy WebKit system package."
#endif

#include <Application.h>

#include <filesystem>
#include <string>

#include "HaikuWebKitSupport.h"
#include "hermes/PaigeRuntime.h"

int main() {
    BApplication app("application/x-vnd.hemera-webkit-smoke");

    const auto cache_root = std::filesystem::temp_directory_path() / "hemera-haiku-webkit-smoke";

    hemera::haiku::HaikuWebKitMessageView message_view(cache_root);
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kWebKit;
    request.mailbox_id = "smoke-mailbox";
    request.message_id = "smoke-message";
    request.html_body = "<p>Haiku WebKit smoke</p>";
    request.plain_text_body = "Haiku WebKit smoke";
    request.styled_source = hermes::StyledDocumentSource::kHtml;
    request.styled_fidelity = hermes::StyledDocumentFidelity::kLossless;
    request.allow_remote_content = true;
    request.read_only = true;
    if (!message_view.Load(request)) {
        return 1;
    }

    hermes::PaigeRuntime runtime;
    std::string error_message;
    runtime.Initialize(&error_message);

    hemera::haiku::HaikuWebKitRichTextSurface surface(runtime, cache_root, "compose");
    hermes::RichTextDocument document;
    document.plain_text = "Haiku WebKit compose smoke";
    document.html_fragment = "<p>Haiku WebKit compose smoke</p>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    document.fidelity = hermes::StyledDocumentFidelity::kLossless;
    if (!surface.Load(document)) {
        return 1;
    }

    auto editor_host = hemera::haiku::CreateWebKitComposeEditorHost(surface);
    if (editor_host == nullptr || editor_host->RootView() == nullptr) {
        return 1;
    }
    editor_host->ReloadFromSurface();
    editor_host->MakeEditorFocus();
    return 0;
}
