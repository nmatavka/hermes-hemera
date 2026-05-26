#include "hermes/PaigeRuntime.h"

namespace hermes {

bool PaigeRuntime::Initialize(std::string* error_message) {
    if (error_message) {
        *error_message =
            "Native Paige runtime initialization is scaffolded, but the Haiku platform device glue is not enabled in this build.";
    }
    return false;
}

void PaigeRuntime::Shutdown() {}

bool PaigeRuntime::IsAvailable() const {
    return false;
}

}  // namespace hermes
