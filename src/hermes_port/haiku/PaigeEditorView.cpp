#include "PaigeEditorView.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>

#include <Application.h>
#include <Clipboard.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <ScrollView.h>
#include <ScrollBar.h>
#include <String.h>

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
#include "Paige.h"
#include "PGHTEXT.H"
#endif

namespace hemera::haiku {

namespace {

constexpr const char* kClipboardMime = "text/plain";

bool UsesNativeSurface(const hermes::PaigeRichTextSurface& surface) {
    return surface.NativeBackendEnabled() && surface.NativeDocumentHandle() != nullptr;
}

std::string WrapPlainText(std::string_view text, std::size_t columns) {
    if (columns == 0) {
        return std::string(text);
    }

    std::stringstream input{std::string(text)};
    std::ostringstream output;
    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        if (!first_line) {
            output << '\n';
        }
        first_line = false;

        std::size_t start = 0;
        while (start < line.size()) {
            std::size_t remaining = line.size() - start;
            if (remaining <= columns) {
                output << line.substr(start);
                break;
            }
            std::size_t break_at = line.rfind(' ', start + columns);
            if (break_at == std::string::npos || break_at < start) {
                break_at = start + columns;
            }
            output << line.substr(start, break_at - start);
            output << '\n';
            start = break_at;
            while (start < line.size() && line[start] == ' ') {
                ++start;
            }
        }
    }
    return output.str();
}

std::string QuoteSelectionText(std::string_view text, bool remove_quote) {
    std::stringstream input{std::string(text)};
    std::ostringstream output;
    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        if (!first_line) {
            output << '\n';
        }
        first_line = false;
        if (remove_quote) {
            if (line.rfind("> ", 0) == 0) {
                output << line.substr(2);
            } else if (!line.empty() && line.front() == '>') {
                output << line.substr(1);
            } else {
                output << line;
            }
        } else {
            output << "> " << line;
        }
    }
    if (!remove_quote && text.empty()) {
        return "> ";
    }
    return output.str();
}

std::string Lowercase(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

BRect LineRect(float inset, float line_height, std::size_t index, float width) {
    const float top = inset + static_cast<float>(index) * line_height;
    return BRect(inset, top, width - inset, top + line_height);
}

rgb_color DiagnosticColor(hermes::TextDiagnosticKind kind) {
    switch (kind) {
        case hermes::TextDiagnosticKind::kSpell:
            return rgb_color{220, 20, 60, 255};
        case hermes::TextDiagnosticKind::kMoodWatch:
            return rgb_color{255, 140, 0, 255};
        case hermes::TextDiagnosticKind::kStyledContent:
            return rgb_color{30, 144, 255, 255};
    }
    return rgb_color{0, 0, 0, 255};
}

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
constexpr pg_fixed kDefaultListIndent = 36;
constexpr std::size_t kMaxNativeHyperlinkLength = 2048;

pg_ref NativeDocument(const hermes::PaigeRichTextSurface& surface) {
    return static_cast<pg_ref>(surface.NativeDocumentHandle());
}

bool IsEditableHyperlinkType(long type) {
    return (type & HYPERLINK_EUDORA_ATTACHMENT) == 0 &&
           (type & HYPERLINK_EUDORA_PLUGIN) == 0 &&
           (type & HYPERLINK_EUDORA_AUTOURL) == 0;
}

bool NativeLinkSelection(pg_ref document, select_pair* selection, pg_hyperlink* hyperlink) {
    if (document == nullptr || selection == nullptr) {
        return false;
    }
    pgGetSelection(document, &selection->begin, &selection->end);
    if (hyperlink == nullptr) {
        return selection->begin != selection->end;
    }

    if (pgGetHyperlinkSourceInfo(document, selection->begin, 0, false, hyperlink) ||
        pgGetHyperlinkSourceInfo(document, selection->end, 0, false, hyperlink)) {
        if (!IsEditableHyperlinkType(hyperlink->type)) {
            return false;
        }
        selection->begin = std::min(selection->begin, hyperlink->applied_range.begin);
        selection->end = std::max(selection->end, hyperlink->applied_range.end);
        return true;
    }
    return selection->begin != selection->end;
}

bool IsFixedWidthFont(const font_info& info) {
    if ((info.environs & NAME_IS_CSTR) == 0) {
        return false;
    }
    const std::string name(reinterpret_cast<const char*>(info.name));
    const std::string lowered = Lowercase(name);
    return lowered.find("mono") != std::string::npos || lowered.find("courier") != std::string::npos;
}

bool ParseCssColor(std::string_view value, COLORREF* color) {
    if (color == nullptr) {
        return false;
    }
    const std::string normalized = Lowercase(value);
    auto pack = [color](unsigned char red, unsigned char green, unsigned char blue) {
        *color = static_cast<COLORREF>((static_cast<unsigned long>(blue) << 16) |
                                       (static_cast<unsigned long>(green) << 8) |
                                       static_cast<unsigned long>(red));
    };

    if (normalized.size() == 7 && normalized.front() == '#') {
        const auto parse = [](char hi, char lo) -> std::optional<unsigned char> {
            const auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') {
                    return ch - '0';
                }
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (ch >= 'a' && ch <= 'f') {
                    return 10 + (ch - 'a');
                }
                return -1;
            };
            const int high = hex(hi);
            const int low = hex(lo);
            if (high < 0 || low < 0) {
                return std::nullopt;
            }
            return static_cast<unsigned char>((high << 4) | low);
        };

        const auto red = parse(normalized[1], normalized[2]);
        const auto green = parse(normalized[3], normalized[4]);
        const auto blue = parse(normalized[5], normalized[6]);
        if (!red || !green || !blue) {
            return false;
        }
        pack(*red, *green, *blue);
        return true;
    }

    if (normalized == "black") {
        pack(0, 0, 0);
        return true;
    }
    if (normalized == "white") {
        pack(255, 255, 255);
        return true;
    }
    if (normalized == "red") {
        pack(255, 0, 0);
        return true;
    }
    if (normalized == "green") {
        pack(0, 128, 0);
        return true;
    }
    if (normalized == "blue") {
        pack(0, 0, 255);
        return true;
    }
    if (normalized == "orange") {
        pack(255, 165, 0);
        return true;
    }
    if (normalized == "purple") {
        pack(128, 0, 128);
        return true;
    }
    if (normalized == "gray" || normalized == "grey") {
        pack(128, 128, 128);
        return true;
    }
    return false;
}

