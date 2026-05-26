#include "TestRegistry.h"

#include "hermes/BufferedMessageRenderer.h"
#include "hermes/MemoryRichTextSurface.h"
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
