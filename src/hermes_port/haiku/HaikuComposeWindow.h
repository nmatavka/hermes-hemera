#pragma once

#include <Window.h>

#include "hermes/ComposeMessage.h"

class BTextControl;
class BTextView;

namespace hermes::haiku_port {

class HaikuComposeWindow final : public BWindow {
public:
    explicit HaikuComposeWindow(const ComposeMessage& message);

    bool QuitRequested() override;

private:
    void PopulateMessage(const ComposeMessage& message);

    BTextControl* to_control_ = nullptr;
    BTextControl* cc_control_ = nullptr;
    BTextControl* bcc_control_ = nullptr;
    BTextControl* subject_control_ = nullptr;
    BTextControl* persona_control_ = nullptr;
    BTextControl* reply_to_control_ = nullptr;
    BTextView* body_view_ = nullptr;
};

}  // namespace hermes::haiku_port
