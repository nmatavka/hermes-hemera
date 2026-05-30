#include "hermes/PaigeRichTextSurface.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
#include <fcntl.h>
#include <unistd.h>
#include "Paige.h"
#endif

namespace hermes {

namespace {

std::string CopySelectedTextFromDocument(const RichTextDocument& document, const TextSelection& selection) {
    if (selection.start > document.plain_text.size()) {
        return {};
    }

    const std::size_t max_length = document.plain_text.size() - selection.start;
    return document.plain_text.substr(selection.start, std::min(selection.length, max_length));
}

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
rectangle NativeBounds(float width, float height) {
    rectangle bounds {};
    bounds.top_left.h = 0;
    bounds.top_left.v = 0;
    bounds.bot_right.h = static_cast<long>(width);
    bounds.bot_right.v = static_cast<long>(height);
    return bounds;
}

std::string ExtractNativeText(pg_ref document) {
    if (document == MEM_NULL) {
        return {};
    }

    select_pair selection {};
    selection.begin = 0;
    selection.end = pgTextSize(document);
    text_ref text_ref_handle = pgCopyText(document, &selection, all_data);
    if (text_ref_handle == MEM_NULL) {
        return {};
    }

    const auto* bytes = static_cast<const char*>(UseMemory(text_ref_handle));
    std::string text(bytes, bytes + GetByteSize(text_ref_handle));
    UnuseMemory(text_ref_handle);
    DisposeMemory(text_ref_handle);
    return text;
}

std::filesystem::path TempPath(std::string_view extension) {
    static std::uint64_t counter = 0;
    const auto base = std::filesystem::temp_directory_path() /
                      ("hermes-paige-" + std::to_string(++counter) + "." +
                       std::string(extension));
    return base;
}

bool WriteTempFile(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

std::string ReadTempFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
#endif

}  // namespace

PaigeRichTextSurface::PaigeRichTextSurface(PaigeRuntime& runtime) : runtime_(runtime) {}

PaigeRichTextSurface::~PaigeRichTextSurface() {
    ReleaseNativeDocument();
}

bool PaigeRichTextSurface::Load(const RichTextDocument& document) {
    document_ = NormalizeRichTextDocument(document);
    if (RequiresHtmlSurface(document_)) {
        document_.read_only = true;
    }
    selection_ = {};
    undo_stack_.clear();
    redo_stack_.clear();
    diagnostics_.clear();

    if (NativeBackendEnabled() && !LoadNativeDocument(document_)) {
        ReleaseNativeDocument();
    }

    return true;
}

RichTextDocument PaigeRichTextSurface::Snapshot() const {
    if (NativeBackendEnabled() && native_document_ != 0) {
        return NativeSnapshot();
    }
    return document_;
}

bool PaigeRichTextSurface::SetSelection(const TextSelection& selection) {
    if (NativeBackendEnabled() && native_document_ != 0) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
        const std::size_t document_size = pgTextSize(static_cast<pg_ref>(native_document_));
        if (selection.start > document_size) {
            return false;
        }
        const std::size_t max_length = document_size - selection.start;
        selection_.start = selection.start;
        selection_.length = std::min(selection.length, max_length);
        pgSetSelection(static_cast<pg_ref>(native_document_),
                       selection_.start,
                       selection_.start + selection_.length,
                       0,
                       FALSE);
        return true;
#endif
    }

    if (selection.start > document_.plain_text.size()) {
        return false;
    }

    const std::size_t max_length = document_.plain_text.size() - selection.start;
    selection_.start = selection.start;
    selection_.length = std::min(selection.length, max_length);
    return true;
}

TextSelection PaigeRichTextSurface::Selection() const {
    if (NativeBackendEnabled() && native_document_ != 0) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
        size_t begin = 0;
        size_t end = 0;
        pgGetSelection(static_cast<pg_ref>(native_document_), &begin, &end);
        return TextSelection{begin, end >= begin ? end - begin : 0};
#endif
    }
    return selection_;
}

