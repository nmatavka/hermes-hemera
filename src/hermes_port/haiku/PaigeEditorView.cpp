#include "PaigeEditorView.h"

#include <algorithm>
#include <cmath>

#include <Application.h>
#include <Clipboard.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <ScrollBar.h>
#include <String.h>

namespace hermes::haiku_port {

namespace {

constexpr const char* kClipboardMime = "text/plain";

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

}  // namespace

PaigeEditorView::PaigeEditorView(hermes::PaigeRichTextSurface& surface)
    : BView("paige-editor", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE | B_FULL_UPDATE_ON_RESIZE),
      surface_(surface) {
    SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetFont(be_fixed_font);
    UpdateMetrics();
    ResizeToDocument();
}

void PaigeEditorView::SetChangeCallback(std::function<void()> callback) {
    change_callback_ = std::move(callback);
}

void PaigeEditorView::ReloadFromSurface() {
    UpdateMetrics();
    ResizeToDocument();
    Invalidate();
}

void PaigeEditorView::ScrollSelectionIntoView() {
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
    const auto lines = BuildLines();
    float widest = 240.0f;
    for (const auto& line : lines) {
        widest = std::max(widest, inset_ * 2.0f + char_width_ * static_cast<float>(line.text.size() + 1));
    }
    return BSize(widest, inset_ * 2.0f + static_cast<float>(lines.size()) * line_height_);
}

void PaigeEditorView::Draw(BRect update_rect) {
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
    drag_anchor_ = OffsetForPoint(where);
    surface_.SetSelection({drag_anchor_, 0});
    Invalidate();
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
}

void PaigeEditorView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag_message) {
    (void)transit;
    (void)drag_message;
    if (!dragging_selection_) {
        return;
    }

    const std::size_t current = OffsetForPoint(where);
    const std::size_t start = std::min(drag_anchor_, current);
    const std::size_t end = std::max(drag_anchor_, current);
    surface_.SetSelection({start, end - start});
    Invalidate();
}

void PaigeEditorView::MouseUp(BPoint where) {
    (void)where;
    dragging_selection_ = false;
}

void PaigeEditorView::KeyDown(const char* bytes, int32 num_bytes) {
    if (bytes == nullptr || num_bytes <= 0) {
        BView::KeyDown(bytes, num_bytes);
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

}  // namespace hermes::haiku_port
