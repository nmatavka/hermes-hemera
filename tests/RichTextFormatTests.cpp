#include "TestRegistry.h"

#include "hermes/MemoryRichTextSurface.h"
#include "hermes/RichTextFormat.h"

HERMES_TEST(RichTextFormatKeepsPlainSourceWhenOnlyPersistenceHelpersExist) {
    hermes::RichTextDocument document;
    document.plain_text = "Plain text only";
    document.html_fragment = hermes::PlainTextToHtml(document.plain_text);
    document.rtf_fragment = hermes::PlainTextToRtf(document.plain_text);

    const hermes::RichTextDocument normalized = hermes::NormalizeRichTextDocument(document);
    HERMES_CHECK_EQ(normalized.styled_source, hermes::StyledDocumentSource::kPlainText);
    HERMES_CHECK_EQ(normalized.fidelity, hermes::StyledDocumentFidelity::kLossless);
    HERMES_CHECK(!hermes::HasAuthenticStyledContent(normalized));
}

HERMES_TEST(RichTextFormatClassifiesUnsupportedHtmlAndRtfAsHtmlSurfaceOnly) {
    hermes::RichTextDocument html_document;
    html_document.html_fragment = "<div>Hello</div><script>alert('x')</script>";
    html_document.styled_source = hermes::StyledDocumentSource::kHtml;

    const hermes::RichTextDocument normalized_html = hermes::NormalizeRichTextDocument(html_document);
    HERMES_CHECK_EQ(normalized_html.fidelity, hermes::StyledDocumentFidelity::kRequiresHtmlSurface);
    HERMES_CHECK(hermes::RequiresHtmlSurface(normalized_html));

    hermes::RichTextDocument rtf_document;
    rtf_document.rtf_fragment = "{\\rtf1\\ansi\\object\\objdata payload}";
    rtf_document.styled_source = hermes::StyledDocumentSource::kRtf;

    const hermes::RichTextDocument normalized_rtf = hermes::NormalizeRichTextDocument(rtf_document);
    HERMES_CHECK_EQ(normalized_rtf.fidelity, hermes::StyledDocumentFidelity::kRequiresHtmlSurface);
    HERMES_CHECK(hermes::RequiresHtmlSurface(normalized_rtf));
}

HERMES_TEST(MemoryRichTextSurfacePreservesReadOnlyRoutingForHtmlSurfaceDocuments) {
    hermes::MemoryRichTextSurface surface;
    hermes::RichTextDocument document;
    document.html_fragment = "<div>Hello</div><iframe src=\"https://example.com\"></iframe>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;

    HERMES_CHECK(surface.Load(document));
    HERMES_CHECK(surface.Snapshot().read_only);
    HERMES_CHECK(surface.SetSelection({0, 0}));
    HERMES_CHECK(!surface.ReplaceSelection("Nope"));
}

HERMES_TEST(RichTextFormatMergeDoesNotPromotePlainDocumentsWithHelperRepresentations) {
    hermes::RichTextDocument left;
    left.plain_text = "One";
    left.html_fragment = hermes::PlainTextToHtml(left.plain_text);
    left.rtf_fragment = hermes::PlainTextToRtf(left.plain_text);

    hermes::RichTextDocument right;
    right.plain_text = "Two";
    right.html_fragment = hermes::PlainTextToHtml(right.plain_text);
    right.rtf_fragment = hermes::PlainTextToRtf(right.plain_text);

    const hermes::RichTextDocument merged = hermes::MergeRichTextDocuments(left, right, "\n");
    HERMES_CHECK_EQ(merged.styled_source, hermes::StyledDocumentSource::kPlainText);
    HERMES_CHECK(merged.html_fragment.empty());
    HERMES_CHECK(merged.rtf_fragment.empty());
    HERMES_CHECK_EQ(merged.plain_text, std::string("One\nTwo"));
}
