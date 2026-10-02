#include "LoginWorldList.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "GameWorldInfoManager.h"
#include "LCWorldList.h"
#include "Player.h"
#include "WorldInfo.h"
#include "repository/LoginAccountRepository.h"

namespace de {

void sendLoginWorldList(Player& player, const GameWorldInfoManager& worlds, LoginAccountRepository& accounts) {
    if (worlds.getSize() > WorldInfo::kMaxCount)
        throw InvalidProtocolException("too many world infos");

    std::vector<const GameWorldInfo*> ordered;
    ordered.reserve(worlds.getSize());
    for (const auto& [id, world] : worlds.getGameWorldInfos())
        ordered.push_back(&world);
    std::sort(ordered.begin(), ordered.end(),
              [](const auto* left, const auto* right) { return left->getID() < right->getID(); });

    LCWorldList reply;
    for (const auto* source : ordered) {
        auto info = std::make_unique<WorldInfo>();
        info->setID(source->getID());
        info->setName(source->getName());
        info->setStat(source->getStatus());
        reply.addListElement(info.release()); // Consumed even if list insertion fails.
    }
    int currentWorldID = 0;
    if (accounts.loadCurrentWorld(player.getID(), currentWorldID)) {
        if (!std::in_range<WorldID_t>(currentWorldID))
            throw Error("invalid current world ID");
        reply.setCurrentWorldID(static_cast<WorldID_t>(currentWorldID));
    }
    player.sendPacket(&reply);
}

} // namespace de
