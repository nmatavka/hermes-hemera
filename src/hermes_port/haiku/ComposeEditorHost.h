#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class BView;

namespace hemera::haiku {

enum class ComposeEditorCommand {
    kPlain,
    kBold,
    kItalic,
    kUnderline,
    kStrikeout,
    kFixedWidth,
    kAddQuote,
    kRemoveQuote,
    kIndentIn,
    kIndentOut,
    kNormalMargins,
    kAlignLeft,
    kAlignCenter,
    kAlignRight,
    kBulletedList,
    kInsertLink,
    kClearFormatting,
    kTextColor,
    kTextSize,
    kFormatPainter,
    kInsertDownloadablePicture,
    kInsertHorizontalRule,
    kWrapSelection,
};

struct ComposeEditorCommandState {
    bool enabled = false;
    bool checked = false;
    bool indeterminate = false;
};

enum class ComposeEditorAlignment {
    kLeft,
    kCenter,
    kRight,
};

struct ComposeEditorStyleSnapshot {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
    bool fixed_width = false;
    bool include_paragraph_style = false;
    bool bulleted_list = false;
    std::optional<std::string> text_color;
    std::optional<std::string> text_size;
    std::optional<ComposeEditorAlignment> alignment;
    std::optional<int> left_indent;

    bool Empty() const {
        return !bold && !italic && !underline && !strikeout && !fixed_width &&
               !include_paragraph_style && !bulleted_list && !text_color.has_value() &&
               !text_size.has_value() && !alignment.has_value() && !left_indent.has_value();
    }
};

class ComposeEditorHost {
public:
    virtual ~ComposeEditorHost() = default;

    virtual BView* RootView() const = 0;
    virtual void SetChangeCallback(std::function<void()> callback) = 0;
    virtual void SetSelectionChangeCallback(std::function<void()> callback) = 0;
    virtual void ReloadFromSurface() = 0;
    virtual void ScrollSelectionIntoView() = 0;
    virtual void SetTabNavigationCallback(std::function<bool(bool shift)> callback) = 0;
    virtual bool SelectAllText() = 0;
    virtual bool CopySelection() = 0;
    virtual bool CutSelection() = 0;
    virtual bool Paste() = 0;
    virtual bool SupportsCommand(ComposeEditorCommand command) const = 0;
    virtual ComposeEditorCommandState CommandState(ComposeEditorCommand command) const = 0;
    virtual bool ExecuteCommand(ComposeEditorCommand command) = 0;
    virtual std::string SelectedText() const = 0;
    virtual bool InsertLink(std::string_view url) = 0;
    virtual bool ApplyTextColor(std::string_view color) = 0;
    virtual bool ApplyTextSize(std::string_view size) = 0;
    virtual bool InsertDownloadablePicture(std::string_view url, std::string_view alt_text) = 0;
    virtual bool InsertHorizontalRule() = 0;
    virtual bool WrapSelection(std::size_t columns) = 0;
    virtual std::optional<ComposeEditorStyleSnapshot> CaptureStyleSnapshot() const = 0;
    virtual bool ApplyStyleSnapshot(const ComposeEditorStyleSnapshot& snapshot) = 0;
    virtual void MakeEditorFocus() = 0;
    virtual void InvalidateEditor() = 0;
};

}  // namespace hemera::haiku
