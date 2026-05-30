#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <GroupView.h>

#include "ComposeEditorHost.h"
#include "hermes/MessageRenderer.h"
#include "hermes/PaigeRuntime.h"
#include "hermes/RichTextSurface.h"

class BWebView;

namespace hemera::haiku {

class HaikuWebKitRichTextSurface final : public hermes::RichTextSurface {
public:
    HaikuWebKitRichTextSurface(hermes::PaigeRuntime& runtime,
                               std::filesystem::path cache_root,
                               std::string cache_key);
    ~HaikuWebKitRichTextSurface() override;

    bool Load(const hermes::RichTextDocument& document) override;
    hermes::RichTextDocument Snapshot() const override;
    bool SetSelection(const hermes::TextSelection& selection) override;
    hermes::TextSelection Selection() const override;
    bool ReplaceSelection(std::string_view replacement) override;
    bool Undo() override;
    bool Redo() override;
    bool SelectAll() override;
    std::string CopySelection() const override;
    std::string CutSelection() override;
    bool Paste(std::string_view text = {}) override;
    void SetDiagnostics(std::vector<hermes::TextDiagnostic> diagnostics) override;
    void ClearDiagnostics() override;
    const std::vector<hermes::TextDiagnostic>& Diagnostics() const override;
    bool RevealSelection(const hermes::TextSelection& selection) override;

    void AttachWebView(BWebView* web_view);
    void DetachWebView();

private:
    bool LoadIntoBoundView();
    std::filesystem::path RenderDirectory() const;

    hermes::PaigeRuntime& runtime_;
    std::filesystem::path cache_root_;
    std::string cache_key_;
    hermes::RichTextDocument document_;
    hermes::TextSelection selection_;
    std::vector<hermes::TextDiagnostic> diagnostics_;
    BWebView* web_view_ = nullptr;
};

class HaikuWebKitMessageView final : public BGroupView, public hermes::MessageRenderer {
public:
    explicit HaikuWebKitMessageView(std::filesystem::path cache_root);
    ~HaikuWebKitMessageView() override;

    bool Load(const hermes::MessageRenderRequest& request) override;
    hermes::RendererMode Mode() const override;
    hermes::FindResult Find(std::string_view needle, const hermes::FindOptions& options) override;
    std::string AllText() const override;
    std::string AllHtml() const override;
    bool CanPrint() const override;
    bool CanDirectPrint() const override;
    bool PrintPreview() override;
    bool DirectPrint() override;
    std::string SelectedText() const;
    void SetContextMenuHandler(std::function<void(BPoint)> handler);

private:
    std::filesystem::path RenderDirectoryForRequest(const hermes::MessageRenderRequest& request) const;
    bool EnsurePrintArtifacts(std::filesystem::path* preview_path,
                              std::filesystem::path* printable_path) const;

    std::filesystem::path cache_root_;
    BWebView* web_view_ = nullptr;
    hermes::MessageRenderRequest request_;
};

std::unique_ptr<ComposeEditorHost> CreateWebKitComposeEditorHost(HaikuWebKitRichTextSurface& surface);

}  // namespace hemera::haiku