std::optional<short> ParsePointSize(std::string_view value) {
    const std::string trimmed = Lowercase(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    if (trimmed.size() == 1 && trimmed.front() >= '1' && trimmed.front() <= '7') {
        static constexpr short kSizeMap[] = {8, 10, 12, 14, 18, 24, 36};
        return kSizeMap[trimmed.front() - '1'];
    }

    try {
        const int numeric = std::stoi(std::string(trimmed));
        if (numeric < 1 || numeric > 96) {
            return std::nullopt;
        }
        return static_cast<short>(numeric);
    } catch (...) {
        return std::nullopt;
    }
}

std::filesystem::path TemporaryImportPath(std::string_view extension) {
    const auto directory = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto path = directory / ("hermes-paige-" + std::to_string(::getpid()) + "-" +
                                       std::to_string(std::rand()) + "." + std::string(extension));
        if (!std::filesystem::exists(path)) {
            return path;
        }
    }
    return directory / ("hermes-paige-" + std::to_string(::getpid()) + "." + std::string(extension));
}

bool InsertNativeHtmlFragment(pg_ref document, std::string_view html) {
    if (document == nullptr || html.empty()) {
        return false;
    }

    const auto path = TemporaryImportPath("html");
    {
        std::ofstream output(path, std::ios::binary);
        if (!output.is_open()) {
            return false;
        }
        output.write(html.data(), static_cast<std::streamsize>(html.size()));
        if (!output.good()) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            return false;
        }
    }

    select_pair selection {};
    pgGetSelection(document, &selection.begin, &selection.end);
    const bool had_selection = selection.begin < selection.end;
    if (had_selection) {
        pgDelete(document, &selection, best_way);
    }

    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return false;
    }
    const long import_flags =
        IMPORT_EVERYTHING_FLAG | IMPORT_BKCOLOR_FLAG | IMPORT_HYPERTEXT_FLAG | IMPORT_TABLES_FLAG;
    const pg_error error = pgImportFileFromC(document, pg_html_type, import_flags, 0, fd);
    ::close(fd);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    if (error != NO_ERROR) {
        return false;
    }

    if (had_selection) {
        select_pair imported_selection {};
        pgGetSelection(document, &imported_selection.begin, &imported_selection.end);
        pgSetSelection(document, selection.begin, imported_selection.end, 0, TRUE);
    }
    return true;
}

bool InsertNativeHorizontalRule(pg_ref document) {
    if (document == nullptr) {
        return false;
    }

    select_pair selection {};
    pgGetSelection(document, &selection.begin, &selection.end);
    long paragraph_begin = 0;
    long paragraph_end = 0;
    pgFindPar(document, selection.begin, &paragraph_begin, &paragraph_end);

    const char line_break = '\n';
    pgInsert(document,
             reinterpret_cast<pg_char_ptr>(const_cast<char*>(&line_break)),
             sizeof(line_break),
             paragraph_begin,
             data_insert_mode,
             0,
             best_way);

    par_info info {};
    par_info mask {};
    pgInitParMask(&info, 0);
    pgInitParMask(&mask, 0);
    mask.table.border_info = -1;
    info.table.border_info |= PG_BORDER_LINERULE;

    selection.begin = selection.end = paragraph_begin;
    pgSetParInfoEx(document, &selection, &info, &mask, FALSE, best_way);
    return true;
}

