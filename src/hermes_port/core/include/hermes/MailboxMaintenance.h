#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace hermes::eudora {

bool CompactMailbox(const std::filesystem::path& root_directory,
                    std::string_view mailbox_id,
                    std::string* error_message = nullptr);

bool CompactAllMailboxes(const std::filesystem::path& root_directory,
                         std::string* error_message = nullptr);

}  // namespace hermes::eudora
