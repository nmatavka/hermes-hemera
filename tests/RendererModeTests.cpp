#include "TestRegistry.h"

#include "hermes/BufferedMessageRenderer.h"
#include "hermes/MemoryRichTextSurface.h"
#include "hermes/PaigeRichTextSurface.h"
#include "hermes/RendererMode.h"

HERMES_TEST(RendererModeRoundTrip) {
    const auto parsed = hermes::ParseRendererMode("WebKit");
    HERMES_CHECK(parsed.has_value());
    HERMES_CHECK_EQ(hermes::ToString(*parsed), std::string("webkit"));
}

HERMES_TEST(BufferedMessageRendererFindsWholeWords) {
    hermes::BufferedMessageRenderer renderer;
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kPlainText;
    request.plain_text_body = "mail mailboxes mailing";
    request.styled_source = hermes::StyledDocumentSource::kPlainText;
    request.styled_fidelity = hermes::StyledDocumentFidelity::kLossless;
    request.read_only = true;
    HERMES_CHECK(renderer.Load(request));

    const hermes::FindResult result = renderer.Find("mail", {false, true});
    HERMES_CHECK(result.matched);
    HERMES_CHECK_EQ(result.match_count, static_cast<std::size_t>(1));
}

HERMES_TEST(BufferedMessageRendererProjectsSupportedStyledBodiesToHtml) {
    hermes::BufferedMessageRenderer renderer;
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kWebKit;
    request.plain_text_body = "Rich body";
    request.rtf_body = "{\\rtf1\\ansi Rich body}";
    request.styled_source = hermes::StyledDocumentSource::kRtf;
    request.styled_fidelity = hermes::StyledDocumentFidelity::kLossless;
    request.read_only = true;
    HERMES_CHECK(renderer.Load(request));

    const std::string html = renderer.AllHtml();
    HERMES_CHECK(!html.empty());
    HERMES_CHECK(html.find("Rich body") != std::string::npos);
}

HERMES_TEST(BufferedMessageRendererDoesNotProjectRequiresHtmlSurfaceBodiesLossily) {
    hermes::BufferedMessageRenderer renderer;
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kWebKit;
    request.plain_text_body = "fallback";
    request.rtf_body = "{\\rtf1\\ansi\\object lossy}";
    request.styled_source = hermes::StyledDocumentSource::kRtf;
    request.styled_fidelity = hermes::StyledDocumentFidelity::kRequiresHtmlSurface;
    request.read_only = true;
    HERMES_CHECK(renderer.Load(request));

    HERMES_CHECK(renderer.AllHtml().empty());
}

HERMES_TEST(BufferedMessageRendererTracksPreviewVersusDirectPrintSeparately) {
    hermes::BufferedMessageRenderer renderer;
    hermes::MessageRenderRequest request;
    request.plain_text_body = "Printable body";
    HERMES_CHECK(renderer.Load(request));

    HERMES_CHECK(renderer.PrintPreview());
    HERMES_CHECK(renderer.LastPrintOperation() ==
                 hermes::BufferedMessageRenderer::PrintOperation::kPreview);

    HERMES_CHECK(renderer.DirectPrint());
    HERMES_CHECK(renderer.LastPrintOperation() ==
                 hermes::BufferedMessageRenderer::PrintOperation::kDirect);
}

HERMES_TEST(MemoryRichTextSurfaceUndoRedo) {
    hermes::MemoryRichTextSurface surface;
    HERMES_CHECK(surface.Load({"hello world", "", false}));
    HERMES_CHECK(surface.SetSelection({6, 5}));
    HERMES_CHECK(surface.ReplaceSelection("haiku"));
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello haiku"));
    HERMES_CHECK(surface.Undo());
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello world"));
    HERMES_CHECK(surface.Redo());
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello haiku"));
}

HERMES_TEST(PaigeRichTextSurfaceProvidesFallbackEditingWhenNativeRuntimeIsUnavailable) {
    hermes::PaigeRuntime runtime;
    hermes::PaigeRichTextSurface surface(runtime);
    hermes::RichTextDocument document;
    document.plain_text = "hello world";
    document.html_fragment = "<p>hello world</p>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    HERMES_CHECK(surface.Load(document));
    HERMES_CHECK(surface.SetSelection({6, 5}));
    HERMES_CHECK(surface.ReplaceSelection("haiku"));
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello haiku"));
    HERMES_CHECK(surface.Undo());
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello world"));
    HERMES_CHECK(surface.Redo());
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello haiku"));
    HERMES_CHECK(!surface.IsAvailable());
}

HERMES_TEST(PaigeRichTextSurfaceSupportsClipboardAndDiagnosticsFallback) {
    hermes::PaigeRuntime runtime;
    hermes::PaigeRichTextSurface surface(runtime);
    hermes::RichTextDocument document;
    document.plain_text = "hello world";
    document.html_fragment = "<div>hello world</div>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    HERMES_CHECK(surface.Load(document));
    HERMES_CHECK(surface.SetSelection({0, 5}));
    HERMES_CHECK_EQ(surface.CopySelection(), std::string("hello"));
    HERMES_CHECK_EQ(surface.CutSelection(), std::string("hello"));
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string(" world"));
    HERMES_CHECK(surface.SetSelection({0, 0}));
    HERMES_CHECK(surface.Paste());
    HERMES_CHECK_EQ(surface.Snapshot().plain_text, std::string("hello world"));

    surface.SetDiagnostics({{
        hermes::TextDiagnosticKind::kSpell,
        hermes::TextDiagnosticSeverity::kWarning,
        6,
        5,
        "Possible misspelling",
    }});
    HERMES_CHECK_EQ(surface.Diagnostics().size(), static_cast<std::size_t>(1));
    surface.ClearDiagnostics();
    HERMES_CHECK(surface.Diagnostics().empty());
}

HERMES_TEST(PaigeRuntimeReportsNoNativeStateOnFallbackHost) {
    hermes::PaigeRuntime runtime;
    HERMES_CHECK_EQ(runtime.NativeGlobals(), nullptr);
    HERMES_CHECK_EQ(runtime.NativeMemoryGlobals(), nullptr);
}

HERMES_TEST(PaigeRichTextSurfaceKeepsNativePathGuardedWhenRuntimeIsUnavailable) {
    hermes::PaigeRuntime runtime;
    hermes::PaigeRichTextSurface surface(runtime);
    hermes::RichTextDocument document;
    document.plain_text = "compose body";
    document.html_fragment = "<p>compose body</p>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    HERMES_CHECK(surface.Load(document));
    HERMES_CHECK(!surface.NativeBackendEnabled());
    HERMES_CHECK_EQ(surface.NativeDocumentHandle(), nullptr);
}
