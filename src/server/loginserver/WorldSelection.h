//////////////////////////////////////////////////////////////////////////////
// Filename    : WorldSelection.h
// Description : the loginserver's world and server-group selection decisions,
//               separated from the CLSelectWorld and CLSelectServer handlers
//               so they can be exercised without a socket or a database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __WORLD_SELECTION_H__
#define __WORLD_SELECTION_H__

#include <string>
#include <vector>

#include "Outcome.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// The population ladder
//////////////////////////////////////////////////////////////////////////////

// The population steps a server group's status is read off. Every step is
// measured from userModify, so raising that one number moves the whole
// ladder; userMax is an absolute cap that is not.
struct ServerLoadThresholds {
    // The offset every step below is measured from.
    int userModify = 800;
    // Below userModify plus one of these the group reads free, normal, busy
    // or very busy; at or above the last one it reads full.
    int freeBelow = 100;
    int normalBelow = 250;
    int busyBelow = 400;
    int veryBusyBelow = 500;
    // At or above this population the group reads full whatever the steps
    // above said.
    int userMax = 1500;
};

// The status a client is shown for one server group: the population ladder,
// then the absolute cap, then the group table's own down flag, which wins
// over both.
BYTE serverGroupStatusFor(UserNum_t userNum, BYTE groupStat, const ServerLoadThresholds& thresholds);

//////////////////////////////////////////////////////////////////////////////
// The server list a world answers with
//////////////////////////////////////////////////////////////////////////////

// One row of the group table, as the server tables hold it.
struct ServerGroupRow {
    ServerGroupID_t groupID = 0;
    std::string groupName;
    // SERVER_DOWN marks a group down whatever its population.
    BYTE stat = SERVER_FREE;
};

// One entry of the list the client is sent. The name is untruncated; the
// packet's own field truncates it.
struct ServerListEntry {
    ServerGroupID_t groupID = 0;
    std::string groupName;
    BYTE stat = SERVER_FREE;
};

// The group table a server list is built from. Every lookup throws
// NoSuchElementException when the tables have no row for an id the
// configuration promised; that is a configuration fault, not a
// player-facing rejection, and is left to the caller.
class ServerListTopology {
public:
    virtual ~ServerListTopology() {}

    // Actual configured keys, in unspecified order. Empty when the world
    // has no groups. Membership stays stable during a decision/list build.
    virtual std::vector<ServerGroupID_t> serverGroupIDs(WorldID_t worldID) = 0;
    virtual ServerGroupRow serverGroup(ServerGroupID_t groupID, WorldID_t worldID) = 0;
    // How many accounts the group is carrying right now.
    virtual UserNum_t serverGroupUserNum(ServerGroupID_t groupID, WorldID_t worldID) = 0;
};

// The world table beside it.
class WorldSelectionTopology : public ServerListTopology {
public:
    // Actual configured keys, in unspecified order; IDs need not be contiguous.
    virtual std::vector<WorldID_t> worldIDs() = 0;
    // Is the world taking logins?
    virtual WorldStatus worldStatus(WorldID_t worldID) = 0;
};

// Every group of a world in ascending ID order, with its live population status.
// The caller turns these into the packet's own entries.
[[nodiscard]] std::vector<ServerListEntry> serverListFor(WorldID_t worldID, const ServerLoadThresholds& thresholds,
                                                         ServerListTopology& topology);

//////////////////////////////////////////////////////////////////////////////
// Selecting a world
//////////////////////////////////////////////////////////////////////////////

// Why a world selection was refused.
//
//   UnknownWorld   the connection is dropped
//   WorldClosed    the connection is dropped
//
// Neither is answerable with a packet: a client that speaks the protocol
// picks a world off the list it was just sent, so both drop the connection.
enum class SelectWorldReason { UnknownWorld, WorldClosed };

struct SelectWorldRejection {
    SelectWorldReason reason = SelectWorldReason::UnknownWorld;
    // errorLogin.txt names the number of configured worlds beside the id it
    // refused.
    int worldCount = 0;
};

// May the account enter this world? The list the accepted world answers
// with is built separately, by serverListFor(), because the caller builds
// and sends it inside a catch that these two refusals must not be in.
[[nodiscard]] Outcome<void, SelectWorldRejection> decideSelectWorld(WorldID_t worldID,
                                                                    WorldSelectionTopology& topology);

//////////////////////////////////////////////////////////////////////////////
// Selecting a server group
//////////////////////////////////////////////////////////////////////////////

// A group selection: the CLSelectServer packet's group, and the world the
// session is on.
struct SelectServerRequest {
    WorldID_t worldID = 0;
    ServerGroupID_t serverGroupID = 0;
};

// The group an accepted selection puts the session on. Both ids are
// checked against actual membership: a missing world or group falls back
// to the highest configured ID in its table, including requests in gaps.
struct SelectedServer {
    // The world the group was looked up in. The session's own world is not
    // rewritten to it.
    WorldID_t worldID = 0;
    ServerGroupID_t serverGroupID = 0;
};

// Each reason drops the connection. An empty table has no fallback ID.
enum class SelectServerReason { ServerClosed, NoWorlds, NoServerGroups };

struct SelectServerRejection {
    SelectServerReason reason = SelectServerReason::ServerClosed;
    // The normalized group, or the requested group when no fallback exists.
    ServerGroupID_t serverGroupID = 0;
};

// Normalize missing IDs to the highest configured keys. Refuse empty tables
// and groups that are down. World-closed policy belongs to decideSelectWorld.
[[nodiscard]] Outcome<SelectedServer, SelectServerRejection> decideSelectServer(const SelectServerRequest& request,
                                                                                WorldSelectionTopology& topology);

#endif
