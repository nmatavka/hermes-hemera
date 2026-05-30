#pragma once

#include <string>
#include <string_view>

#include <Messenger.h>
#include <Window.h>

class BTextControl;

namespace hemera::haiku {

bool HasSharedFindQuery();
std::string SharedFindQuery();
void SetSharedFindQuery(std::string_view query);

class HaikuFindWindow final : public BWindow {
public:
    HaikuFindWindow(BMessenger target,
                    uint32 confirm_what,
                    uint32 closed_what,
                    const char* title = "Find");

    bool QuitRequested() override;
    void MessageReceived(BMessage* message) override;

    void SetQuery(std::string_view query);
    std::string Query() const;
    void FocusQuery();

private:
    BMessenger target_;
    uint32 confirm_what_ = 0;
    uint32 closed_what_ = 0;
    BTextControl* field_ = nullptr;
};

}  // namespace hemera::haiku