bool ShouldCopyParagraphInfo(pg_ref document, select_pair selection) {
    if (document == nullptr) {
        return false;
    }
    if (selection.begin == selection.end) {
        return true;
    }

    long paragraph_begin = 0;
    long paragraph_end = 0;
    pgFindPar(document, selection.begin, &paragraph_begin, &paragraph_end);
    if (paragraph_begin < selection.begin) {
        if ((paragraph_end - 1) < (selection.end - 2)) {
            pgFindPar(document, paragraph_end + 2, &paragraph_begin, &paragraph_end);
        }
    } else if ((paragraph_end - 1) > selection.end) {
        if (paragraph_begin > (selection.begin + 2)) {
            pgFindPar(document, paragraph_begin - 2, &paragraph_begin, &paragraph_end);
        }
    }

    paragraph_end = std::max<long>(paragraph_begin, paragraph_end - 1);
    return paragraph_begin >= selection.begin && paragraph_end <= selection.end;
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

ComposeEditorCommandState NativeCommandState(const hermes::PaigeRichTextSurface& surface,
                                            ComposeEditorCommand command) {
    ComposeEditorCommandState state;
    if (!UsesNativeSurface(surface)) {
        return state;
    }

    state.enabled = true;
    const auto document = NativeDocument(surface);

    style_info style_info_value {};
    style_info style_mask {};
    pgInitStyleMask(&style_info_value, 0);
    pgInitStyleMask(&style_mask, 0);
    pgGetStyleInfo(document, nullptr, FALSE, &style_info_value, &style_mask);

    switch (command) {
        case ComposeEditorCommand::kInsertLink: {
            select_pair selection {};
            pg_hyperlink hyperlink {};
            state.enabled = NativeLinkSelection(document, &selection, &hyperlink);
            state.checked =
                state.enabled &&
                (pgGetHyperlinkSourceInfo(document, selection.begin, 0, false, &hyperlink) ||
                 pgGetHyperlinkSourceInfo(document, selection.end, 0, false, &hyperlink));
            return state;
        }
        case ComposeEditorCommand::kBold:
            state.checked = style_info_value.styles[bold_var] != 0 && style_mask.styles[bold_var] != 0;
            state.indeterminate = style_mask.styles[bold_var] == 0;
            return state;
        case ComposeEditorCommand::kItalic:
            state.checked = style_info_value.styles[italic_var] != 0 && style_mask.styles[italic_var] != 0;
            state.indeterminate = style_mask.styles[italic_var] == 0;
            return state;
        case ComposeEditorCommand::kUnderline:
            state.checked = style_info_value.styles[underline_var] != 0 && style_mask.styles[underline_var] != 0;
            state.indeterminate = style_mask.styles[underline_var] == 0;
            return state;
        case ComposeEditorCommand::kStrikeout:
            state.checked = style_info_value.styles[strikeout_var] != 0 && style_mask.styles[strikeout_var] != 0;
            state.indeterminate = style_mask.styles[strikeout_var] == 0;
            return state;
        case ComposeEditorCommand::kPlain:
            state.checked = (style_info_value.styles[bold_var] == 0 && style_info_value.styles[italic_var] == 0 &&
                             style_info_value.styles[underline_var] == 0 &&
                             style_info_value.styles[strikeout_var] == 0);
            return state;
        case ComposeEditorCommand::kFixedWidth: {
            font_info font_info_value {};
            font_info font_mask {};
            pgFillBlock(&font_info_value, sizeof(font_info), 0);
            pgFillBlock(&font_mask, sizeof(font_info), 0);
            pgGetFontInfo(document, nullptr, FALSE, &font_info_value, &font_mask);
            state.checked = IsFixedWidthFont(font_info_value);
            return state;
        }
        case ComposeEditorCommand::kTextColor:
        case ComposeEditorCommand::kTextSize:
        case ComposeEditorCommand::kAddQuote:
        case ComposeEditorCommand::kRemoveQuote:
        case ComposeEditorCommand::kIndentIn:
        case ComposeEditorCommand::kIndentOut:
        case ComposeEditorCommand::kNormalMargins:
        case ComposeEditorCommand::kClearFormatting:
        case ComposeEditorCommand::kInsertHorizontalRule:
        case ComposeEditorCommand::kWrapSelection:
            return state;
        default:
            break;
    }

    par_info paragraph_info {};
    par_info paragraph_mask {};
    pgInitParMask(&paragraph_info, 0);
    pgInitParMask(&paragraph_mask, 0);
    pgGetParInfo(document, nullptr, FALSE, &paragraph_info, &paragraph_mask);
    switch (command) {
        case ComposeEditorCommand::kAlignLeft:
            state.checked = paragraph_info.justification == justify_left && paragraph_mask.justification != 0;
            state.indeterminate = paragraph_mask.justification == 0;
            break;
        case ComposeEditorCommand::kAlignCenter:
            state.checked = paragraph_info.justification == justify_center && paragraph_mask.justification != 0;
            state.indeterminate = paragraph_mask.justification == 0;
            break;
        case ComposeEditorCommand::kAlignRight:
            state.checked = paragraph_info.justification == justify_right && paragraph_mask.justification != 0;
            state.indeterminate = paragraph_mask.justification == 0;
            break;
        case ComposeEditorCommand::kBulletedList:
            state.checked =
                (paragraph_info.class_info & BULLETED_LINE) != 0 &&
                (paragraph_info.html_style & html_unordered_list) != 0;
            break;
        default:
            break;
    }
    return state;
}

bool ToggleNativeStyleBit(pg_ref document, long bit) {
    pgSetStyleBits(document, bit, bit, nullptr, best_way);
    return true;
}

bool SetNativeJustification(pg_ref document, short justification) {
    par_info info {};
    par_info mask {};
    pgInitParMask(&info, 0);
    pgInitParMask(&mask, 0);
    pgGetParInfo(document, nullptr, FALSE, &info, &mask);
    info.justification = justification;
    mask.justification = -1;
    pgSetParInfo(document, nullptr, &info, &mask, best_way);
    return true;
}

bool ShiftNativeIndent(pg_ref document, pg_fixed delta) {
    pg_indents indent {};
    pg_indents mask {};
    std::memset(&indent, 0, sizeof(indent));
    std::memset(&mask, 0, sizeof(mask));
    mask.left_indent = -1;
    mask.first_indent = 0;
    mask.right_indent = 0;
    pgGetIndents(document, nullptr, &indent, &mask, nullptr, nullptr);
    indent.left_indent = std::max<pg_fixed>(0, indent.left_indent + delta);
    pgSetIndents(document, nullptr, &indent, &mask, best_way);
    return true;
}

bool ResetNativeMargins(pg_ref document) {
    pg_indents indent {};
    pg_indents mask {};
    std::memset(&indent, 0, sizeof(indent));
    std::memset(&mask, 0, sizeof(mask));
    mask.left_indent = -1;
    mask.first_indent = 0;
    mask.right_indent = 0;
    indent.left_indent = 0;
    pgSetIndents(document, nullptr, &indent, &mask, best_way);
    return true;
}

bool ToggleNativeBulletList(pg_ref document) {
    par_info info {};
    par_info mask {};
    pgInitParMask(&info, 0);
    pgInitParMask(&mask, 0);
    pgGetParInfo(document, nullptr, FALSE, &info, &mask);
    mask.html_bullet = -1;
    mask.class_info = -1;
    mask.indents.left_indent = -1;
    mask.html_style = -1;
    const bool enabled =
        (info.class_info & BULLETED_LINE) != 0 && (info.html_style & html_unordered_list) != 0;
    if (enabled) {
        info.html_bullet = 0;
        info.class_info = 0;
        info.indents.left_indent = std::max<pg_fixed>(0, info.indents.left_indent - kDefaultListIndent);
        info.html_style = (info.indents.left_indent >= kDefaultListIndent) ? html_definition_list : 0;
    } else {
        info.class_info |= BULLETED_LINE;
        info.html_bullet = 1;
        info.indents.left_indent += kDefaultListIndent;
        info.html_style = html_unordered_list;
    }
    pgSetParInfo(document, nullptr, &info, &mask, best_way);
    return true;
}

bool SetNativeFixedWidth(pg_ref document) {
    font_info info {};
    font_info mask {};
    pgFillBlock(&info, sizeof(font_info), 0);
    pgFillBlock(&mask, sizeof(font_info), 0);
    std::strncpy(reinterpret_cast<char*>(info.name), "Courier New", sizeof(info.name) - 1);
    info.environs |= NAME_IS_CSTR;
    mask.environs = NAME_IS_CSTR;
    std::memset(mask.name, 0xFF, sizeof(mask.name));
    pgSetFontInfo(document, nullptr, &info, &mask, best_way);
    return true;
}

bool SetNativeTextColor(pg_ref document, std::string_view color_value) {
    COLORREF color = 0;
    if (!ParseCssColor(color_value, &color)) {
        return false;
    }
    pgSetTextColor(document, &color, nullptr, TRUE);
    return true;
}

bool SetNativeTextSize(pg_ref document, std::string_view size_value) {
    const auto point_size = ParsePointSize(size_value);
    if (!point_size.has_value()) {
        return false;
    }
    pgSetPointSize(document, *point_size, nullptr, TRUE);
    return true;
}

bool ClearNativeFormatting(pg_ref document, const hermes::PaigeRichTextSurface& surface) {
    pgSetStyleBits(document, X_PLAIN_TEXT, X_ALL_STYLES, nullptr, best_way);
    (void)ResetNativeMargins(document);
    (void)SetNativeJustification(document, justify_left);
    const auto bullet_state = NativeCommandState(surface, ComposeEditorCommand::kBulletedList);
    if (bullet_state.checked) {
        (void)ToggleNativeBulletList(document);
    }
    return true;
}

std::optional<ComposeEditorStyleSnapshot> CaptureNativeStyleSnapshot(const hermes::PaigeRichTextSurface& surface) {
    if (!UsesNativeSurface(surface)) {
        return std::nullopt;
    }

    ComposeEditorStyleSnapshot snapshot;
    const auto document = NativeDocument(surface);
    select_pair selection {};
    pgGetSelection(document, &selection.begin, &selection.end);
    style_info style_info_value {};
    style_info style_mask {};
    pgInitStyleMask(&style_info_value, 0);
    pgInitStyleMask(&style_mask, 0);
    pgGetStyleInfo(document, nullptr, FALSE, &style_info_value, &style_mask);
    snapshot.bold = style_info_value.styles[bold_var] != 0;
    snapshot.italic = style_info_value.styles[italic_var] != 0;
    snapshot.underline = style_info_value.styles[underline_var] != 0;
    snapshot.strikeout = style_info_value.styles[strikeout_var] != 0;

    font_info font_info_value {};
    font_info font_mask {};
    pgFillBlock(&font_info_value, sizeof(font_info), 0);
    pgFillBlock(&font_mask, sizeof(font_info), 0);
    pgGetFontInfo(document, nullptr, FALSE, &font_info_value, &font_mask);
    snapshot.fixed_width = IsFixedWidthFont(font_info_value);
    if (style_info_value.point > 0) {
        snapshot.text_size = std::to_string(style_info_value.point);
    }
    if (style_info_value.fg_color.red != 0 || style_info_value.fg_color.green != 0 ||
        style_info_value.fg_color.blue != 0) {
        auto channel = [](unsigned short value) {
            return static_cast<unsigned int>(value >> 8);
        };
        std::ostringstream color;
        color << '#'
              << std::hex << std::uppercase
              << std::setw(2) << std::setfill('0') << channel(style_info_value.fg_color.red)
              << std::setw(2) << std::setfill('0') << channel(style_info_value.fg_color.green)
              << std::setw(2) << std::setfill('0') << channel(style_info_value.fg_color.blue);
        snapshot.text_color = color.str();
    }

    par_info paragraph_info {};
    par_info paragraph_mask {};
    pgInitParMask(&paragraph_info, 0);
    pgInitParMask(&paragraph_mask, 0);
    snapshot.include_paragraph_style = ShouldCopyParagraphInfo(document, selection);
    if (snapshot.include_paragraph_style) {
        pgGetParInfo(document, nullptr, FALSE, &paragraph_info, &paragraph_mask);
        snapshot.bulleted_list =
            (paragraph_info.class_info & BULLETED_LINE) != 0 &&
            (paragraph_info.html_style & html_unordered_list) != 0;
        switch (paragraph_info.justification) {
            case justify_center:
                snapshot.alignment = ComposeEditorAlignment::kCenter;
                break;
            case justify_right:
                snapshot.alignment = ComposeEditorAlignment::kRight;
                break;
            default:
                snapshot.alignment = ComposeEditorAlignment::kLeft;
                break;
        }
    }

    pg_indents indent {};
    pg_indents indent_mask {};
    std::memset(&indent, 0, sizeof(indent));
    std::memset(&indent_mask, 0, sizeof(indent_mask));
    indent_mask.left_indent = -1;
    if (snapshot.include_paragraph_style) {
        pgGetIndents(document, nullptr, &indent, &indent_mask, nullptr, nullptr);
        snapshot.left_indent = static_cast<int>(indent.left_indent);
    }
    return snapshot;
}

bool ApplyNativeStyleSnapshot(const hermes::PaigeRichTextSurface& surface,
                              const ComposeEditorStyleSnapshot& snapshot) {
    if (!UsesNativeSurface(surface)) {
        return false;
    }
    const auto* document = NativeDocument(surface);
    long style_bits = 0;
    long set_bits = X_BOLD_BIT | X_ITALIC_BIT | X_UNDERLINE_BIT | X_STRIKEOUT_BIT;
    if (snapshot.bold) {
        style_bits |= X_BOLD_BIT;
    }
    if (snapshot.italic) {
        style_bits |= X_ITALIC_BIT;
    }
    if (snapshot.underline) {
        style_bits |= X_UNDERLINE_BIT;
    }
    if (snapshot.strikeout) {
        style_bits |= X_STRIKEOUT_BIT;
    }
    pgSetStyleBits(document, style_bits, set_bits, nullptr, best_way);
    if (snapshot.fixed_width) {
        (void)SetNativeFixedWidth(document);
    }
    if (snapshot.text_color.has_value()) {
        (void)SetNativeTextColor(document, *snapshot.text_color);
    }
    if (snapshot.text_size.has_value()) {
        (void)SetNativeTextSize(document, *snapshot.text_size);
    }
    if (snapshot.include_paragraph_style) {
        if (snapshot.alignment.has_value()) {
            switch (*snapshot.alignment) {
                case ComposeEditorAlignment::kLeft:
                    (void)SetNativeJustification(document, justify_left);
                    break;
                case ComposeEditorAlignment::kCenter:
                    (void)SetNativeJustification(document, justify_center);
                    break;
                case ComposeEditorAlignment::kRight:
                    (void)SetNativeJustification(document, justify_right);
                    break;
            }
        }
        if (snapshot.left_indent.has_value()) {
            pg_indents indent {};
            pg_indents mask {};
            std::memset(&indent, 0, sizeof(indent));
            std::memset(&mask, 0, sizeof(mask));
            mask.left_indent = -1;
            indent.left_indent = static_cast<pg_fixed>(*snapshot.left_indent);
            pgSetIndents(document, nullptr, &indent, &mask, best_way);
        }
        const auto bullet_state = NativeCommandState(surface, ComposeEditorCommand::kBulletedList);
        if (bullet_state.checked != snapshot.bulleted_list) {
            (void)ToggleNativeBulletList(document);
        }
    }
    return true;
}
#endif

}  // namespace

