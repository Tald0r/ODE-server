#include "LoginPlayerRetirement.h"

#include <iostream>

#include "DatabaseError.h"
#include "LoginPlayer.h"
#include "Socket.h"

namespace de {
namespace {

void report(const LoginRetirementActions& actions, const std::exception_ptr& error) noexcept {
    if (!error || !actions.report)
        return;
    try {
        actions.report(error);
    } catch (...) {
    }
}

void disconnect(LoginPlayer& player, bool flush, const LoginRetirementActions& actions) noexcept {
    std::exception_ptr failure;
    try {
        actions.disconnect(player, flush);
    } catch (...) {
        failure = std::current_exception();
    }
    player.setPlayerStatus(LPS_END_SESSION);
    // Also close when a supplied disconnect failed before reaching transport
    // cleanup. Socket::close remembers closure and cannot close a reused number.
    std::exception_ptr closeFailure;
    try {
        player.getSocket()->close();
    } catch (...) {
        closeFailure = std::current_exception();
    }
    report(actions, failure);
    report(actions, closeFailure);
}

constexpr auto retryDelay = std::chrono::seconds(5);

} // namespace

const LoginRetirementActions& defaultLoginRetirementActions() {
    static const LoginRetirementActions actions{+[](LoginPlayer& player, bool flush) { player.disconnect(flush); },
                                                +[](const std::exception_ptr& error) {
                                                    try {
                                                        std::rethrow_exception(error);
                                                    } catch (const Throwable& cause) {
                                                        std::cout << cause.toString() << std::endl;
                                                    } catch (const DatabaseError& cause) {
                                                        std::cout << cause.message() << std::endl;
                                                    } catch (const std::exception& cause) {
                                                        std::cout << cause.what() << std::endl;
                                                    } catch (...) {
                                                        std::cout << "Unknown login retirement failure" << std::endl;
                                                    }
                                                }};
    return actions;
}

LoginPlayerRetirement::~LoginPlayerRetirement() {
    clear();
}

void LoginPlayerRetirement::retire(LoginConnection player, bool flush, LoginRetirementTime now,
                                   const LoginRetirementActions& actions, std::exception_ptr reason) noexcept {
    if (!player)
        return;
    report(actions, reason);
    disconnect(*player, flush, actions);
    if (player->loginAccountOwnership().account()) {
        player->m_RetirementDeadline = now + retryDelay;
        player->m_RetirementNext = std::move(m_Pending);
        m_Pending = std::move(player);
        ++m_Size;
    }
}

void LoginPlayerRetirement::retry(LoginRetirementTime now, const LoginRetirementActions& actions) noexcept {
    auto* link = &m_Pending;
    while (*link) {
        auto& player = **link;
        if (now < player.m_RetirementDeadline) {
            link = &player.m_RetirementNext;
            continue;
        }
        disconnect(player, false, actions);
        if (player.loginAccountOwnership().account()) {
            player.m_RetirementDeadline = now + retryDelay;
            link = &player.m_RetirementNext;
        } else {
            auto finished = std::move(*link);
            *link = std::move(finished->m_RetirementNext);
            --m_Size;
        }
    }
}

void LoginPlayerRetirement::clear() noexcept {
    while (m_Pending) {
        auto finished = std::move(m_Pending);
        m_Pending = std::move(finished->m_RetirementNext);
    }
    m_Size = 0;
}

} // namespace de
