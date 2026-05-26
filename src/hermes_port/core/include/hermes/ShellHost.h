#pragma once

#include <string_view>

#include "hermes/RichTextSurface.h"

namespace hermes {

class ShellHost {
public:
    virtual ~ShellHost() = default;

    virtual int Run() = 0;
    virtual bool OpenMailbox(std::string_view mailbox_id) = 0;
    virtual bool OpenComposer(const RichTextDocument& document) = 0;
};

}  // namespace hermes