class PaigeEditorHost final : public ComposeEditorHost {
public:
    explicit PaigeEditorHost(hermes::PaigeRichTextSurface& surface)
        : editor_(new PaigeEditorView(surface)),
          root_(new BScrollView("paige-editor-scroll", editor_, 0, true, true)),
          surface_(surface) {}

    BView* RootView() const override {
        return root_;
    }

    void SetChangeCallback(std::function<void()> callback) override {
        editor_->SetChangeCallback(std::move(callback));
    }

    void SetSelectionChangeCallback(std::function<void()> callback) override {
        editor_->SetSelectionChangeCallback(std::move(callback));
    }

    void ReloadFromSurface() override {
        editor_->ReloadFromSurface();
    }

    void ScrollSelectionIntoView() override {
        editor_->ScrollSelectionIntoView();
    }

    void SetTabNavigationCallback(std::function<bool(bool shift)> callback) override {
        editor_->SetTabNavigationCallback(std::move(callback));
    }

    bool SelectAllText() override {
        return editor_->SelectAllText();
    }

    bool CopySelection() override {
        return editor_->CopySelection();
    }

    bool CutSelection() override {
        return editor_->CutSelection();
    }

    bool Paste() override {
        return editor_->Paste();
    }

    bool SupportsCommand(ComposeEditorCommand command) const override {
        if (UsesNativeSurface(surface_)) {
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
                case ComposeEditorCommand::kClearFormatting:
                case ComposeEditorCommand::kTextColor:
                case ComposeEditorCommand::kTextSize:
                case ComposeEditorCommand::kInsertDownloadablePicture:
                case ComposeEditorCommand::kInsertHorizontalRule:
                case ComposeEditorCommand::kWrapSelection:
                    return true;
                case ComposeEditorCommand::kFormatPainter:
                    return false;
                case ComposeEditorCommand::kInsertLink: {
                    select_pair selection {};
                    pg_hyperlink hyperlink {};
                    return NativeLinkSelection(NativeDocument(surface_), &selection, &hyperlink);
                }
            }
        }
        switch (command) {
            case ComposeEditorCommand::kAddQuote:
            case ComposeEditorCommand::kRemoveQuote:
            case ComposeEditorCommand::kInsertHorizontalRule:
            case ComposeEditorCommand::kWrapSelection:
                return true;
            default:
                return false;
        }
    }

    ComposeEditorCommandState CommandState(ComposeEditorCommand command) const override {
        if (UsesNativeSurface(surface_)) {
            return NativeCommandState(surface_, command);
        }
        ComposeEditorCommandState state;
        state.enabled = SupportsCommand(command);
        return state;
    }

    bool ExecuteCommand(ComposeEditorCommand command) override {
        if (UsesNativeSurface(surface_)) {
            const auto document = NativeDocument(surface_);
            switch (command) {
                case ComposeEditorCommand::kPlain:
                    pgSetStyleBits(document, X_PLAIN_TEXT, X_ALL_STYLES, nullptr, best_way);
                    return true;
                case ComposeEditorCommand::kBold:
                    return ToggleNativeStyleBit(document, X_BOLD_BIT);
                case ComposeEditorCommand::kItalic:
                    return ToggleNativeStyleBit(document, X_ITALIC_BIT);
                case ComposeEditorCommand::kUnderline:
                    return ToggleNativeStyleBit(document, X_UNDERLINE_BIT);
                case ComposeEditorCommand::kStrikeout:
                    return ToggleNativeStyleBit(document, X_STRIKEOUT_BIT);
                case ComposeEditorCommand::kFixedWidth:
                    return SetNativeFixedWidth(document);
                case ComposeEditorCommand::kAddQuote:
                    return ReplaceCurrentSelection(QuoteSelectionText(surface_.CopySelection(), false));
                case ComposeEditorCommand::kRemoveQuote:
                    return ReplaceCurrentSelection(QuoteSelectionText(surface_.CopySelection(), true));
                case ComposeEditorCommand::kIndentIn:
                    return ShiftNativeIndent(document, kDefaultListIndent);
                case ComposeEditorCommand::kIndentOut:
                    return ShiftNativeIndent(document, -kDefaultListIndent);
                case ComposeEditorCommand::kNormalMargins:
                    return ResetNativeMargins(document);
                case ComposeEditorCommand::kAlignLeft:
                    return SetNativeJustification(document, justify_left);
                case ComposeEditorCommand::kAlignCenter:
                    return SetNativeJustification(document, justify_center);
                case ComposeEditorCommand::kAlignRight:
                    return SetNativeJustification(document, justify_right);
                case ComposeEditorCommand::kBulletedList:
                    return ToggleNativeBulletList(document);
                case ComposeEditorCommand::kClearFormatting:
                    return ClearNativeFormatting(document, surface_);
                case ComposeEditorCommand::kInsertHorizontalRule:
                    return InsertNativeHorizontalRule(document);
                case ComposeEditorCommand::kWrapSelection:
                    return ReplaceCurrentSelection(WrapPlainText(surface_.CopySelection(), 72));
                case ComposeEditorCommand::kTextColor:
                case ComposeEditorCommand::kTextSize:
                case ComposeEditorCommand::kFormatPainter:
                    return false;
                case ComposeEditorCommand::kInsertDownloadablePicture:
                    return InsertDownloadablePicture({}, {});
                case ComposeEditorCommand::kInsertLink:
                    return false;
            }
        }
        switch (command) {
            case ComposeEditorCommand::kAddQuote:
                return ReplaceCurrentSelection(QuoteSelectionText(surface_.CopySelection(), false));
            case ComposeEditorCommand::kRemoveQuote:
                return ReplaceCurrentSelection(QuoteSelectionText(surface_.CopySelection(), true));
            case ComposeEditorCommand::kInsertHorizontalRule:
                return ReplaceCurrentSelection("\n----------------------------------------\n");
            default:
                return false;
        }
    }

    std::string SelectedText() const override {
        return surface_.CopySelection();
    }

    bool InsertLink(std::string_view url) override {
        if (!UsesNativeSurface(surface_)) {
            return false;
        }
        std::string link_url(url);
        if (link_url.empty()) {
            return false;
        }

        const auto document = NativeDocument(surface_);
        select_pair selection {};
        pg_hyperlink hyperlink {};
        if (!NativeLinkSelection(document, &selection, &hyperlink)) {
            return false;
        }

        link_url.resize(std::min(link_url.size(), kMaxNativeHyperlinkLength - 1));
        if (pgGetHyperlinkSourceInfo(document, selection.begin, 0, false, &hyperlink) ||
            pgGetHyperlinkSourceInfo(document, selection.end, 0, false, &hyperlink)) {
            pgSetSelection(document, selection.begin, selection.end, 0, true);
            pgChangeHyperlinkSource(document,
                                    hyperlink.applied_range.begin,
                                    &selection,
                                    reinterpret_cast<pg_char_ptr>(link_url.data()),
                                    nullptr,
                                    nullptr,
                                    0,
                                    0,
                                    0,
                                    best_way);
            return true;
        }

        auto* pg_ptr = reinterpret_cast<paige_rec_ptr>(UseMemory(document));
        if (pg_ptr == nullptr) {
            return false;
        }
        const long link_id = pgAssignLinkID(pg_ptr->hyperlinks);
        UnuseMemory(document);
        pgSetHyperlinkSource(document,
                             &selection,
                             reinterpret_cast<pg_char_ptr>(link_url.data()),
                             nullptr,
                             nullptr,
                             HYPERLINK_EUDORA_URL,
                             link_id,
                             0,
                             0,
                             0,
                             best_way);
        return true;
    }

    bool ApplyTextColor(std::string_view color) override {
        if (!UsesNativeSurface(surface_)) {
            (void)color;
            return false;
        }
        return SetNativeTextColor(NativeDocument(surface_), color);
    }

    bool ApplyTextSize(std::string_view size) override {
        if (!UsesNativeSurface(surface_)) {
            (void)size;
            return false;
        }
        return SetNativeTextSize(NativeDocument(surface_), size);
    }

    bool InsertDownloadablePicture(std::string_view url, std::string_view alt_text) override {
        if (!UsesNativeSurface(surface_) || url.empty()) {
            return false;
        }

        std::ostringstream html;
        html << "<img src=\"" << EscapeHtmlAttribute(url) << '"';
        if (!alt_text.empty()) {
            html << " alt=\"" << EscapeHtmlAttribute(alt_text) << '"';
        }
        html << '>';
        return InsertNativeHtmlFragment(NativeDocument(surface_), html.str());
    }

    bool InsertHorizontalRule() override {
        return ExecuteCommand(ComposeEditorCommand::kInsertHorizontalRule);
    }

    bool WrapSelection(std::size_t columns) override {
        return ReplaceCurrentSelection(WrapPlainText(surface_.CopySelection(), columns));
    }

    std::optional<ComposeEditorStyleSnapshot> CaptureStyleSnapshot() const override {
        if (UsesNativeSurface(surface_)) {
            return CaptureNativeStyleSnapshot(surface_);
        }
        return std::nullopt;
    }

    bool ApplyStyleSnapshot(const ComposeEditorStyleSnapshot& snapshot) override {
        if (UsesNativeSurface(surface_)) {
            return ApplyNativeStyleSnapshot(surface_, snapshot);
        }
        (void)snapshot;
        return false;
    }

    void MakeEditorFocus() override {
        editor_->MakeFocus(true);
    }

    void InvalidateEditor() override {
        editor_->Invalidate();
    }

