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
    HERMES_CHECK(renderer.Load({
        hermes::RendererMode::kPlainText,
        "",
        "mail mailboxes mailing",
        "",
        false,
        true,
    }));

    const hermes::FindResult result = renderer.Find("mail", {false, true});
    HERMES_CHECK(result.matched);
    HERMES_CHECK_EQ(result.match_count, static_cast<std::size_t>(1));
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
    HERMES_CHECK(surface.Load({"hello world", "<p>hello world</p>", false}));
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
    HERMES_CHECK(surface.Load({"hello world", "<div>hello world</div>", false}));
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
