#include "hermes/PaigeRuntime.h"

#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
#include "Paige.h"
#endif

namespace hermes {

struct PaigeRuntime::Impl {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    pg_globals globals {};
    pgm_globals memory_globals {};
#endif
    bool initialized = false;
};

PaigeRuntime::PaigeRuntime() : impl_(std::make_unique<Impl>()) {}

PaigeRuntime::~PaigeRuntime() {
    Shutdown();
}

bool PaigeRuntime::Initialize(std::string* error_message) {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE && HERMES_IS_HAIKU
    if (impl_->initialized) {
        return true;
    }

    pgMemStartup(&impl_->memory_globals, 0);
    pgInit(&impl_->globals, &impl_->memory_globals);
    impl_->initialized = true;
    return true;
#else
    if (error_message) {
        *error_message =
            "Native Paige runtime initialization is scaffolded, but the Haiku platform device glue is not enabled in this build.";
    }
    return false;
#endif
}

void PaigeRuntime::Shutdown() {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE && HERMES_IS_HAIKU
    if (!impl_ || !impl_->initialized) {
        return;
    }

    pgShutdown(&impl_->globals);
    pgMemShutdown(&impl_->memory_globals);
    impl_->initialized = false;
#endif
}

bool PaigeRuntime::IsAvailable() const {
    return impl_ && impl_->initialized;
}

void* PaigeRuntime::NativeGlobals() {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    return impl_ ? &impl_->globals : nullptr;
#else
    return nullptr;
#endif
}

const void* PaigeRuntime::NativeGlobals() const {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    return impl_ ? &impl_->globals : nullptr;
#else
    return nullptr;
#endif
}

void* PaigeRuntime::NativeMemoryGlobals() {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    return impl_ ? &impl_->memory_globals : nullptr;
#else
    return nullptr;
#endif
}

const void* PaigeRuntime::NativeMemoryGlobals() const {
#if HERMES_ENABLE_NATIVE_PAIGE && HERMES_HAS_PAIGE
    return impl_ ? &impl_->memory_globals : nullptr;
#else
    return nullptr;
#endif
}

}  // namespace hermes