private:
    bool ReplaceCurrentSelection(std::string replacement) {
        return surface_.ReplaceSelection(replacement);
    }

    PaigeEditorView* editor_ = nullptr;
    BScrollView* root_ = nullptr;
    hermes::PaigeRichTextSurface& surface_;
};

PaigeEditorView::PaigeEditorView(hermes::PaigeRichTextSurface& surface)
    : BView("paige-editor", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE | B_FULL_UPDATE_ON_RESIZE),
      surface_(surface) {
    SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetFont(be_fixed_font);
    UpdateMetrics();
    surface_.AttachNativeView(this);
    surface_.ResizeNativeHost(Bounds().Width(), Bounds().Height());
    ResizeToDocument();
}

PaigeEditorView::~PaigeEditorView() {
    surface_.DetachNativeView();
}

void PaigeEditorView::SetChangeCallback(std::function<void()> callback) {
    change_callback_ = std::move(callback);
}

void PaigeEditorView::SetSelectionChangeCallback(std::function<void()> callback) {
    selection_change_callback_ = std::move(callback);
}

void PaigeEditorView::SetTabNavigationCallback(std::function<bool(bool shift)> callback) {
    tab_navigation_callback_ = std::move(callback);
}

void PaigeEditorView::ReloadFromSurface() {
    if (UsesNativeSurface(surface_)) {
        surface_.ResizeNativeHost(Bounds().Width(), Bounds().Height());
        Invalidate();
        return;
    }
    UpdateMetrics();
    ResizeToDocument();
    Invalidate();
}

