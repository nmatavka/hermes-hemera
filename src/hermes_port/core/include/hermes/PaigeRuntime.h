#pragma once

#include <string>

namespace hermes {

class PaigeRuntime {
public:
    PaigeRuntime() = default;
    ~PaigeRuntime() = default;

    PaigeRuntime(const PaigeRuntime&) = delete;
    PaigeRuntime& operator=(const PaigeRuntime&) = delete;

    bool Initialize(std::string* error_message = nullptr);
    void Shutdown();
    bool IsAvailable() const;
};

}  // namespace hermes
