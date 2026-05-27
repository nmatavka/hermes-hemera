#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "hermes/AccountService.h"
#include "hermes/MailTaskModel.h"

namespace hermes {

struct GssapiStepResult {
    bool continue_needed = false;
    std::string output_token;
};

class GssapiConversation {
public:
    virtual ~GssapiConversation() = default;

    virtual bool Step(std::string_view input_token,
                      GssapiStepResult* result,
                      MailTaskErrorKind* error_kind,
                      std::string* error_message) = 0;
    virtual bool Unwrap(std::string_view input_token,
                        std::string* output_token,
                        MailTaskErrorKind* error_kind,
                        std::string* error_message) = 0;
    virtual bool Wrap(std::string_view input_token,
                      std::string* output_token,
                      MailTaskErrorKind* error_kind,
                      std::string* error_message) = 0;
};

class GssapiEngine {
public:
    virtual ~GssapiEngine() = default;

    virtual bool IsAvailable(MailTaskErrorKind* error_kind, std::string* error_message) const = 0;
    virtual std::unique_ptr<GssapiConversation> CreateConversation(
        std::string_view service_principal,
        MailTaskErrorKind* error_kind,
        std::string* error_message) const = 0;
};

struct GssapiAuthFailure {
    MailTaskErrorKind kind = MailTaskErrorKind::kUnknown;
    std::string mechanism = "GSSAPI";
    std::string principal;
    std::string message;
};

class GssapiAuthenticator {
public:
    explicit GssapiAuthenticator(const GssapiEngine& engine);

    bool Exchange(std::string_view service_principal,
                  std::string_view authzid,
                  const std::function<bool(std::string*, std::string*)>& read_challenge,
                  const std::function<bool(std::string_view, std::string*)>& send_response,
                  GssapiAuthFailure* failure) const;

private:
    const GssapiEngine& engine_;
};

const GssapiEngine& DefaultGssapiEngine();

std::string BuildKerberosServicePrincipal(const AccountProfile& account,
                                          std::string_view host,
                                          std::string_view default_service_name,
                                          MailTaskErrorKind* error_kind = nullptr,
                                          std::string* error_message = nullptr);

}  // namespace hermes
