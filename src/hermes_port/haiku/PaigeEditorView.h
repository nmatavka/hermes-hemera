#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <View.h>

#include "ComposeEditorHost.h"
#include "hermes/PaigeRichTextSurface.h"

namespace hemera::haiku {

class PaigeEditorView final : public BView {
public:
    explicit PaigeEditorView(hermes::PaigeRichTextSurface& surface);
    ~PaigeEditorView() override;

    void SetChangeCallback(std::function<void()> callback);
    void SetSelectionChangeCallback(std::function<void()> callback);
    void SetTabNavigationCallback(std::function<bool(bool shift)> callback);
    void ReloadFromSurface();
    void ScrollSelectionIntoView();
    bool SelectAllText();
    bool CopySelection();
    bool CutSelection();
    bool Paste();

    BSize MinSize() override;
    BSize PreferredSize() override;
    void Draw(BRect update_rect) override;
    void MouseDown(BPoint where) override;
    void MouseMoved(BPoint where, uint32 transit, const BMessage* drag_message) override;
    void MouseUp(BPoint where) override;
    void KeyDown(const char* bytes, int32 num_bytes) override;
    void MakeFocus(bool focus = true) override;
    void FrameResized(float width, float height) override;

private:
    struct LayoutLine {
        std::size_t start = 0;
        std::string text;
    };

    void NotifyEdited();
    void UpdateMetrics();
    void ResizeToDocument();
    std::vector<LayoutLine> BuildLines() const;
    std::size_t OffsetForPoint(BPoint where) const;
    BPoint PointForOffset(std::size_t offset) const;
    void SetCaret(std::size_t offset);
    void MoveCaretHorizontal(int delta);
    void MoveCaretVertical(int delta);
    void InsertText(std::string_view text);
    void DeleteBackward();
    void DeleteForward();
    void CopySelectionToClipboard() const;
    bool PasteFromClipboard();
    void DrawSelection(const std::vector<LayoutLine>& lines);
    void DrawDiagnostics(const std::vector<LayoutLine>& lines);
    void DrawCaret();

    hermes::PaigeRichTextSurface& surface_;
    std::function<void()> change_callback_;
    std::function<void()> selection_change_callback_;
    std::function<bool(bool shift)> tab_navigation_callback_;
    bool dragging_selection_ = false;
    std::size_t drag_anchor_ = 0;
    float line_height_ = 18.0f;
    float char_width_ = 8.0f;
    float inset_ = 8.0f;
};

std::unique_ptr<ComposeEditorHost> CreatePaigeEditorHost(hermes::PaigeRichTextSurface& surface);

}  // namespace hemera::haiku
