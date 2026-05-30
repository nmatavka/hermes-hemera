#if !HERMES_IS_HAIKU
#error "Haiku native Paige smoke target should only be built on Haiku."
#endif

#if !HERMES_ENABLE_NATIVE_PAIGE
#error "Haiku native Paige smoke target requires HERMES_ENABLE_NATIVE_PAIGE."
#endif

#if !PAIGE_HAIKU_SOURCE_SET
#error "Haiku native Paige smoke target must compile against the Haiku Paige source set."
#endif

#ifndef HAIKU_COMPILE
#error "Haiku native Paige smoke target must inherit Haiku Paige compile definitions."
#endif

#include "hermes/PaigeRichTextSurface.h"
#include "hermes/PaigeRuntime.h"

int main() {
    hermes::PaigeRuntime runtime;
    std::string error_message;
    runtime.Initialize(&error_message);

    hermes::PaigeRichTextSurface surface(runtime);
    hermes::RichTextDocument document;
    document.plain_text = "Native Paige build smoke";
    document.html_fragment = "<p>Native Paige build smoke</p>";
    document.styled_source = hermes::StyledDocumentSource::kHtml;
    surface.Load(document);
    surface.ResizeNativeHost(640.0f, 480.0f);
    (void)surface.NativeBackendEnabled();
    (void)surface.NativeDocumentHandle();
    return 0;
}
