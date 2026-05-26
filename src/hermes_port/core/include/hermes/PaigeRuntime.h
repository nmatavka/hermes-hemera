#pragma once

#include <memory>
#include <string>

namespace hermes {

class PaigeRuntime {
public:
    PaigeRuntime();
    ~PaigeRuntime();

    PaigeRuntime(const PaigeRuntime&) = delete;
    PaigeRuntime& operator=(const PaigeRuntime&) = delete;

    bool Initialize(std::string* error_message = nullptr);
    void Shutdown();
    bool IsAvailable() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hermes