bool PaigeRichTextSurface::ReplaceSelection(std::string_view replacement) {
    if (document_.read_only) {
        return false;
    }

    if (NativeBackendEnabled() && native_document_ != 0) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
        const TextSelection current_selection = Selection();
        undo_stack_.push_back(NativeSnapshot());
        redo_stack_.clear();

        select_pair range {};
        range.begin = current_selection.start;
        range.end = current_selection.start + current_selection.length;
        if (range.end > range.begin) {
            pgDelete(static_cast<pg_ref>(native_document_), &range, draw_none);
        }

        if (!replacement.empty()) {
            auto* bytes =
                const_cast<pg_char_ptr>(reinterpret_cast<const pg_char_ptr>(replacement.data()));
            pgInsert(static_cast<pg_ref>(native_document_),
                     bytes,
                     replacement.size(),
                     current_selection.start,
                     data_insert_mode,
                     0,
                     draw_none);
        }

        selection_ = {current_selection.start + replacement.size(), 0};
        pgSetSelection(static_cast<pg_ref>(native_document_),
                       selection_.start,
                       selection_.start,
                       0,
                       FALSE);
        document_ = NativeSnapshot();
        return true;
#endif
    }

    if (selection_.start > document_.plain_text.size()) {
        return false;
    }

    undo_stack_.push_back(document_);
    redo_stack_.clear();
    document_.plain_text.replace(selection_.start, selection_.length, replacement);
    selection_.length = replacement.size();
    SyncStyledBody();
    return true;
}

bool PaigeRichTextSurface::Undo() {
    if (undo_stack_.empty()) {
        return false;
    }

    redo_stack_.push_back(Snapshot());
    document_ = undo_stack_.back();
    undo_stack_.pop_back();
    selection_ = {};
    if (NativeBackendEnabled() && native_document_ != 0) {
        (void)LoadNativeDocument(document_);
    }
    return true;
}

bool PaigeRichTextSurface::Redo() {
    if (redo_stack_.empty()) {
        return false;
    }

    undo_stack_.push_back(Snapshot());
    document_ = redo_stack_.back();
    redo_stack_.pop_back();
    selection_ = {};
    if (NativeBackendEnabled() && native_document_ != 0) {
        (void)LoadNativeDocument(document_);
    }
    return true;
}

bool PaigeRichTextSurface::SelectAll() {
    if (NativeBackendEnabled() && native_document_ != 0) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
        const std::size_t size = pgTextSize(static_cast<pg_ref>(native_document_));
        selection_ = {0, size};
        pgSetSelection(static_cast<pg_ref>(native_document_), 0, size, 0, FALSE);
        return true;
#endif
    }

    selection_ = {0, document_.plain_text.size()};
    return true;
}

std::string PaigeRichTextSurface::CopySelection() const {
    return CopySelectedText();
}

std::string PaigeRichTextSurface::CutSelection() {
    if (document_.read_only) {
        return {};
    }

    clipboard_ = CopySelectedText();
    clipboard_document_ = {};
    clipboard_document_.plain_text = clipboard_;
    clipboard_document_.html_fragment = PlainTextToHtml(clipboard_);
    clipboard_document_.rtf_fragment = PlainTextToRtf(clipboard_);
    if (!clipboard_.empty()) {
        (void)ReplaceSelection("");
    }
    return clipboard_;
}

bool PaigeRichTextSurface::Paste(std::string_view text) {
    const std::string replacement = text.empty() ? clipboard_ : std::string(text);
    if (replacement.empty()) {
        return false;
    }
    clipboard_ = replacement;
    if (!text.empty()) {
        clipboard_document_ = {};
        clipboard_document_.plain_text = replacement;
        clipboard_document_.html_fragment = PlainTextToHtml(replacement);
        clipboard_document_.rtf_fragment = PlainTextToRtf(replacement);
    }
    return ReplaceSelection(replacement);
}

void PaigeRichTextSurface::SetDiagnostics(std::vector<TextDiagnostic> diagnostics) {
    diagnostics_ = std::move(diagnostics);
}

void PaigeRichTextSurface::ClearDiagnostics() {
    diagnostics_.clear();
}

const std::vector<TextDiagnostic>& PaigeRichTextSurface::Diagnostics() const {
    return diagnostics_;
}

bool PaigeRichTextSurface::RevealSelection(const TextSelection& selection) {
    return SetSelection(selection);
}

bool PaigeRichTextSurface::IsAvailable() const {
    return runtime_.IsAvailable();
}

bool PaigeRichTextSurface::NativeBackendEnabled() const {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE && HERMES_IS_HAIKU
    return runtime_.IsAvailable();
#else
    return false;
#endif
}

void* PaigeRichTextSurface::NativeDocumentHandle() const {
    return reinterpret_cast<void*>(native_document_);
}

#if HERMES_IS_HAIKU
void PaigeRichTextSurface::AttachNativeView(void* native_view) {
    native_view_ = native_view;
}

void PaigeRichTextSurface::DetachNativeView() {
    native_view_ = nullptr;
}

void PaigeRichTextSurface::ResizeNativeHost(float width, float height) {
    native_width_ = std::max(64.0f, width);
    native_height_ = std::max(64.0f, height);

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (!NativeBackendEnabled() || !EnsureNativeDocument()) {
        return;
    }

    const rectangle bounds = NativeBounds(native_width_, native_height_);
    pgSetAreaBounds(static_cast<pg_ref>(native_document_), &bounds, &bounds);
#endif
}