void PaigeEditorView::ScrollSelectionIntoView() {
    if (UsesNativeSurface(surface_)) {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        if (!surface_.NativeCaretRect(surface_.Selection().start, &left, &top, &right, &bottom)) {
            return;
        }
        if (top < Bounds().top + inset_) {
            ScrollTo(Bounds().left, std::max(0.0f, top - inset_));
        } else if (bottom > Bounds().bottom - inset_) {
            ScrollTo(Bounds().left, std::max(0.0f, bottom - Bounds().Height() + inset_));
        }
        return;
    }
    const BPoint caret = PointForOffset(surface_.Selection().start);
    if (caret.y < Bounds().top + inset_) {
        ScrollTo(Bounds().left, std::max(0.0f, caret.y - inset_));
    } else if (caret.y + line_height_ > Bounds().bottom - inset_) {
        ScrollTo(Bounds().left, std::max(0.0f, caret.y + line_height_ - Bounds().Height() + inset_));
    }
}

bool PaigeEditorView::SelectAllText() {
    if (!surface_.SelectAll()) {
        return false;
    }
    Invalidate();
    return true;
}

bool PaigeEditorView::CopySelection() {
    const std::string copied = surface_.CopySelection();
    if (copied.empty()) {
        return false;
    }
    CopySelectionToClipboard();
    return true;
}

bool PaigeEditorView::CutSelection() {
    const std::string copied = surface_.CopySelection();
    if (copied.empty()) {
        return false;
    }
    CopySelectionToClipboard();
    if (surface_.CutSelection().empty()) {
        return false;
    }
    NotifyEdited();
    return true;
}

bool PaigeEditorView::Paste() {
    if (!PasteFromClipboard()) {
        return false;
    }
    NotifyEdited();
    return true;
}

