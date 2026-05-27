#include "hermes/GssapiAuthenticator.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#if HERMES_HAS_KRB5
#include <gssapi/gssapi.h>
#endif

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string ReplaceAll(std::string value, std::string_view needle, std::string_view replacement) {
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
    return value;
}

std::string Base64Encode(const std::string& input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((input.size() + 2) / 3) * 4);

    int val = 0;
    int valb = -6;
    for (unsigned char ch : input) {
        val = (val << 8) + ch;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(kAlphabet[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(kAlphabet[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded.size() % 4 != 0) {
        encoded.push_back('=');
    }
    return encoded;
}

std::string Base64Decode(const std::string& input) {
    static const std::string kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> lookup(256, -1);
    for (std::size_t index = 0; index < kAlphabet.size(); ++index) {
        lookup[static_cast<unsigned char>(kAlphabet[index])] = static_cast<int>(index);
    }

    std::string decoded;
    int val = 0;
    int valb = -8;
    for (unsigned char ch : input) {
        if (lookup[ch] == -1) {
            if (ch == '=') {
                break;
            }
            continue;
        }
        val = (val << 6) + lookup[ch];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

#if HERMES_HAS_KRB5
std::string GssStatusToString(OM_uint32 code, int type) {
    OM_uint32 minor = 0;
    OM_uint32 context = 0;
    std::string combined;
    do {
        gss_buffer_desc buffer = GSS_C_EMPTY_BUFFER;
        const OM_uint32 major =
            gss_display_status(&minor, code, type, GSS_C_NO_OID, &context, &buffer);
        if (major != GSS_S_COMPLETE) {
            break;
        }
        if (buffer.length > 0 && buffer.value != nullptr) {
            if (!combined.empty()) {
                combined += " | ";
            }
            combined.append(static_cast<const char*>(buffer.value), buffer.length);
        }
        gss_release_buffer(&minor, &buffer);
    } while (context != 0);
    return combined;
}

std::string GssErrorText(OM_uint32 major, OM_uint32 minor) {
    std::string major_text = GssStatusToString(major, GSS_C_GSS_CODE);
    std::string minor_text = GssStatusToString(minor, GSS_C_MECH_CODE);
    if (major_text.empty()) {
        major_text = "Unknown GSSAPI failure";
    }
    if (!minor_text.empty()) {
        major_text += ": ";
        major_text += minor_text;
    }
    return major_text;
}

class SystemGssapiConversation final : public GssapiConversation {
public:
    explicit SystemGssapiConversation(gss_name_t target_name) : target_name_(target_name) {}

    ~SystemGssapiConversation() override {
        OM_uint32 minor = 0;
        if (target_name_ != GSS_C_NO_NAME) {
            gss_release_name(&minor, &target_name_);
        }
        if (context_ != GSS_C_NO_CONTEXT) {
            gss_delete_sec_context(&minor, &context_, GSS_C_NO_BUFFER);
        }
    }

    bool Step(std::string_view input_token,
              GssapiStepResult* result,
              MailTaskErrorKind* error_kind,
              std::string* error_message) override {
        if (result == nullptr) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = "GSSAPI step result storage is unavailable.";
            }
            return false;
        }

        OM_uint32 major = 0;
        OM_uint32 minor = 0;
        gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
        if (!input_token.empty()) {
            input.length = input_token.size();
            input.value = const_cast<char*>(input_token.data());
        }

        gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
        major = gss_init_sec_context(&minor,
                                     GSS_C_NO_CREDENTIAL,
                                     &context_,
                                     target_name_,
                                     GSS_C_NO_OID,
                                     GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,
                                     0,
                                     GSS_C_NO_CHANNEL_BINDINGS,
                                     input_token.empty() ? GSS_C_NO_BUFFER : &input,
                                     nullptr,
                                     &output,
                                     nullptr,
                                     nullptr);
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kCredentialAcquisitionFailed;
            }
            if (error_message) {
                *error_message = GssErrorText(major, minor);
            }
            if (output.length > 0) {
                gss_release_buffer(&minor, &output);
            }
            return false;
        }

        result->continue_needed = major == GSS_S_CONTINUE_NEEDED;
        result->output_token =
            output.length > 0 && output.value != nullptr
                ? std::string(static_cast<const char*>(output.value), output.length)
                : std::string();
        gss_release_buffer(&minor, &output);
        return true;
    }

    bool Unwrap(std::string_view input_token,
                std::string* output_token,
                MailTaskErrorKind* error_kind,
                std::string* error_message) override {
        if (output_token == nullptr) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = "GSSAPI unwrap output storage is unavailable.";
            }
            return false;
        }

        OM_uint32 major = 0;
        OM_uint32 minor = 0;
        gss_buffer_desc input = {input_token.size(), const_cast<char*>(input_token.data())};
        gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
        int conf_state = 0;
        gss_qop_t qop_state = 0;
        major = gss_unwrap(&minor, context_, &input, &output, &conf_state, &qop_state);
        if (major != GSS_S_COMPLETE) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = GssErrorText(major, minor);
            }
            return false;
        }

        *output_token =
            output.length > 0 && output.value != nullptr
                ? std::string(static_cast<const char*>(output.value), output.length)
                : std::string();
        gss_release_buffer(&minor, &output);
        return true;
    }

    bool Wrap(std::string_view input_token,
              std::string* output_token,
              MailTaskErrorKind* error_kind,
              std::string* error_message) override {
        if (output_token == nullptr) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = "GSSAPI wrap output storage is unavailable.";
            }
            return false;
        }

        OM_uint32 major = 0;
        OM_uint32 minor = 0;
        gss_buffer_desc input = {input_token.size(), const_cast<char*>(input_token.data())};
        gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
        int conf_state = 0;
        major = gss_wrap(&minor, context_, 0, 0, &input, &conf_state, &output);
        if (major != GSS_S_COMPLETE) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kHandshakeFailed;
            }
            if (error_message) {
                *error_message = GssErrorText(major, minor);
            }
            return false;
        }

        *output_token =
            output.length > 0 && output.value != nullptr
                ? std::string(static_cast<const char*>(output.value), output.length)
                : std::string();
        gss_release_buffer(&minor, &output);
        return true;
    }

