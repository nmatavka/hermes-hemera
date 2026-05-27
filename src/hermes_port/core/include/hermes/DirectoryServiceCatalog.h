#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hermes {

class AddressBookService;
class NicknameStore;

struct DirectoryProviderInfo {
    std::string id;
    std::string display_name;
};

struct DirectoryEntry {
    std::string provider_id;
    std::string display_name;
    std::string email_address;
    std::string notes;
};

class DirectoryServiceCatalog {
public:
    virtual ~DirectoryServiceCatalog() = default;

    virtual std::vector<DirectoryProviderInfo> Providers() const = 0;
    virtual std::vector<DirectoryEntry> Search(std::string_view query) const = 0;
};

class LocalDirectoryServiceCatalog final : public DirectoryServiceCatalog {
public:
    LocalDirectoryServiceCatalog(const NicknameStore* nickname_store,
                                 const AddressBookService* address_book_service);

    std::vector<DirectoryProviderInfo> Providers() const override;
    std::vector<DirectoryEntry> Search(std::string_view query) const override;

private:
    const NicknameStore* nickname_store_ = nullptr;
    const AddressBookService* address_book_service_ = nullptr;
};

}  // namespace hermes
