#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct NicknameEntry {
    std::string nickname;
    std::string full_name;
    std::vector<std::string> addresses;
    std::string notes;
    bool recipient_list = false;
    bool bp_list = false;
};

class NicknameStore {
public:
    virtual ~NicknameStore() = default;

    virtual bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const = 0;
    virtual std::vector<NicknameEntry> Entries() const = 0;
    virtual std::optional<NicknameEntry> FindNickname(std::string_view nickname) const = 0;
    virtual void AddOrReplace(const NicknameEntry& entry) = 0;
};

class FlatFileNicknameStore final : public NicknameStore {
public:
    bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const override;
    std::vector<NicknameEntry> Entries() const override;
    std::optional<NicknameEntry> FindNickname(std::string_view nickname) const override;
    void AddOrReplace(const NicknameEntry& entry) override;

private:
    static std::string Normalize(std::string_view value);

    std::vector<NicknameEntry> entries_;
};

}  // namespace hermes
