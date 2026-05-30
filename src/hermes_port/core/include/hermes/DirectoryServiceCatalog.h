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
    std::vector<std::string> email_addresses;
    std::string notes;
};

std::string DirectoryComposeAddress(const DirectoryEntry& entry);
std::string DirectoryComposeAddressList(const std::vector<DirectoryEntry>& entries);

class DirectoryServiceCatalog {
public:
    virtual ~DirectoryServiceCatalog() = default;

    virtual std::vector<DirectoryProviderInfo> Providers() const = 0;
    virtual std::vector<DirectoryEntry> Search(std::string_view query) const = 0;
    virtual std::vector<DirectoryEntry> SearchProvider(std::string_view provider_id,
                                                       std::string_view query) const = 0;
    virtual std::vector<DirectoryEntry> SearchProviders(const std::vector<std::string>& provider_ids,
                                                        std::string_view query) const = 0;
    virtual std::string LongDetailText(const DirectoryEntry& entry) const = 0;
    virtual std::string PrintableDetailText(const DirectoryEntry& entry) const = 0;
    virtual std::string LongDetailText(const std::vector<DirectoryEntry>& entries) const;
    virtual std::string LongDetailText(const std::vector<std::string>& provider_ids,
                                       std::string_view query,
                                       const std::vector<DirectoryEntry>& entries) const = 0;
    virtual std::string PrintableDetailText(const std::vector<DirectoryEntry>& entries) const;
    virtual std::string ComposeAddressText(const std::vector<DirectoryEntry>& entries) const;
    virtual std::string PrintableText(std::string_view provider_id,
                                      std::string_view query,
                                      const std::vector<DirectoryEntry>& entries) const = 0;
    virtual std::string PrintableText(const std::vector<std::string>& provider_ids,
                                      std::string_view query,
                                      const std::vector<DirectoryEntry>& entries) const = 0;
};

class LocalDirectoryServiceCatalog final : public DirectoryServiceCatalog {
public:
    LocalDirectoryServiceCatalog(const NicknameStore* nickname_store,
                                 const AddressBookService* address_book_service);

    std::vector<DirectoryProviderInfo> Providers() const override;
    std::vector<DirectoryEntry> Search(std::string_view query) const override;
    std::vector<DirectoryEntry> SearchProvider(std::string_view provider_id,
                                               std::string_view query) const override;
    std::vector<DirectoryEntry> SearchProviders(const std::vector<std::string>& provider_ids,
                                                std::string_view query) const override;
    std::string LongDetailText(const DirectoryEntry& entry) const override;
    std::string PrintableDetailText(const DirectoryEntry& entry) const override;
    std::string LongDetailText(const std::vector<DirectoryEntry>& entries) const override;
    std::string LongDetailText(const std::vector<std::string>& provider_ids,
                               std::string_view query,
                               const std::vector<DirectoryEntry>& entries) const override;
    std::string PrintableDetailText(const std::vector<DirectoryEntry>& entries) const override;
    std::string ComposeAddressText(const std::vector<DirectoryEntry>& entries) const override;
    std::string PrintableText(std::string_view provider_id,
                              std::string_view query,
                              const std::vector<DirectoryEntry>& entries) const override;
    std::string PrintableText(const std::vector<std::string>& provider_ids,
                              std::string_view query,
                              const std::vector<DirectoryEntry>& entries) const override;

private:
    const NicknameStore* nickname_store_ = nullptr;
    const AddressBookService* address_book_service_ = nullptr;
};

}  // namespace hermes
