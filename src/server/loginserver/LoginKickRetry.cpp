#include "LoginKickRetry.h"

#include "LoginPlayer.h"

namespace de {
namespace {

bool isPending(const LoginPlayer& player, const std::string& account) {
    if (player.getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY || account.empty() || account == "NONE")
        return false;
    const auto* target = player.getLoginKickTarget();
    return target && !target->characterName.empty();
}

} // namespace

bool beginLoginKick(LoginPlayer& player, const LoginKickRequest& send) {
    player.m_KickCharacterCount = 0;
    ++player.m_KickCharacterAttempt;
    return send(player);
}

bool retryLoginKick(LoginPlayer& player, const Timeval& now, const LoginKickRequest& send,
                    const std::function<void(LoginPlayer&)>& complete) {
    if (!isPending(player, player.m_ID) || now < player.getExpireTimeForKickCharacter())
        return false;

    // Own these values before invoking an action that may replace the cache or
    // start another attempt, even for the same account and character.
    const auto account = player.m_ID;
    const auto target = *player.getLoginKickTarget();
    const auto attempt = player.m_KickCharacterAttempt;
    if (!send(player) || !isPending(player, player.m_ID) || player.m_KickCharacterAttempt != attempt ||
        player.m_ID != account || *player.getLoginKickTarget() != target)
        return false;

    constexpr uint maxRetries = 3;
    if (player.m_KickCharacterCount < maxRetries)
        ++player.m_KickCharacterCount;
    if (player.m_KickCharacterCount == maxRetries)
        complete(player);
    return true;
}

} // namespace de
