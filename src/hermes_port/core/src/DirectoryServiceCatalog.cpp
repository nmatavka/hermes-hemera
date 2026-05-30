#include "hermes/DirectoryServiceCatalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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

std::string ProviderDisplayName(std::string_view provider_id) {
    if (provider_id == "nicknames") {
        return "Nicknames";
    }
    if (provider_id == "address-book") {
        return "Address Book";
    }
    return std::string(provider_id);
}

std::string JoinStrings(const std::vector<std::string>& values, std::string_view separator);

std::vector<std::string> NormalizeProviderIds(const std::vector<std::string>& provider_ids) {
    std::vector<std::string> normalized;
    for (const auto& provider_id : provider_ids) {
        if (provider_id.empty()) {
            continue;
        }
        if (std::find(normalized.begin(), normalized.end(), provider_id) == normalized.end()) {
            normalized.push_back(provider_id);
        }
    }
    return normalized;
}

std::string ProviderSelectionLabel(const std::vector<std::string>& provider_ids) {
    const auto normalized = NormalizeProviderIds(provider_ids);
    if (normalized.empty()) {
        return "No providers selected";
    }
    std::vector<std::string> labels;
    labels.reserve(normalized.size());
    for (const auto& provider_id : normalized) {
        labels.push_back(ProviderDisplayName(provider_id));
    }
    return JoinStrings(labels, ", ");
}

std::vector<std::string> EntryAddresses(const DirectoryEntry& entry) {
    if (!entry.email_addresses.empty()) {
        return entry.email_addresses;
    }
    if (!entry.email_address.empty()) {
        return {entry.email_address};
    }
    return {};
}

std::string ComposeNamedAddress(std::string_view display_name, std::string_view address) {
    if (address.empty()) {
        return {};
    }
    if (display_name.empty()) {
        return std::string(address);
    }
    return std::string(display_name) + " <" + std::string(address) + ">";
}

std::string JoinStrings(const std::vector<std::string>& values, std::string_view separator) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << separator;
        }
        output << values[index];
    }
    return output.str();
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
            const std::string display_name =
                nickname.full_name.empty() ? nickname.nickname : nickname.full_name;
            results.push_back({"nicknames",
                               display_name,
                               nickname.addresses.empty() ? std::string() : nickname.addresses.front(),
                               nickname.addresses,
                               nickname.notes});
        }
    }

    if (address_book_service_ != nullptr) {
        for (const auto& entry : address_book_service_->Entries()) {
            if (!ContainsInsensitive(entry.display_name, query) &&
                !ContainsInsensitive(entry.email_address, query) &&
                !ContainsInsensitive(entry.notes, query)) {
                continue;
            }
            results.push_back({"address-book",
                               entry.display_name,
                               entry.email_address,
                               {entry.email_address},
                               entry.notes});
        }
    }

    return results;
}

