#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/SignatureStore.h"

HERMES_TEST(FilesystemSignatureStoreDiscoversPlainAndHtmlSignatures) {
    hermes::FilesystemSignatureStore store;
    std::string error_message;
    HERMES_CHECK(store.Discover(hermes::tests::FixtureRoot() / "compose" / "signatures", &error_message));

    const auto signatures = store.Templates();
    HERMES_CHECK_EQ(signatures.size(), static_cast<std::size_t>(2));

    const auto standard = store.Find("Standard");
    HERMES_CHECK(static_cast<bool>(standard));
    HERMES_CHECK(standard->body.plain_text.find("Nick Example") != std::string::npos);
    HERMES_CHECK(standard->body.html_fragment.empty());
    HERMES_CHECK_EQ(standard->body.styled_source, hermes::StyledDocumentSource::kPlainText);

    const auto alternate = store.Find("Alternate");
    HERMES_CHECK(static_cast<bool>(alternate));
    HERMES_CHECK(!alternate->body.html_fragment.empty());
    HERMES_CHECK(alternate->body.plain_text.find("Status Desk") != std::string::npos);
    HERMES_CHECK_EQ(alternate->body.styled_source, hermes::StyledDocumentSource::kHtml);
}

HERMES_TEST(FilesystemSignatureStoreSupportsCrudOperations) {
    hermes::tests::ScopedTempDirectory temp("hemera-signatures");

    hermes::FilesystemSignatureStore store;
    std::string error_message;
    HERMES_CHECK(store.Discover(temp.Path(), &error_message));

    hermes::SignatureTemplate created;
    created.name = "Ops";
    created.body.plain_text = "Regards,\nOps";
    HERMES_CHECK(store.SaveTemplate(created, &error_message));
    HERMES_CHECK(static_cast<bool>(store.Find("ops")));

    created.body.html_fragment = "<p>Regards,<br>Ops</p>";
    created.body.styled_source = hermes::StyledDocumentSource::kHtml;
    HERMES_CHECK(store.SaveTemplate(created, &error_message));
    const auto updated = store.Find("Ops");
    HERMES_CHECK(static_cast<bool>(updated));
    HERMES_CHECK(!updated->body.html_fragment.empty());

    HERMES_CHECK(store.DeleteTemplate("Ops", &error_message));
    HERMES_CHECK(!store.Find("Ops"));
}

HERMES_TEST(FilesystemSignatureStorePersistsRtfAndNativeRepresentations) {
    hermes::tests::ScopedTempDirectory temp("hemera-signatures-rtf");

    hermes::FilesystemSignatureStore store;
    std::string error_message;
    HERMES_CHECK(store.Discover(temp.Path(), &error_message));

    hermes::SignatureTemplate created;
    created.name = "Styled";
    created.body.plain_text = "Regards,\nStyled";
    created.body.rtf_fragment = "{\\rtf1\\ansi Regards,\\par Styled}";
    created.body.paige_native_bytes = "native-signature";
    created.body.styled_source = hermes::StyledDocumentSource::kRtf;
    HERMES_CHECK(store.SaveTemplate(created, &error_message));

    const auto loaded = store.Find("Styled");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->body.styled_source, hermes::StyledDocumentSource::kRtf);
    HERMES_CHECK_EQ(loaded->body.rtf_fragment, std::string("{\\rtf1\\ansi Regards,\\par Styled}"));
    HERMES_CHECK_EQ(loaded->body.paige_native_bytes, std::string("native-signature"));
}
