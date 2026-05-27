#include "hermes/DirectoryServiceCatalog.h"

#include <algorithm>
#include <cctype>

#include "hermes/AddressBookService.h"
#include "hermes/NicknameStore.h"

namespace hermes {

namespace {

std::string Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

bool ContainsInsensitive(std::string_view haystack, std::string_view needle) {
    return Normalize(haystack).find(Normalize(needle)) != std::string::npos;
}

}  // namespace

LocalDirectoryServiceCatalog::LocalDirectoryServiceCatalog(const NicknameStore* nickname_store,
                                                           const AddressBookService* address_book_service)
    : nickname_store_(nickname_store),
      address_book_service_(address_book_service) {}

std::vector<DirectoryProviderInfo> LocalDirectoryServiceCatalog::Providers() const {
    std::vector<DirectoryProviderInfo> providers;
    if (nickname_store_ != nullptr) {
        providers.push_back({"nicknames", "Nicknames"});
    }
    if (address_book_service_ != nullptr) {
        providers.push_back({"address-book", "Address Book"});
    }
    return providers;
}

std::vector<DirectoryEntry> LocalDirectoryServiceCatalog::Search(std::string_view query) const {
    std::vector<DirectoryEntry> results;
    if (query.empty()) {
        return results;
    }

    if (nickname_store_ != nullptr) {
        for (const auto& nickname : nickname_store_->Entries()) {
            const bool match = ContainsInsensitive(nickname.nickname, query) ||
                               ContainsInsensitive(nickname.full_name, query) ||
                               std::any_of(nickname.addresses.begin(),
                                           nickname.addresses.end(),
                                           [&](const std::string& address) {
                                               return ContainsInsensitive(address, query);
                                           });
            if (!match) {
                continue;
            }
            for (const auto& address : nickname.addresses) {
                results.push_back({"nicknames", nickname.full_name.empty() ? nickname.nickname : nickname.full_name, address, nickname.notes});
            }
        }
    }

    if (address_book_service_ != nullptr) {
        for (const auto& entry : address_book_service_->Entries()) {
            if (!ContainsInsensitive(entry.display_name, query) &&
                !ContainsInsensitive(entry.email_address, query) &&
                !ContainsInsensitive(entry.notes, query)) {
                continue;
            }
            results.push_back({"address-book", entry.display_name, entry.email_address, entry.notes});
        }
    }

    return results;
}

}  // namespace hermes