std::vector<DirectoryEntry> LocalDirectoryServiceCatalog::SearchProvider(std::string_view provider_id,
                                                                         std::string_view query) const {
    const auto results = Search(query);
    if (provider_id.empty()) {
        return results;
    }

    std::vector<DirectoryEntry> filtered;
    for (const auto& entry : results) {
        if (entry.provider_id == provider_id) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::vector<DirectoryEntry> LocalDirectoryServiceCatalog::SearchProviders(
    const std::vector<std::string>& provider_ids,
    std::string_view query) const {
    const auto normalized = NormalizeProviderIds(provider_ids);
    if (normalized.empty() || query.empty()) {
        return {};
    }

    const auto results = Search(query);
    std::vector<DirectoryEntry> filtered;
    for (const auto& entry : results) {
        if (std::find(normalized.begin(), normalized.end(), entry.provider_id) != normalized.end()) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

std::string LocalDirectoryServiceCatalog::LongDetailText(const DirectoryEntry& entry) const {
    std::ostringstream detail;
    const auto addresses = EntryAddresses(entry);
    detail << "Name: "
           << (entry.display_name.empty() ? entry.email_address : entry.display_name) << '\n'
           << "Address" << (addresses.size() == 1 ? "" : "es") << ": "
           << DirectoryComposeAddress(entry) << '\n'
           << "Provider: " << ProviderDisplayName(entry.provider_id);
    if (!entry.notes.empty()) {
        detail << '\n' << "Notes: " << entry.notes;
    }
    return detail.str();
}

std::string LocalDirectoryServiceCatalog::PrintableDetailText(const DirectoryEntry& entry) const {
    std::ostringstream detail;
    const auto addresses = EntryAddresses(entry);
    detail << "Directory Result\n"
           << "Name: "
           << (entry.display_name.empty() ? entry.email_address : entry.display_name) << '\n'
           << "Address" << (addresses.size() == 1 ? "" : "es") << ": "
           << DirectoryComposeAddress(entry) << '\n'
           << "Provider: " << ProviderDisplayName(entry.provider_id);
    if (!entry.notes.empty()) {
        detail << '\n' << '\n' << entry.notes;
    }
    return detail.str();
}

std::string DirectoryServiceCatalog::LongDetailText(const std::vector<DirectoryEntry>& entries) const {
    if (entries.empty()) {
        return "No directory results selected.";
    }
    if (entries.size() == 1) {
        return LongDetailText(entries.front());
    }
    std::ostringstream detail;
    detail << "Selected Results: " << entries.size() << '\n'
           << "Addresses: " << ComposeAddressText(entries);
    return detail.str();
}

std::string DirectoryServiceCatalog::PrintableDetailText(const std::vector<DirectoryEntry>& entries) const {
    if (entries.empty()) {
        return "No directory results selected.";
    }
    if (entries.size() == 1) {
        return PrintableDetailText(entries.front());
    }
    std::ostringstream detail;
    detail << "Directory Results\n"
           << "Selected Results: " << entries.size() << '\n'
           << "Addresses: " << ComposeAddressText(entries);
    return detail.str();
}

std::string DirectoryServiceCatalog::ComposeAddressText(const std::vector<DirectoryEntry>& entries) const {
    return DirectoryComposeAddressList(entries);
}

std::string LocalDirectoryServiceCatalog::LongDetailText(const std::vector<DirectoryEntry>& entries) const {
    if (entries.empty()) {
        return "Type a query, then select one or more directory results.";
    }
    if (entries.size() == 1) {
        return LongDetailText(entries.front());
    }

    std::ostringstream detail;
    detail << "Selected Results: " << entries.size() << '\n'
           << "Addresses: " << ComposeAddressText(entries);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        detail << "\n\n[" << (index + 1) << "] "
               << (entries[index].display_name.empty() ? entries[index].email_address
                                                       : entries[index].display_name)
               << '\n'
               << "Provider: " << ProviderDisplayName(entries[index].provider_id);
        if (!entries[index].notes.empty()) {
            detail << '\n' << "Notes: " << entries[index].notes;
        }
    }
    return detail.str();
}

std::string LocalDirectoryServiceCatalog::LongDetailText(const std::vector<std::string>& provider_ids,
                                                         std::string_view query,
                                                         const std::vector<DirectoryEntry>& entries) const {
    std::ostringstream detail;
    detail << "Directory Providers: " << ProviderSelectionLabel(provider_ids);
    if (!query.empty()) {
        detail << '\n' << "Query: " << query;
    }
    detail << '\n' << "Results: " << entries.size();
    if (entries.empty()) {
        detail << "\n\nNo matching results.";
        return detail.str();
    }

    detail << "\n\nUse the results list to inspect one or more directory entries.";
    if (!entries.empty()) {
        detail << "\nFirst Result: "
               << (entries.front().display_name.empty() ? entries.front().email_address
                                                        : entries.front().display_name);
    }
    return detail.str();
}

std::string LocalDirectoryServiceCatalog::PrintableDetailText(const std::vector<DirectoryEntry>& entries) const {
    if (entries.empty()) {
        return "No directory results selected.";
    }
    if (entries.size() == 1) {
        return PrintableDetailText(entries.front());
    }

    std::ostringstream detail;
    detail << "Directory Results\n"
           << "Selected Results: " << entries.size() << '\n'
           << "Addresses: " << ComposeAddressText(entries);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        detail << "\n\n[" << (index + 1) << "]\n" << PrintableDetailText(entries[index]);
    }
    return detail.str();
}

std::string LocalDirectoryServiceCatalog::ComposeAddressText(const std::vector<DirectoryEntry>& entries) const {
    return DirectoryComposeAddressList(entries);
}

std::string LocalDirectoryServiceCatalog::PrintableText(std::string_view provider_id,
                                                        std::string_view query,
                                                        const std::vector<DirectoryEntry>& entries) const {
    return PrintableText(std::vector<std::string>{std::string(provider_id)}, query, entries);
}

std::string LocalDirectoryServiceCatalog::PrintableText(const std::vector<std::string>& provider_ids,
                                                        std::string_view query,
                                                        const std::vector<DirectoryEntry>& entries) const {
    std::ostringstream output;
    output << "Directory Providers: " << ProviderSelectionLabel(provider_ids) << '\n'
           << "Query: " << query << "\n\n";
    if (entries.empty()) {
        output << "No matching results.";
        return output.str();
    }

    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) {
            output << "\n\n";
        }
        output << PrintableDetailText({entries[index]});
    }
    return output.str();
}

std::string DirectoryComposeAddress(const DirectoryEntry& entry) {
    std::vector<std::string> addresses;
    for (const auto& address : EntryAddresses(entry)) {
        const std::string composed = ComposeNamedAddress(entry.display_name, address);
        if (!composed.empty()) {
            addresses.push_back(composed);
        }
    }
    return JoinStrings(addresses, ", ");
}

std::string DirectoryComposeAddressList(const std::vector<DirectoryEntry>& entries) {
    std::vector<std::string> addresses;
    for (const auto& entry : entries) {
        for (const auto& address : EntryAddresses(entry)) {
            const std::string composed = ComposeNamedAddress(entry.display_name, address);
            if (!composed.empty() &&
                std::find(addresses.begin(), addresses.end(), composed) == addresses.end()) {
                addresses.push_back(composed);
            }
        }
    }
    return JoinStrings(addresses, ", ");
}

}  // namespace hermes
