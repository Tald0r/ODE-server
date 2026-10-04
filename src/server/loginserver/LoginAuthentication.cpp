#include "LoginAuthentication.h"

#include <exception>
#include <iostream>
#include <utility>

#include "CLLogin.h"
#include "LCLoginError.h"
#include "LoginPlayer.h"
#include "Utility.h"

namespace de {
namespace {

template <typename Action, typename... Args> void report(const Action& action, Args&&... args) noexcept {
    try {
        if (action)
            action(std::forward<Args>(args)...);
    } catch (...) {
    }
}

struct Refusal {
    BYTE code;
    const char* name;
    int site;
};

Refusal describeRefusal(LoginRejectReason reason) {
    switch (reason) {
    case LoginRejectReason::IPBlocked:
        return {IP_DENYED, "IP_DENYED", 1};
    case LoginRejectReason::MalformedID:
        return {INVALID_ID_PASSWORD, "INVALID_ID_PASSWORD", 2};
    case LoginRejectReason::UnknownAccountOrPassword:
        return {INVALID_ID_PASSWORD, "INVALID_ID_PASSWORD", 3};
    case LoginRejectReason::FreePassAccountMissing:
        return {ETC_ERROR, "ETC_ERROR", 4};
    case LoginRejectReason::AccessNotAllowed:
        return {ETC_ERROR, "ETC_ERROR", 5};
    case LoginRejectReason::NotPayAccount:
        return {NOT_PAY_ACCOUNT, "NOT_PAY_ACCOUNT", 6};
    case LoginRejectReason::AlreadyConnected:
        return {ALREADY_CONNECTED, "ALREADY_CONNECTED", 7};
    case LoginRejectReason::AlreadyLoggedOnElsewhere:
        return {ALREADY_CONNECTED, "ALREADY_CONNECTED", 8};
    case LoginRejectReason::NetMarbleAuthorization:
        return {INVALID_ID_PASSWORD, "INVALID_ID_PASSWORD", 9};
    case LoginRejectReason::WebLoginKeyMismatch:
        return {INVALID_ID_PASSWORD, "INVALID_ID_PASSWORD", 10};
    case LoginRejectReason::WebLoginKeyNotFound:
        return {NOT_FOUND_KEY, "NOT_FOUND_KEY", 11};
    case LoginRejectReason::WebLoginKeyExpired:
        return {KEY_EXPIRED, "KEY_EXPIRED", 12};
    }
    throw Error("unknown login rejection");
}

bool verifyNetMarblePassword(const std::string& account, const std::string& credential,
                             LoginAccountRepository& repository, const LoginAuthenticationActions& actions) {
    try {
        std::string stored;
        if (repository.loadPasswordHash(account, stored)) {
            const auto verdict = actions.verifyPassword(stored, credential);
            if (verdict == password::Verify::Rejected)
                return false;
            if (verdict == password::Verify::AcceptedRehash) {
                std::string hash;
                try {
                    hash = actions.hashPassword(credential);
                } catch (const std::exception& error) {
                    report(actions.hashingFailure, account, error.what(), true);
                    return true;
                }
                repository.updatePassword(hash, account);
            }
            return true;
        }
        report(actions.newNetMarbleAccount, account);
        std::string hash;
        try {
            hash = actions.hashPassword(credential);
        } catch (const std::exception& error) {
            report(actions.hashingFailure, account, error.what(), false);
            return false;
        }
        repository.insertNetMarbleAccount(account, hash);
        return true;
    } catch (const Throwable&) {
        return false;
    }
}

} // namespace

const LoginAuthenticationActions& defaultLoginAuthenticationActions() {
    static const LoginAuthenticationActions actions{
        password::verify,
        password::hash,
        +[](LoginPlayer& player, LCLoginError& reply) { player.sendPacket(&reply); },
        +[](const std::string& account, const char* name, int site) {
            filelog("loginfail.txt", "Error Code: %s, %d, PlayerID : %s", name, site, account.c_str());
        },
        +[](const std::string& account, const char* detail, bool rehash) {
            if (rehash)
                filelog("loginfail.txt", "Password rehash failed, PlayerID : %s : %s", account.c_str(), detail);
            else
                filelog("loginfail.txt", "Password hashing failed, PlayerID : %s : %s", account.c_str(), detail);
        },
        +[](const std::string& account) { std::cout << "NetMarble New Player: " << account << std::endl; },
        {+[](const std::string& account) {
            filelog("keydiff.txt", "Web login key mismatch, Player ID: %s", account.c_str());
        }}};
    return actions;
}

void sendLoginRefusal(LoginPlayer& player, std::string account, LoginRejectReason reason,
                      const LoginAuthenticationActions& actions) {
    const auto refusal = describeRefusal(reason);
    LCLoginError reply;
    reply.setErrorID(refusal.code);
    actions.sendRefusal(player, reply);
    report(actions.refusal, account, refusal.name, refusal.site);
}

bool verifyNetMarblePassword(const CLLogin& packet, LoginAccountRepository& repository,
                             const LoginAuthenticationActions& actions) {
    const auto account = packet.getID();
    const auto credential = packet.getPassword();
    return verifyNetMarblePassword(account, credential, repository, actions);
}

bool authorizeNetMarbleLogin(LoginPlayer& player, const CLLogin& packet, LoginAccountRepository& repository,
                             const LoginAuthenticationActions& actions) {
    if (!packet.isNetmarble())
        return true;
    const auto account = packet.getID();
    const auto credential = packet.getPassword();
    if (!verifyNetMarblePassword(account, credential, repository, actions)) {
        sendLoginRefusal(player, account, LoginRejectReason::NetMarbleAuthorization, actions);
        return false;
    }
    player.setFreePass(true);
    return true;
}

bool authorizeWebLogin(LoginPlayer& player, const CLLogin& packet, LoginAccountRepository& repository,
                       const LoginAuthenticationActions& actions) {
    const auto account = packet.getID();
    const auto credential = packet.getPassword();
    try {
        const auto result = decideWebLoginKey(account, credential, repository, actions.webKey);
        if (result.isRejected()) {
            sendLoginRefusal(player, account, result.rejection().reason, actions);
            return false;
        }
        repository.deleteWebLoginKey(account);
        player.setFreePass(true);
        return true;
    } catch (const Throwable&) {
        return false;
    }
}

} // namespace de
