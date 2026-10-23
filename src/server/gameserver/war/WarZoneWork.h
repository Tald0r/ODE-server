//////////////////////////////////////////////////////////////////////////////
// Filename    : WarZoneWork.h
// Description : a war's zone work, posted to the threads that own it.
//
//               A war starts and ends on the thread that runs its schedule
//               -- the main thread's war heartbeat, a castle zone's own
//               scheduler, or a zone thread whose GM starts the race war --
//               and the zones it changes belong to their zone groups'
//               threads (docs/threading/mutation-rule.md). Each change is
//               therefore posted to the owning group (ZoneGroup::post) or,
//               for an item a player carries, to the player
//               (de::postToPlayer), capturing only values: zone ids, item
//               ids and the item-object row that says where an item lies.
//               The command looks the zone up again when it runs, on the
//               owner's thread under the group mutex. The routing decisions
//               are in WarZoneRouting.h.
//
//               A broadcast to a zone's players reads the zone's player list,
//               which its group's thread adds to and removes from, so it is
//               posted too (postBroadcast). The packet crosses as its written
//               body (CapturedPacket.h): the caller's packet is a stack object
//               the command would outlive, and some packets own heap objects
//               a copy would share and free twice, while the body is a plain
//               byte string written once, on the calling thread. Resending
//               bytes already framed by one stream would be wrong -- the
//               sequence byte and the encryption belong to each player's own
//               stream -- so the capture holds the body alone, and each
//               player's stream frames it as it frames any packet.
//////////////////////////////////////////////////////////////////////////////

#ifndef __WAR_ZONE_WORK_H__
#define __WAR_ZONE_WORK_H__

#include <functional>
#include <vector>

#include "Item.h"
#include "Types.h"

class Packet;
class Zone;
class ZoneGroup;

namespace de::war {

// Work on one zone, run on the zone's group thread with the group mutex held.
using ZoneWork = std::function<void(Zone& zone)>;

// Posts work to the group that owns zoneID. False, posting nothing, when this
// server has no such zone (logged).
bool postToZone(ZoneID_t zoneID, ZoneWork work);

// Posts one command to each group owning some of zoneIDs, running work on
// that group's zones in the order given. A zone whose work throws is logged
// and the group's other zones still run. Zones this server does not have are
// logged and skipped.
void postToZones(const std::vector<ZoneID_t>& zoneIDs, const ZoneWork& work);

// Posts work to every zone group, for work on the players a group owns.
void postToEveryZoneGroup(const std::function<void(ZoneGroup& zoneGroup)>& work);

// How a posted broadcast hands its packet to one zone's players, on the
// zone's thread.
using ZoneSend = void (*)(Zone& zone, Packet& packet);

// Zone::broadcastPacket: every player in the zone.
void sendToEveryPlayer(Zone& zone, Packet& packet);

// Sends packet to the players in each of zoneIDs, from the zones' own
// threads: the body is captured now and one command goes to each owning
// group, as postToZones sends them, where send hands it to each zone, so a
// player sees it at the top of his group's next tick. A packet whose write
// refuses a field (InvalidProtocolException, which a player's send drops) is
// logged and sent to nobody. Callable from any thread; it takes the zone
// groups' mailbox mutexes, which are leaves.
void postBroadcast(const std::vector<ZoneID_t>& zoneIDs, const Packet& packet, ZoneSend send = sendToEveryPlayer);

// What a return does with the item it took, on the holder's thread: from is
// the zone the item was taken from -- the zone it lay in, or the zone of the
// player who carried it -- and is owned by the calling thread. The item is
// out of every container, so the callback owns it; it hands it on the way
// Zone::transportItemToCorpse does, or keeps it where it is referenced.
using ItemTaken = std::function<void(Zone& from, Item* pItem)>;

// Returns the item itemID of itemClass from wherever it is: reads where its
// row says it lies, and posts the step that takes it out to that holder --
// the zone's group for an item on the ground or in a corpse, the player for
// an item in an inventory or on the mouse. The holder's step takes the item
// out under the holder's lock only if it is still that item, and hands it to
// taken. A step that finds the item gone reads the row again and follows it
// (WarZoneRouting.h, kItemReturnAttempts); a player who logs out first drops
// what he carried (a dragon eye goes back to its default tile instead), and
// the zone's add saves the row as it does for any relic, so the return
// follows it from there. A row naming a player who is no longer logged in
// is read again the same way: he dropped the item before he was gone.
// False when no step could be posted: no row, a holder no position loader
// reaches, or a row that still names a player who is not logged in after
// the last attempt (each logged to WarError.log). No relic can lie where no
// position reaches (relicMayLieIn), and a relic's row names a zone of this
// server: the war relics are made in this server's shrines and default
// tiles, a player carries them only between this server's zones, and he
// drops them before his logout hands him to another server.
bool postItemReturn(Item::ItemClass itemClass, ItemID_t itemID, ItemTaken taken, int attemptsMade = 0);

} // namespace de::war

#endif // __WAR_ZONE_WORK_H__