private:
    gss_name_t target_name_ = GSS_C_NO_NAME;
    gss_ctx_id_t context_ = GSS_C_NO_CONTEXT;
};

class SystemGssapiEngine final : public GssapiEngine {
public:
    bool IsAvailable(MailTaskErrorKind* error_kind, std::string* error_message) const override {
        (void)error_kind;
        (void)error_message;
        return true;
    }

    std::unique_ptr<GssapiConversation> CreateConversation(std::string_view service_principal,
                                                           MailTaskErrorKind* error_kind,
                                                           std::string* error_message) const override {
        OM_uint32 major = 0;
        OM_uint32 minor = 0;
        gss_name_t target_name = GSS_C_NO_NAME;
        gss_buffer_desc service_buffer = {
            service_principal.size(),
            const_cast<char*>(service_principal.data()),
        };
        major = gss_import_name(&minor, &service_buffer, GSS_C_NT_HOSTBASED_SERVICE, &target_name);
        if (major != GSS_S_COMPLETE) {
            if (error_kind) {
                *error_kind = MailTaskErrorKind::kServicePrincipalFailure;
            }
            if (error_message) {
                *error_message = GssErrorText(major, minor);
            }
            return nullptr;
        }

        return std::make_unique<SystemGssapiConversation>(target_name);
    }
};
#else
class SystemGssapiEngine final : public GssapiEngine {
public:
    bool IsAvailable(MailTaskErrorKind* error_kind, std::string* error_message) const override {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kKerberosUnavailable;
        }
        if (error_message) {
            *error_message = "Kerberos support is unavailable in this build.";
        }
        return false;
    }

    std::unique_ptr<GssapiConversation> CreateConversation(std::string_view /*service_principal*/,
                                                           MailTaskErrorKind* error_kind,
                                                           std::string* error_message) const override {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kKerberosUnavailable;
        }
        if (error_message) {
            *error_message = "Kerberos support is unavailable in this build.";
        }
        return nullptr;
    }
};
#endif

}  // namespace

GssapiAuthenticator::GssapiAuthenticator(const GssapiEngine& engine) : engine_(engine) {}