void PaigeRichTextSurface::DrawNative(void* target_view, float left, float top, float right, float bottom) {
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (!NativeBackendEnabled() || !EnsureNativeDocument() || target_view == nullptr) {
        return;
    }

    auto* globals = static_cast<pg_globals_ptr>(runtime_.NativeGlobals());
    graf_device device {};
    const auto raw_view = reinterpret_cast<intptr_t>(target_view);
    pgInitDevice(globals, static_cast<generic_var>(raw_view), static_cast<size_t>(raw_view), &device);
    pgSetDefaultDevice(static_cast<pg_ref>(native_document_), &device);
    pgDisplay(static_cast<pg_ref>(native_document_),
              &device,
              pgGetVisArea(static_cast<pg_ref>(native_document_)),
              pgGetPageArea(static_cast<pg_ref>(native_document_)),
              nullptr,
              best_way);
    pgCloseDevice(globals, &device);
#endif
}

std::size_t PaigeRichTextSurface::NativeOffsetForPoint(float x, float y) const {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (!NativeBackendEnabled() || native_document_ == 0) {
        return Selection().start;
    }

    co_ordinate point {};
    point.h = static_cast<long>(x);
    point.v = static_cast<long>(y);
    return pgPtToChar(static_cast<pg_ref>(native_document_), &point, nullptr);
#else
    (void)x;
    (void)y;
    return Selection().start;
#endif
}

bool PaigeRichTextSurface::NativeCaretRect(std::size_t offset,
                                           float* left,
                                           float* top,
                                           float* right,
                                           float* bottom) const {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (!NativeBackendEnabled() || native_document_ == 0) {
        return false;
    }

    rectangle caret {};
    if (!pgCaretPosition(static_cast<pg_ref>(native_document_), offset, &caret)) {
        return false;
    }

    if (left != nullptr) {
        *left = static_cast<float>(caret.top_left.h);
    }
    if (top != nullptr) {
        *top = static_cast<float>(caret.top_left.v);
    }
    if (right != nullptr) {
        *right = static_cast<float>(caret.bot_right.h);
    }
    if (bottom != nullptr) {
        *bottom = static_cast<float>(caret.bot_right.v);
    }
    return true;
#else
    (void)offset;
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    return false;
#endif
}
#endif

bool PaigeRichTextSurface::EnsureNativeDocument() {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE && HERMES_IS_HAIKU
    if (native_document_ != 0) {
        return true;
    }
    if (!runtime_.IsAvailable()) {
        return false;
    }

    auto* globals = static_cast<pg_globals_ptr>(runtime_.NativeGlobals());
    auto* mem_globals = static_cast<pgm_globals_ptr>(runtime_.NativeMemoryGlobals());
    const rectangle bounds = NativeBounds(native_width_, native_height_);
    shape_ref vis = pgRectToShape(mem_globals, &bounds);
    shape_ref page = pgRectToShape(mem_globals, &bounds);
    native_document_ = static_cast<std::uintptr_t>(pgNew(globals,
                                                         USE_NO_DEVICE,
                                                         vis,
                                                         page,
                                                         MEM_NULL,
                                                         NO_DEFAULT_LEADING | NO_LF_BIT |
                                                             COUNT_LINES_BIT | SMART_QUOTES_BIT));
    pgDisposeShape(vis);
    pgDisposeShape(page);
    return native_document_ != 0;
#else
    return false;
#endif
}

void PaigeRichTextSurface::ReleaseNativeDocument() {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (native_document_ != 0) {
        pgDispose(static_cast<pg_ref>(native_document_));
        native_document_ = 0;
    }
#endif
}

RichTextDocument PaigeRichTextSurface::NativeSnapshot() const {
    RichTextDocument snapshot = document_;
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    if (native_document_ == 0) {
        return snapshot;
    }
    snapshot.plain_text = ExtractNativeText(static_cast<pg_ref>(native_document_));
    auto export_document = [&](pg_filetype file_type, std::string_view extension) -> std::string {
        const auto path = TempPath(extension);
        const int fd = ::open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
        if (fd < 0) {
            return {};
        }
        const pg_error error =
            pgExportFileFromC(static_cast<pg_ref>(native_document_), file_type, EXPORT_EVERYTHING_FLAG, 0, nullptr, FALSE, fd);
        ::close(fd);
        const std::string exported = error == NO_ERROR ? ReadTempFile(path) : std::string();
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return exported;
    };
    snapshot.paige_native_bytes = export_document(pg_paige_type, "pg");
    snapshot.html_fragment = export_document(pg_html_type, "html");
    snapshot.rtf_fragment = export_document(pg_rtf_type, "rtf");
    snapshot.styled_source =
        !snapshot.paige_native_bytes.empty() ? StyledDocumentSource::kPaigeNative : BestAvailableSource(snapshot);
    snapshot.fidelity = ClassifyStyledDocument(snapshot);
#endif
    return NormalizeRichTextDocument(snapshot);
}