BSize PaigeEditorView::MinSize() {
    return BSize(240.0f, 160.0f);
}

BSize PaigeEditorView::PreferredSize() {
    if (UsesNativeSurface(surface_)) {
        return BSize(std::max(240.0f, Bounds().Width()), std::max(160.0f, Bounds().Height()));
    }

    const auto lines = BuildLines();
    float widest = 240.0f;
    for (const auto& line : lines) {
        widest = std::max(widest, inset_ * 2.0f + char_width_ * static_cast<float>(line.text.size() + 1));
    }
    return BSize(widest, inset_ * 2.0f + static_cast<float>(lines.size()) * line_height_);
}

void PaigeEditorView::Draw(BRect update_rect) {
    if (UsesNativeSurface(surface_)) {
        FillRect(Bounds(), B_SOLID_LOW);
        surface_.DrawNative(this, update_rect.left, update_rect.top, update_rect.right, update_rect.bottom);
        return;
    }

    (void)update_rect;
    FillRect(Bounds(), B_SOLID_LOW);

    const auto lines = BuildLines();
    DrawSelection(lines);

    SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const float baseline = inset_ + static_cast<float>(index) * line_height_ + line_height_ - 5.0f;
        DrawString(lines[index].text.c_str(), BPoint(inset_, baseline));
    }

    DrawDiagnostics(lines);
    DrawCaret();
}

void PaigeEditorView::MouseDown(BPoint where) {
    MakeFocus(true);
    dragging_selection_ = true;
    drag_anchor_ = UsesNativeSurface(surface_) ? surface_.NativeOffsetForPoint(where.x, where.y)
                                               : OffsetForPoint(where);
    surface_.SetSelection({drag_anchor_, 0});
    if (selection_change_callback_) {
        selection_change_callback_();
    }
    Invalidate();
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
}

void PaigeEditorView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag_message) {
    (void)transit;
    (void)drag_message;
    if (!dragging_selection_) {
        return;
    }

    const std::size_t current =
        UsesNativeSurface(surface_) ? surface_.NativeOffsetForPoint(where.x, where.y) : OffsetForPoint(where);
    const std::size_t start = std::min(drag_anchor_, current);
    const std::size_t end = std::max(drag_anchor_, current);
    surface_.SetSelection({start, end - start});
    if (selection_change_callback_) {
        selection_change_callback_();
    }
    Invalidate();
}

void PaigeEditorView::MouseUp(BPoint where) {
    (void)where;
    dragging_selection_ = false;
    if (selection_change_callback_) {
        selection_change_callback_();
    }
}

void PaigeEditorView::KeyDown(const char* bytes, int32 num_bytes) {
    if (bytes == nullptr || num_bytes <= 0) {
        BView::KeyDown(bytes, num_bytes);
        return;
    }

    if (bytes[0] == B_TAB && tab_navigation_callback_ &&
        tab_navigation_callback_((modifiers() & B_SHIFT_KEY) != 0)) {
        return;
    }

    switch (bytes[0]) {
        case B_LEFT_ARROW:
            MoveCaretHorizontal(-1);
            return;
        case B_RIGHT_ARROW:
            MoveCaretHorizontal(1);
            return;
        case B_UP_ARROW:
            MoveCaretVertical(-1);
            return;
        case B_DOWN_ARROW:
            MoveCaretVertical(1);
            return;
        case B_BACKSPACE:
            DeleteBackward();
            return;
        case B_DELETE:
            DeleteForward();
            return;
        default:
            break;
    }

    if ((modifiers() & B_COMMAND_KEY) != 0) {
        switch (bytes[0]) {
            case 'a':
            case 'A':
                (void)SelectAllText();
                return;
            case 'c':
            case 'C':
                (void)CopySelection();
                return;
            case 'x':
            case 'X':
                (void)CutSelection();
                return;
            case 'v':
            case 'V':
                (void)Paste();
                return;
            default:
                break;
        }
    }

    InsertText(std::string_view(bytes, static_cast<std::size_t>(num_bytes)));
}

void PaigeEditorView::MakeFocus(bool focus) {
    BView::MakeFocus(focus);
    Invalidate();
}

void PaigeEditorView::FrameResized(float width, float height) {
    BView::FrameResized(width, height);
    surface_.ResizeNativeHost(width, height);
    Invalidate();
}

void PaigeEditorView::NotifyEdited() {
    if (change_callback_) {
        change_callback_();
    } else {
        ReloadFromSurface();
    }
}

void PaigeEditorView::UpdateMetrics() {
    font_height font_metrics{};
    GetFontHeight(&font_metrics);
    line_height_ = std::ceil(font_metrics.ascent + font_metrics.descent + font_metrics.leading + 4.0f);
    char_width_ = std::max(6.0f, StringWidth("M"));
}

void PaigeEditorView::ResizeToDocument() {
    if (UsesNativeSurface(surface_)) {
        ResizeTo(std::max(240.0f, Bounds().Width()), std::max(160.0f, Bounds().Height()));
        return;
    }
    const BSize preferred = PreferredSize();
    ResizeTo(std::max(preferred.width, Bounds().Width()), std::max(preferred.height, Bounds().Height()));
}

std::vector<PaigeEditorView::LayoutLine> PaigeEditorView::BuildLines() const {
    std::vector<LayoutLine> lines;
    const std::string& text = surface_.Snapshot().plain_text;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back({start, text.substr(start)});
            break;
        }
        lines.push_back({start, text.substr(start, end - start)});
        start = end + 1;
        if (start == text.size()) {
            lines.push_back({start, ""});
            break;
        }
    }
    if (lines.empty()) {
        lines.push_back({0, ""});
    }
    return lines;
}

std::size_t PaigeEditorView::OffsetForPoint(BPoint where) const {
    const auto lines = BuildLines();
    const float local_y = std::max(0.0f, where.y - inset_);
    const std::size_t line_index =
        std::min(lines.size() - 1, static_cast<std::size_t>(local_y / line_height_));
    const auto& line = lines[line_index];
    const float local_x = std::max(0.0f, where.x - inset_);
    const std::size_t column =
        std::min(line.text.size(), static_cast<std::size_t>(local_x / char_width_));
    return line.start + column;
}

