//----------------------------------------------------------------------
//
// Filename    : GameServerGroupInfoManager.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameServerGroupInfoManager.h"

#include "DiagnosticTrace.h"
#include "repository/GameInfoRepository.h"


//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
GameServerGroupInfoManager::GameServerGroupInfoManager()

{
    m_MaxWorldID = 0;
}

//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
GameServerGroupInfoManager::~GameServerGroupInfoManager()

{
    clear();
}

//----------------------------------------------------------------------
// clear GameServerGroupInfos
//----------------------------------------------------------------------
void GameServerGroupInfoManager::clear()

{
    // Delete only the second of each pair in the hashmap, that is the GameServerGroupInfo
    // objects, and leave the pairs themselves alone. GameServerGroupInfo is heap allocated,
    // so it must be deleted explicitly. GSIM being destructed means the login server is
    // shutting down anyway.
    for (int i = 1; i < m_MaxWorldID; i++) {
        for (HashMapGameServerGroupInfo::iterator itr = m_GameServerGroupInfos[i].begin();
             itr != m_GameServerGroupInfos[i].end(); itr++) {
            SAFE_DELETE(itr->second);
        }

        // Now delete every pair in the hashmap.
        m_GameServerGroupInfos[i].clear();
    }

    SAFE_DELETE_ARRAY(m_GameServerGroupInfos);
}


//----------------------------------------------------------------------
// initialize GSIM
//----------------------------------------------------------------------
void GameServerGroupInfoManager::init()

{
    // just load data from GameServerGroupInfo table
    load();

    // Optional startup catalogue diagnostics.
    de::diagnosticTrace([&](std::ostream& output) { output << toString(); });
}

//----------------------------------------------------------------------
// load data from database
//----------------------------------------------------------------------
void GameServerGroupInfoManager::load()

{
    clear();

    int maxWorldID = 0;
    if (!defaultGameInfoRepository().loadMaxWorldID(maxWorldID)) {
        throw Error("GameServerGroupInfo TABLE does not exist!");
    }

    m_MaxWorldID = maxWorldID + 2;

    m_GameServerGroupInfos = new HashMapGameServerGroupInfo[m_MaxWorldID];

    // A SQL failure is converted inside the repository (DBError.log + a
    // thrown DatabaseError, see DB.h's END_DB) and escapes this try; a
    // failure in addGameServerGroupInfo is printed and swallowed.
    try {
        vector<GameServerGroupRow> rows = defaultGameInfoRepository().loadGameServerGroups();

        for (size_t r = 0; r < rows.size(); r++) {
            GameServerGroupInfo* pGameServerGroupInfo = new GameServerGroupInfo();
            WorldID_t WorldID = rows[r].worldID;
            pGameServerGroupInfo->setWorldID(WorldID);
            pGameServerGroupInfo->setGroupID(rows[r].groupID);
            pGameServerGroupInfo->setGroupName(rows[r].groupName);
            pGameServerGroupInfo->setStat(rows[r].stat);
            addGameServerGroupInfo(pGameServerGroupInfo, WorldID);
        }
    } catch (Throwable& t) {
        cout << t.toString() << endl;
    }
}

//----------------------------------------------------------------------
// add info
//----------------------------------------------------------------------
void GameServerGroupInfoManager::addGameServerGroupInfo(GameServerGroupInfo* pGameServerGroupInfo, WorldID_t WorldID) {
    int GroupID = pGameServerGroupInfo->getGroupID();
    HashMapGameServerGroupInfo::iterator itr = m_GameServerGroupInfos[WorldID].find(GroupID);

    if (itr != m_GameServerGroupInfos[WorldID].end())
        throw DuplicatedException("duplicated game-server nickname");

    de::diagnosticTrace([&](std::ostream& output) {
        output << "addGameServerGroupInfo: " << (int)WorldID << ", " << GroupID << " : "
               << pGameServerGroupInfo->getGroupName().c_str();
    });

    m_GameServerGroupInfos[WorldID][GroupID] = pGameServerGroupInfo;
}

//----------------------------------------------------------------------
// delete info
//----------------------------------------------------------------------
void GameServerGroupInfoManager::deleteGameServerGroupInfo(const ServerGroupID_t GroupID, WorldID_t WorldID) {
    HashMapGameServerGroupInfo::iterator itr = m_GameServerGroupInfos[WorldID].find(GroupID);

    if (itr != m_GameServerGroupInfos[WorldID].end()) {
        // Delete the GameServerGroupInfo.
        delete itr->second;

        // Delete the pair.
        m_GameServerGroupInfos[WorldID].erase(itr);

    } else {
        // No such game server info object was found.
        throw NoSuchElementException();
    }
}

//----------------------------------------------------------------------
// get GameServerGroupinfo by ServerGroupID
//----------------------------------------------------------------------
GameServerGroupInfo* GameServerGroupInfoManager::getGameServerGroupInfo(const ServerGroupID_t GroupID,
                                                                        WorldID_t WorldID) const {
    if (WorldID >= m_MaxWorldID) {
        // No such game server info object was found.
        throw NoSuchElementException();
    }

    GameServerGroupInfo* pGameServerGroupInfo = NULL;

    HashMapGameServerGroupInfo::const_iterator itr = m_GameServerGroupInfos[WorldID].find(GroupID);

    if (itr != m_GameServerGroupInfos[WorldID].end()) {
        pGameServerGroupInfo = itr->second;
    } else {
        // No such game server info object was found.
        throw NoSuchElementException();
    }

    return pGameServerGroupInfo;
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string GameServerGroupInfoManager::toString() const

{
    StringStream msg;

    msg << "GameServerGroupInfoManager(\n";

    for (int i = 1; i < m_MaxWorldID; i++) {
        if (m_GameServerGroupInfos[i].empty()) {
            msg << "EMPTY";

        } else {
            //--------------------------------------------------
            // *OPTIMIZATION*
            //
            // Use for_each().
            //--------------------------------------------------
            for (HashMapGameServerGroupInfo::const_iterator itr = m_GameServerGroupInfos[i].begin();
                 itr != m_GameServerGroupInfos[i].end(); itr++)
                msg << itr->second->toString() << '\n';
        }
    }

    msg << ")";

    return msg.toString();
}