bool PaigeRichTextSurface::LoadNativeDocument(const RichTextDocument& document) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE && HERMES_IS_HAIKU
    if (!EnsureNativeDocument()) {
        return false;
    }

    pg_ref native_ref = static_cast<pg_ref>(native_document_);
    select_pair selection {};
    selection.begin = 0;
    selection.end = pgTextSize(native_ref);
    if (selection.end > 0) {
        pgDelete(native_ref, &selection, draw_none);
    }

    const RichTextDocument normalized = NormalizeRichTextDocument(document);
    auto import_document = [&](pg_filetype file_type, std::string_view extension, const std::string& contents) -> bool {
        if (contents.empty()) {
            return false;
        }
        const auto path = TempPath(extension);
        if (!WriteTempFile(path, contents)) {
            return false;
        }
        const int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            return false;
        }
        const long import_flags = IMPORT_EVERYTHING_FLAG | IMPORT_BKCOLOR_FLAG | IMPORT_HYPERTEXT_FLAG | IMPORT_TABLES_FLAG;
        const pg_error error = pgImportFileFromC(native_ref, file_type, import_flags, 0, fd);
        ::close(fd);
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return error == NO_ERROR;
    };

    const bool imported =
        (!normalized.paige_native_bytes.empty() &&
         import_document(pg_paige_type, "pg", normalized.paige_native_bytes)) ||
        (!RequiresHtmlSurface(normalized) && !normalized.html_fragment.empty() &&
         import_document(pg_html_type, "html", normalized.html_fragment)) ||
        (!RequiresHtmlSurface(normalized) && !normalized.rtf_fragment.empty() &&
         import_document(pg_rtf_type, "rtf", normalized.rtf_fragment));

    if (!imported && !normalized.plain_text.empty()) {
        auto* bytes = const_cast<pg_char_ptr>(
            reinterpret_cast<const pg_char_ptr>(normalized.plain_text.data()));
        pgInsert(native_ref, bytes, normalized.plain_text.size(), 0, data_insert_mode, 0, draw_none);
    }

    const long attributes = pgGetAttributes(native_ref);
    pgSetAttributes(native_ref,
                    normalized.read_only ? (attributes | NO_EDIT_BIT) : (attributes & ~NO_EDIT_BIT));
    pgSetSelection(native_ref, 0, 0, 0, FALSE);
    ResizeNativeHost(native_width_, native_height_);
    return true;
#else
    (void)document;
    return false;
#endif
}

std::string PaigeRichTextSurface::CopySelectedText() const {
    if (NativeBackendEnabled() && native_document_ != 0) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
        const TextSelection selection = Selection();
        select_pair range {};
        range.begin = selection.start;
        range.end = selection.start + selection.length;
        text_ref text_ref_handle = pgCopyText(static_cast<pg_ref>(native_document_), &range, all_data);
        if (text_ref_handle == MEM_NULL) {
            return {};
        }

        const auto* bytes = static_cast<const char*>(UseMemory(text_ref_handle));
        std::string text(bytes, bytes + GetByteSize(text_ref_handle));
        UnuseMemory(text_ref_handle);
        DisposeMemory(text_ref_handle);
        return text;
#endif
    }

    return CopySelectedTextFromDocument(document_, selection_);
}

void PaigeRichTextSurface::SyncStyledBody() {
    const bool had_structured_content = HasAuthenticStyledContent(document_);
    document_.paige_native_bytes.clear();
    if (had_structured_content) {
        document_.html_fragment = PlainTextToHtml(document_.plain_text);
        document_.rtf_fragment = PlainTextToRtf(document_.plain_text);
        if (document_.styled_source == StyledDocumentSource::kPaigeNative) {
            document_.styled_source = StyledDocumentSource::kHtml;
        }
        document_.fidelity = StyledDocumentFidelity::kLossy;
    } else {
        document_.html_fragment.clear();
        document_.rtf_fragment.clear();
        document_.styled_source = StyledDocumentSource::kPlainText;
        document_.fidelity = StyledDocumentFidelity::kLossless;
    }
}

}  // namespace hermes
