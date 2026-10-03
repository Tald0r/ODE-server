#include "LoginKickPreparation.h"

#include <iostream>
#include <utility>

#include "LCLoginError.h"
#include "LoginPlayer.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"

namespace de {
namespace {

std::nullopt_t refuseKick(LoginPlayer& player, const char* diagnostic) {
    std::cout << diagnostic << std::endl;
    LCLoginError error;
    error.setErrorID(ALREADY_CONNECTED);
    player.sendPacket(&error);
    player.clearLoginKickTarget();
    player.setPlayerStatus(LPS_BEGIN_SESSION);
    player.setID("NONE"); // Disconnect must not write LOGOFF for this account.
    return std::nullopt;
}

} // namespace

std::optional<LoginKickTarget> prepareLoginKick(LoginPlayer& player, LoginAccountRepository& accounts,
                                                LoginCharacterRepository& characters) {
    const auto account = player.getID();
    const auto* cached = player.getLoginKickTarget();
    const bool hadLocation = cached != nullptr;
    const bool hadName = cached && !cached->characterName.empty();
    LoginKickTarget target = cached ? *cached : LoginKickTarget{};

    if (!hadLocation) {
        int currentWorldID = 0;
        int currentGroupID = 0;
        int currentLastSlot = 0;
        if (!accounts.loadLastLocation(account, currentWorldID, currentGroupID, currentLastSlot))
            return refuseKick(player, "No LastLocation");
        if (!std::in_range<WorldID_t>(currentWorldID))
            throw Error("invalid last world ID");
        if (!std::in_range<ServerGroupID_t>(currentGroupID))
            throw Error("invalid last server group ID");
        if (currentLastSlot < 0 || currentLastSlot > SLOT_MAX)
            throw Error("invalid last character slot");
        target.worldID = static_cast<WorldID_t>(currentWorldID);
        target.groupID = static_cast<ServerGroupID_t>(currentGroupID);
        target.lastSlot = static_cast<uint>(currentLastSlot);
    }

    if (target.lastSlot > SLOT_MAX)
        throw Error("invalid last character slot");
    if (target.characterName.empty()) {
        if (target.lastSlot == 0 ||
            !characters.loadSlayerNameInSlot(target.worldID, account, static_cast<int>(target.lastSlot),
                                             target.characterName) ||
            target.characterName.empty())
            return refuseKick(player, "No CharacterName");
    }

    // Prepare both account identity and target before replacing the cache.
    if (!hadLocation || !hadName)
        player.cacheLoginKickTarget(target);
    if (!hadLocation)
        player.setWorldID(target.worldID);
    return std::optional<LoginKickTarget>{std::move(target)};
}

} // namespace de
