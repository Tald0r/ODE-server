#include "LoginKickVerification.h"

#include <mutex>

#include "GLKickVerify.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"

namespace de {

bool verifyLoginKick(LoginPlayerManager& players, const GLKickVerify& packet, const LoginKickCompletion& complete) {
    const auto descriptor = packet.getID();
    if (descriptor >= PlayerManager::nMaxPlayers)
        return false;
    const auto name = packet.getPCName();
    if (name.empty())
        return false;

    std::lock_guard guard(players);
    LoginPlayer* player;
    try {
        player =
            dynamic_cast<LoginPlayer*>(static_cast<PlayerManager&>(players).getPlayer(static_cast<SOCKET>(descriptor)));
    } catch (const NoSuchElementException&) {
        return false;
    }
    if (!player || player->getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY || player->getID().empty() ||
        player->getID() == "NONE")
        return false;
    const auto* target = player->getLoginKickTarget();
    if (!target || target->characterName != name)
        return false;

    // Either a completed kick or an already-absent character permits login.
    complete(*player);
    return true;
}

} // namespace de
