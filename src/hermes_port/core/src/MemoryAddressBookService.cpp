#include "hermes/AddressBookService.h"

#include <algorithm>
#include <cctype>

namespace hermes {

void MemoryAddressBookService::AddEntry(const AddressBookEntry& entry) {
    const std::string normalized = Normalize(entry.email_address);
    for (auto& existing : entries_) {
        if (Normalize(existing.email_address) == normalized) {
            existing = entry;
            return;
        }
    }
    entries_.push_back(entry);
}

std::vector<AddressBookEntry> MemoryAddressBookService::Entries() const {
    return entries_;
}

bool MemoryAddressBookService::Contains(std::string_view email_address) const {
    const std::string normalized = Normalize(email_address);
    return std::any_of(entries_.begin(), entries_.end(), [&](const AddressBookEntry& entry) {
        return Normalize(entry.email_address) == normalized;
    });
}

std::string MemoryAddressBookService::Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return normalized;
}

}  // namespace hermes
