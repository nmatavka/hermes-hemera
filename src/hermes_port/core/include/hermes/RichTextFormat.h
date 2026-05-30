#pragma once

#include <string>
#include <string_view>

#include "hermes/RichTextSurface.h"

namespace hermes {

bool HasAuthenticStyledContent(const RichTextDocument& document);
bool HasAnyStyledPayload(const RichTextDocument& document);
bool RequiresHtmlSurface(const RichTextDocument& document);
std::string ToString(StyledDocumentSource source);
std::string ToString(StyledDocumentFidelity fidelity);
StyledDocumentSource ParseStyledDocumentSource(std::string_view value);
StyledDocumentFidelity ParseStyledDocumentFidelity(std::string_view value);

std::string StripHtml(std::string_view html);
std::string StripRtf(std::string_view rtf);
std::string PlainTextToHtml(std::string_view plain_text);
std::string PlainTextToRtf(std::string_view plain_text);

bool LooksLikeHtmlDocument(std::string_view value);
bool LooksLikeRtfDocument(std::string_view value);

RichTextDocument NormalizeRichTextDocument(const RichTextDocument& document);
RichTextDocument PrepareRichTextDocumentForPersistence(const RichTextDocument& document);
RichTextDocument MergeRichTextDocuments(const RichTextDocument& base,
                                       const RichTextDocument& addition,
                                       std::string_view separator);

StyledDocumentSource BestAvailableSource(const RichTextDocument& document);
StyledDocumentFidelity ClassifyStyledDocument(const RichTextDocument& document);

}  // namespace hermes
