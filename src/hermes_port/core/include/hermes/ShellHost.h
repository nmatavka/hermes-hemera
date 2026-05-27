#pragma once

#include <string_view>

#include "hermes/ComposeMessage.h"

namespace hermes {

class ShellHost {
public:
    virtual ~ShellHost() = default;

    virtual int Run() = 0;
    virtual bool OpenMailbox(std::string_view mailbox_id) = 0;
    virtual bool OpenComposer(const ComposeMessage& message) = 0;
    virtual bool SendQueued() = 0;
    virtual bool CheckMail() = 0;
    virtual bool SendAndReceive() = 0;
    virtual bool StopActiveTasks() = 0;
};

}  // namespace hermes
