// The common game-server catalogue, with owned rows and a borrowed traversal index.
#ifndef __GAME_SERVER_INFO_MANAGER_H__
#define __GAME_SERVER_INFO_MANAGER_H__

#include <memory>
#include <vector>

#include <unordered_map>

#include "GameServerInfo.h"

class ServerInfoRepository;

using HashMapGameServerInfo = std::unordered_map<ServerID_t, GameServerInfo*>;

// Load/clear require quiescent readers. Failure preserves borrowed rows and
// the traversal index; successful replacement/clear invalidates both.
class GameServerInfoManager {
public:
    GameServerInfoManager() = default;
    GameServerInfoManager(const GameServerInfoManager&) = delete;
    GameServerInfoManager& operator=(const GameServerInfoManager&) = delete;

    void init();
    void load();
    void load(ServerInfoRepository& repository);
    void clear() noexcept;

    GameServerInfo* getGameServerInfo(ServerID_t serverID, ServerGroupID_t groupID, WorldID_t worldID) const;
    uint getSize(WorldID_t worldID, ServerGroupID_t groupID) const noexcept;
    string toString() const;

    // Preserve the existing traversal dimensions: MAX(WorldID)+2 and MAX(GroupID)+1.
    int getMaxWorldID() const noexcept {
        return static_cast<int>(m_Catalogue.tables.size());
    }
    int getMaxServerGroupID() const noexcept {
        return m_Catalogue.groupCount;
    }
    // Borrowed legacy view. Callers may traverse it, but must not delete rows
    // or mutate the index. Every world slot, including zero, has a group table.
    HashMapGameServerInfo** getGameServerInfos() const noexcept {
        return m_Catalogue.view.get();
    }

private:
    struct Catalogue {
        GameServerInfo* lookup(ServerID_t serverID, ServerGroupID_t groupID, WorldID_t worldID) const;
        void swap(Catalogue& other) noexcept;

        std::vector<std::unique_ptr<GameServerInfo>> rows;
        std::vector<std::vector<HashMapGameServerInfo>> tables;
        std::unique_ptr<HashMapGameServerInfo*[]> view;
        int groupCount = 0;
    };

    Catalogue m_Catalogue;
};

#endif
