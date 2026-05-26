#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct AddressBookEntry {
    std::string display_name;
    std::string email_address;
    std::string notes;
};

class AddressBookService {
public:
    virtual ~AddressBookService() = default;

    virtual void AddEntry(const AddressBookEntry& entry) = 0;
    virtual std::vector<AddressBookEntry> Entries() const = 0;
    virtual bool Contains(std::string_view email_address) const = 0;
};

class MemoryAddressBookService final : public AddressBookService {
public:
    void AddEntry(const AddressBookEntry& entry) override;
    std::vector<AddressBookEntry> Entries() const override;
    bool Contains(std::string_view email_address) const override;

private:
    static std::string Normalize(std::string_view value);

    std::vector<AddressBookEntry> entries_;
};

}  // namespace hermes