bool GssapiAuthenticator::Exchange(std::string_view service_principal,
                                   std::string_view authzid,
                                   const std::function<bool(std::string*, std::string*)>& read_challenge,
                                   const std::function<bool(std::string_view, std::string*)>& send_response,
                                   GssapiAuthFailure* failure) const {
    constexpr unsigned char kAuthGssapiProtectionNone = 1;

    auto fail = [&](MailTaskErrorKind kind, std::string message) {
        if (failure != nullptr) {
            failure->kind = kind;
            failure->message = std::move(message);
            failure->principal = std::string(service_principal);
        }
        return false;
    };

    MailTaskErrorKind error_kind = MailTaskErrorKind::kUnknown;
    std::string error_message;
    if (service_principal.empty()) {
        return fail(MailTaskErrorKind::kServicePrincipalFailure,
                    "Kerberos service principal is empty.");
    }
    if (!engine_.IsAvailable(&error_kind, &error_message)) {
        return fail(error_kind, std::move(error_message));
    }

    auto conversation = engine_.CreateConversation(service_principal, &error_kind, &error_message);
    if (!conversation) {
        return fail(error_kind, std::move(error_message));
    }

    std::string current_challenge;
    if (!read_challenge(&current_challenge, &error_message)) {
        return fail(MailTaskErrorKind::kHandshakeFailed, std::move(error_message));
    }

    while (true) {
        const std::string decoded_challenge = Base64Decode(current_challenge);
        GssapiStepResult result;
        if (!conversation->Step(decoded_challenge, &result, &error_kind, &error_message)) {
            return fail(error_kind, std::move(error_message));
        }

        const std::string response = Base64Encode(result.output_token);
        if (!send_response(response + "\r\n", &error_message)) {
            return fail(MailTaskErrorKind::kHandshakeFailed, std::move(error_message));
        }

        if (!result.continue_needed) {
            break;
        }
        if (!read_challenge(&current_challenge, &error_message)) {
            return fail(MailTaskErrorKind::kHandshakeFailed, std::move(error_message));
        }
    }

    if (!read_challenge(&current_challenge, &error_message)) {
        return fail(MailTaskErrorKind::kHandshakeFailed, std::move(error_message));
    }

    const std::string decoded_final = Base64Decode(current_challenge);
    if (!decoded_final.empty()) {
        std::string unwrapped;
        if (!conversation->Unwrap(decoded_final, &unwrapped, &error_kind, &error_message)) {
            return fail(error_kind, std::move(error_message));
        }

        if (unwrapped.size() < 4 ||
            !(static_cast<unsigned char>(unwrapped[0]) & kAuthGssapiProtectionNone)) {
            return fail(MailTaskErrorKind::kHandshakeFailed,
                        "GSSAPI server did not offer no-protection SASL mode.");
        }

        std::string final_payload = unwrapped;
        final_payload.resize(4);
        final_payload[0] = static_cast<char>(kAuthGssapiProtectionNone);
        final_payload += authzid;

        std::string wrapped;
        if (!conversation->Wrap(final_payload, &wrapped, &error_kind, &error_message)) {
            return fail(error_kind, std::move(error_message));
        }

        if (!send_response(Base64Encode(wrapped) + "\r\n", &error_message)) {
            return fail(MailTaskErrorKind::kHandshakeFailed, std::move(error_message));
        }
    }

    return true;
}

const GssapiEngine& DefaultGssapiEngine() {
    static const SystemGssapiEngine engine;
    return engine;
}

std::string BuildKerberosServicePrincipal(const AccountProfile& account,
                                          std::string_view host,
                                          std::string_view default_service_name,
                                          MailTaskErrorKind* error_kind,
                                          std::string* error_message) {
    const std::string service_name =
        account.kerberos.service_name.empty() ? std::string(default_service_name)
                                              : account.kerberos.service_name;
    if (service_name.empty() || host.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kServicePrincipalFailure;
        }
        if (error_message) {
            *error_message = "Kerberos service principal is missing a service name or host.";
        }
        return {};
    }

    std::string format =
        account.kerberos.service_format.empty() ? "%s@%h" : account.kerberos.service_format;
    format = ReplaceAll(std::move(format), "%s", service_name);
    format = ReplaceAll(std::move(format), "%h", host);
    format = ReplaceAll(std::move(format), "%r", account.kerberos.realm);
    format = Trim(std::move(format));
    if (format.empty()) {
        if (error_kind) {
            *error_kind = MailTaskErrorKind::kServicePrincipalFailure;
        }
        if (error_message) {
            *error_message = "Kerberos service principal format produced an empty principal.";
        }
        return {};
    }

    return format;
}

}  // namespace hermes