BPoint PaigeEditorView::PointForOffset(std::size_t offset) const {
    const auto lines = BuildLines();
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        const std::size_t line_end = line.start + line.text.size();
        if (offset <= line_end || index + 1 == lines.size()) {
            const std::size_t column = std::min(line.text.size(), offset - line.start);
            return BPoint(inset_ + char_width_ * static_cast<float>(column),
                          inset_ + static_cast<float>(index) * line_height_);
        }
    }
    return BPoint(inset_, inset_);
}

void PaigeEditorView::SetCaret(std::size_t offset) {
    surface_.SetSelection({offset, 0});
    if (selection_change_callback_) {
        selection_change_callback_();
    }
    ScrollSelectionIntoView();
    Invalidate();
}

void PaigeEditorView::MoveCaretHorizontal(int delta) {
    const auto document = surface_.Snapshot();
    const auto selection = surface_.Selection();
    if (selection.length != 0 && delta != 0) {
        SetCaret(delta < 0 ? selection.start : selection.start + selection.length);
        return;
    }

    const std::size_t next =
        delta < 0 ? (selection.start == 0 ? 0 : selection.start - 1)
                  : std::min(document.plain_text.size(), selection.start + 1);
    SetCaret(next);
}

void PaigeEditorView::MoveCaretVertical(int delta) {
    const auto lines = BuildLines();
    const auto selection = surface_.Selection();
    const BPoint point = PointForOffset(selection.start);
    const std::size_t current_line = std::min(
        lines.size() - 1,
        static_cast<std::size_t>(std::max(0.0f, point.y - inset_) / line_height_));
    const std::size_t target_line =
        delta < 0 ? (current_line == 0 ? 0 : current_line - 1)
                  : std::min(lines.size() - 1, current_line + 1);
    const auto& current = lines[current_line];
    const auto& target = lines[target_line];
    const std::size_t column = std::min(selection.start - current.start, target.text.size());
    SetCaret(target.start + column);
}

void PaigeEditorView::InsertText(std::string_view text) {
    if (text.empty()) {
        return;
    }
    if (surface_.ReplaceSelection(text)) {
        NotifyEdited();
    }
}

void PaigeEditorView::DeleteBackward() {
    auto selection = surface_.Selection();
    if (selection.length == 0) {
        if (selection.start == 0) {
            return;
        }
        surface_.SetSelection({selection.start - 1, 1});
    }
    if (surface_.ReplaceSelection("")) {
        NotifyEdited();
    }
}

void PaigeEditorView::DeleteForward() {
    auto selection = surface_.Selection();
    const auto document = surface_.Snapshot();
    if (selection.length == 0) {
        if (selection.start >= document.plain_text.size()) {
            return;
        }
        surface_.SetSelection({selection.start, 1});
    }
    if (surface_.ReplaceSelection("")) {
        NotifyEdited();
    }
}

void PaigeEditorView::CopySelectionToClipboard() const {
    const std::string copied = surface_.CopySelection();
    if (copied.empty() || be_clipboard == nullptr) {
        return;
    }

    if (be_clipboard->Lock()) {
        be_clipboard->Clear();
        if (BMessage* data = be_clipboard->Data()) {
            data->AddData(kClipboardMime, B_MIME_TYPE, copied.data(), copied.size());
        }
        be_clipboard->Commit();
        be_clipboard->Unlock();
    }
}

bool PaigeEditorView::PasteFromClipboard() {
    if (be_clipboard == nullptr || !be_clipboard->Lock()) {
        return false;
    }

    const void* data = nullptr;
    ssize_t size = 0;
    bool pasted = false;
    if (const BMessage* clip = be_clipboard->Data();
        clip != nullptr && clip->FindData(kClipboardMime, B_MIME_TYPE, &data, &size) == B_OK &&
        data != nullptr && size > 0) {
        pasted = surface_.Paste(std::string_view(static_cast<const char*>(data), static_cast<std::size_t>(size)));
    }
    be_clipboard->Unlock();
    return pasted;
}

void PaigeEditorView::DrawSelection(const std::vector<LayoutLine>& lines) {
    const auto selection = surface_.Selection();
    if (selection.length == 0) {
        return;
    }

    SetHighColor(180, 215, 255, 255);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        const std::size_t line_end = line.start + line.text.size();
        const std::size_t selection_end = selection.start + selection.length;
        if (selection.start > line_end || selection_end < line.start) {
            continue;
        }

        const std::size_t start = std::max(selection.start, line.start);
        const std::size_t end = std::min(selection_end, line_end);
        const float start_x = inset_ + char_width_ * static_cast<float>(start - line.start);
        const float end_x = inset_ + char_width_ * static_cast<float>(end - line.start);
        BRect rect = LineRect(inset_, line_height_, index, Bounds().Width());
        rect.left = start_x;
        rect.right = std::max(start_x + char_width_, end_x);
        FillRect(rect);
    }
}

void PaigeEditorView::DrawDiagnostics(const std::vector<LayoutLine>& lines) {
    for (const auto& diagnostic : surface_.Diagnostics()) {
        if (diagnostic.length == 0) {
            continue;
        }

        SetHighColor(DiagnosticColor(diagnostic.kind));
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto& line = lines[index];
            const std::size_t line_end = line.start + line.text.size();
            const std::size_t diagnostic_end = diagnostic.start + diagnostic.length;
            if (diagnostic.start > line_end || diagnostic_end < line.start) {
                continue;
            }

            const std::size_t start = std::max(diagnostic.start, line.start);
            const std::size_t end = std::min(diagnostic_end, line_end);
            const float start_x = inset_ + char_width_ * static_cast<float>(start - line.start);
            const float end_x = inset_ + char_width_ * static_cast<float>(end - line.start);
            const float y = inset_ + static_cast<float>(index) * line_height_ + line_height_ - 2.0f;
            StrokeLine(BPoint(start_x, y), BPoint(std::max(start_x + char_width_, end_x), y));
        }
    }
}

void PaigeEditorView::DrawCaret() {
    if (!IsFocus()) {
        return;
    }

    const auto selection = surface_.Selection();
    if (selection.length != 0) {
        return;
    }

    SetHighUIColor(B_CONTROL_TEXT_COLOR);
    const BPoint point = PointForOffset(selection.start);
    StrokeLine(BPoint(point.x, point.y + 2.0f), BPoint(point.x, point.y + line_height_ - 2.0f));
}

std::unique_ptr<ComposeEditorHost> CreatePaigeEditorHost(hermes::PaigeRichTextSurface& surface) {
    return std::make_unique<PaigeEditorHost>(surface);
}

}  // namespace hemera::haiku
