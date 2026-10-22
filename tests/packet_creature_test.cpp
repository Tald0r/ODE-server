//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_creature_test.cpp
// Description : Golden byte fixtures, loopback round trips and size
//               pins for the packets a client and a game server
//               exchange about the state of a creature and of the zone
//               around it: riding and leaving a motorcycle, a vampire's
//               blood drain and the shapes it morphs through, becoming
//               visible again, the corpse and the enemy list, hit-point
//               and mana regeneration, the counter and knockback
//               results, a pet's skill, and the zone's light and
//               weather.
//
//               The set is taken from the code that sends them: the
//               handlers under src/server/gameserver/handler
//               (CGRideMotorCycleHandler, CGGetOffMotorCycleHandler,
//               CGBloodDrainHandler, CGAbsorbSoulHandler,
//               CGResurrectHandler, CGSilverCoatingHandler,
//               CGTameMonsterHandler, CGVisibleHandler,
//               CGDissectionCorpseHandler, the three potion and item
//               use handlers) and the senders outside handler/:
//               skill/BloodDrain.cpp, skill/CrossCounter.cpp,
//               skill/EatCorpse.cpp, skill/Restore.cpp,
//               skill/SkillUtil.cpp, skill/WildWolf.cpp,
//               skill/BloodyScarify.cpp, skill/EffectBloodDrain.cpp,
//               EventMorph.cpp, EffectMute.cpp, EffectComa.cpp,
//               EffectRideMotorcycle.cpp, WeatherManager.cpp,
//               GQuestExecuteElement.cpp, Pet.cpp, PCManager.cpp,
//               ZoneUtil.cpp, InitAllStat.cpp, Slayer.cpp,
//               Vampire.cpp, Ousters.cpp and
//               quest/ActionSearchMotorcycle.cpp. Forty-two packets,
//               each with the reason it is here:
//
//               CGRideMotorCycle the client asks to get on the
//                                motorcycle standing on a tile, and
//               GCRideMotorCycleOK   the rider is told it worked,
//               GCRideMotorCycleFailed   or that it did not, while
//               GCRideMotorCycle is what everyone else in the zone
//                                sees.
//               CGGetOffMotorCycle   the client asks to get off, and
//               GCGetOffMotorCycleOK the rider is told it worked,
//               GCGetOffMotorCycleFailed or that it did not, while
//               GCGetOffMotorCycle   is what the zone sees - also sent
//                                when a coma, a zone move or a death
//                                takes the rider off.
//               GCSearchMotorcycleOK where a parked motorcycle is,
//               GCSearchMotorcycleFail   and the answer when the key
//                                in the inventory matches none.
//
//               CGBloodDrain     the vampire bites, and
//               GCBloodDrainOK1  the biter,
//               GCBloodDrainOK2  the bitten and
//               GCBloodDrainOK3  the onlookers each get their own
//                                result, the first two carrying the
//                                stat record the bite moved.
//               GCMorph1         the whole character sheet a morph
//                                hands the morphing player, in the
//                                slayer or the vampire shape.
//               GCMorphSlayer2   the same change as the zone sees it,
//               GCMorphVampire2  one packet per resulting race.
//               GCChangeShape    the lighter change: the item a
//                                creature now appears to hold.
//
//               CGAbsorbSoul     the ousters draws a soul out of a
//                                corpse into a lava stone.
//               CGResurrect      the dead player asks to stand up.
//               CGSilverCoating  the slayer coats a weapon in silver.
//               CGTameMonster    the player tames a monster into a pet.
//
//               CGVisible        a wolf or a bat asks to be seen again,
//               GCVisibleFail    refused when the creature is in
//                                neither shape, and
//               GCVisibleOK      the answer the client can read for it.
//
//               GCRemoveCorpseHead   the head comes off a corpse, from
//                                dissection or from being eaten.
//               GCAddInjuriousCreature   the name of whoever struck
//                                first, so the target may strike back.
//
//               GCLightning      a flash over the zone,
//               GCChangeWeather  the weather it belongs to, and
//               GCChangeDarkLight    the zone's darkness and light,
//                                which the weather, the clock and half
//                                a dozen effects all move.
//               GCExecuteElement a general quest element firing on the
//                                creature.
//
//               GCHPRecoveryStartToSelf  a potion or a skill starts
//                                healing the drinker, and
//               GCHPRecoveryStartToOthers    the same healing as the
//                                zone sees it;
//               GCMPRecoveryStart    the mana equivalent.
//
//               GCCrossCounterOK1    the counter-attacker,
//               GCCrossCounterOK2    the countered and
//               GCCrossCounterOK3    the onlookers, the first two with
//                                the stat record the counter moved.
//               GCKnocksTargetBackOK1    the knockback result for the
//                                attacker,
//               GCKnocksTargetBackOK2    for the target,
//               GCKnocksTargetBackOK4    for the onlookers and
//               GCKnocksTargetBackOK5    for the attacker of a shot
//                                that missed.
//               GCPetUseSkill    a pet's own attack.
//
//               Deliberately excluded:
//
//               No packet in this family reads or writes the
//               encrypter. The nineteen src/Core sources that call
//               readEncrypt or writeEncrypt are CGAddMouseToZone,
//               CGAddZoneToInventory, CGAddZoneToMouse, CGAttack,
//               CGDissectionCorpse, CGDropMoney, CGMove,
//               CGNPCAskAnswer, CGPickupMoney, CGSkillToInventory,
//               CGSkillToObject, CGSkillToSelf, CGSkillToTile,
//               CGUseItemFromGear, CGUseItemFromInventory,
//               CGUsePotionFromInventory, GCAddItemToZone,
//               GCMoveError and GCMoveOK, all pinned at encrypt codes
//               0..5 by tests/packet_encrypter_test.cpp. None of the
//               forty-two derives from one of them: thirty-six extend
//               Packet directly and six extend ModifyInfo, which calls
//               neither. So each golden is recorded at code 0 and its
//               test also asserts the bytes do not vary with the code -
//               adopting the encrypter fails loudly instead of
//               silently voiding the pin.
//
//               Every one of the forty-two has a registered factory in
//               tests/ratchet/factory_registrations.txt, and none of
//               them had a golden or a test of any kind before this
//               file.
//
//               The answers these senders share with other families
//               are pinned with those families and not repeated here:
//               GCAttack, GCGetDamage, GCSkillFailed1, GCSkillFailed2,
//               GCStatusCurrentHP, GCModifyInformation,
//               GCOtherModifyInfo, GCSkillToSelfOK1, GCSkillToSelfOK2,
//               GCSkillToObjectOK1, GCSkillToObjectOK4,
//               GCSkillToObjectOK6, the six GCSkillToTileOK
//               (packet_combat_test.cpp); GCAddEffect,
//               GCAddEffectToTile, GCDeleteObject
//               (packet_zone_scan_test.cpp); GCRemoveEffect,
//               GCCannotUse (packet_movement_test.cpp); GCCreateItem,
//               GCDeleteInventoryItem, GCRemoveFromGear
//               (packet_inventory_test.cpp); GCUpdateInfo
//               (packet_gameserver_handshake_test.cpp);
//               GCLearnSkillReady (packet_skill_test.cpp);
//               GCNPCResponse (packet_guild_test.cpp);
//               GCSystemMessage, GCKickMessage
//               (packet_chat_test.cpp).
//
//               GCVisibleOK and GCKnocksTargetBackOK1 / 2 / 4 / 5 have
//               no sender at all: CGVisibleHandler answers a success by
//               making the creature visible and sending nothing, and no
//               source outside src/Core constructs any of the four
//               knockback results. All five are pinned anyway, because
//               their factories ARE registered: a registered factory is
//               the wire contract the client's own copy has to match,
//               and a GC packet the client can only receive needs no
//               server sender to be part of it.
//
//               Three packets outside this set are worth recording.
//               GCRing is constructed by CGDialUpHandler but its
//               factory appears in no registration list, so nothing can
//               build one off the wire; GCModifyMoney,
//               GCSubInventoryInfo and GCShowGuildRegist have neither a
//               registered factory nor a sender.
//
//               Each packet gets three pins (CREATURE_PACKET_TESTS):
//
//               - a loopback round trip through the real socket and
//                 stream classes, comparing every getter;
//               - the body bytes against tests/golden/<Name>.code0.hex;
//               - getPacketSize() against the byte count write()
//                 actually emits, and against the factory's
//                 getPacketMaxSize(). writePacket() puts
//                 getPacketSize() on the wire BEFORE calling write(),
//                 so a disagreement is not a wrong length, it is a
//                 stream that never resynchronises; and the receiving
//                 side sizes its read buffer from the factory max, so a
//                 body that outgrows it is a truncated packet, not a
//                 caught error.
//
//               GCMorph1 gets all three written out rather than from
//               the macro, because it holds its four records by
//               pointer.
//
//               Extra goldens cover the branches one fixture cannot:
//               the vampire shape of a morph (.vampire on GCMorph1,
//               with the empty inventory, gear and extra lists beside
//               the slayer fixture's full ones); the injurious
//               creature's name at ten bytes (.maxname); and the empty
//               stat record the three
//               ModifyInfo-carrying families can send (.nostats on
//               GCBloodDrainOK1, GCCrossCounterOK1 and
//               GCKnocksTargetBackOK1). The record's full 255-entry
//               lists and every one of its type tags are pinned in
//               tests/packet_combat_test.cpp and not repeated.
//
//               Fixture values are distinct per field and >= 128 in
//               every byte the width allows. Five groups cannot follow
//               that rule and say so at the point of use: the creature
//               and guild names, which are text; the weather, the sex,
//               the quest element condition and the slayer and vampire
//               outlook slices, which are enumerators; the knockback
//               success flags, which are bools; the PC attributes,
//               whose getters refuse anything above 2000.
//
//               What the two halves agree on, each pinned by a test of
//               its own beside the three above. No valid packet's bytes
//               moved for any of them, and docs/reviews/FIXES.md carries the
//               wire consequence of each:
//
//               - GCMorph1::getPacketSize() counts the pc-type byte
//                 write() puts in front of the record, so the declared
//                 length and the body sent agree.
//               - GCMorph1's four records start empty and both
//                 getPacketSize() and write() refuse a packet missing
//                 one, rather than following a null pointer. The
//                 records belong to the packet: every sender builds
//                 them for it and keeps none, so a setter frees what it
//                 replaces, read() frees the four it holds before
//                 filling four more, and the destructor frees what is
//                 left. The pc type is bounded by
//                 InvalidProtocolException on both sides.
//               - GCAddInjuriousCreature carries a whole character
//                 name: maxNameLength in the setter, in write() and in
//                 read(), with the factory max budgeting it.
//               - PCSlayerInfo3's, PCVampireInfo3's and
//                 PCOustersInfo3's copy constructors carry every member
//                 write() emits, the union id included. All three
//                 records are copied by value into the packets that
//                 hold one, so a dropped member was a field the sender
//                 set and the wire never saw.
//               - GCChangeWeather::read tests the raw byte against
//                 WEATHER_MAX before it reaches an enum that declares
//                 fewer values than a byte carries, and toString()
//                 prints a weather Weather2String does not name as its
//                 number.
//               - GCKnocksTargetBackOK1::read and
//                 GCKnocksTargetBackOK5::read take the success flag as
//                 a BYTE and refuse anything but 0 or 1 rather than
//                 storing an invalid bool.
//               - GCExecuteElement bounds the condition byte against
//                 the four conditions its own header names in the
//                 setter, in write() and in read().
//               - GCMorphVampire2's accessor for the vampire record it
//                 holds is getVampireInfo.
//               - CGBloodDrain::read and ::write carry the object id
//                 through the typed stream calls every other packet in
//                 the set uses. szObjectID is sizeof(ObjectID_t), so
//                 the bytes are unchanged.
//               - All forty-two initialise every member their write()
//                 emits, so a packet sent without every setter called
//                 puts no indeterminate byte on the wire. The
//                 poisoned-storage pin at the end of the file asserts
//                 that for forty-one of them; GCMorph1 holds its
//                 records by pointer, and what a fresh one holds is
//                 the refusal above.
//
//////////////////////////////////////////////////////////////////////

#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CGAbsorbSoul.h"
#include "CGBloodDrain.h"
#include "CGGetOffMotorCycle.h"
#include "CGResurrect.h"
#include "CGRideMotorCycle.h"
#include "CGSilverCoating.h"
#include "CGTameMonster.h"
#include "CGVisible.h"
#include "Exception.h"
#include "GCAddInjuriousCreature.h"
#include "GCBloodDrainOK1.h"
#include "GCBloodDrainOK2.h"
#include "GCBloodDrainOK3.h"
#include "GCChangeDarkLight.h"
#include "GCChangeShape.h"
#include "GCChangeWeather.h"
#include "GCCrossCounterOK1.h"
#include "GCCrossCounterOK2.h"
#include "GCCrossCounterOK3.h"
#include "GCExecuteElement.h"
#include "GCGetOffMotorCycle.h"
#include "GCGetOffMotorCycleFailed.h"
#include "GCGetOffMotorCycleOK.h"
#include "GCHPRecoveryStartToOthers.h"
#include "GCHPRecoveryStartToSelf.h"
#include "GCKnocksTargetBackOK1.h"
#include "GCKnocksTargetBackOK2.h"
#include "GCKnocksTargetBackOK4.h"
#include "GCKnocksTargetBackOK5.h"
#include "GCLightning.h"
#include "GCMPRecoveryStart.h"
#include "GCMorph1.h"
#include "GCMorphSlayer2.h"
#include "GCMorphVampire2.h"
#include "GCPetUseSkill.h"
#include "GCRemoveCorpseHead.h"
#include "GCRideMotorCycle.h"
#include "GCRideMotorCycleFailed.h"
#include "GCRideMotorCycleOK.h"
#include "GCSearchMotorcycleFail.h"
#include "GCSearchMotorcycleOK.h"
#include "GCVisibleFail.h"
#include "GCVisibleOK.h"
#include "PCOustersInfo2.h"
#include "PCOustersInfo3.h"
#include "TestStreams.h"

using wiretest::expectGolden;
using wiretest::kEncryptCodeCount;
using wiretest::kEncryptCodes;
using wiretest::Loopback;
using wiretest::roundTrip;
using wiretest::writeBody;

namespace {

// The unencrypted branch. No packet in this file rides the encrypter, so
// this is the only code whose bytes could differ from any other.
const uchar kPlainCode = 0;

// PC attributes are WORDs whose getters refuse anything above 2000, so
// the high byte of an attribute fixture cannot be >= 128. The low byte
// still is, and the nine values of a race stay distinct.
const Attr_t kAttrs[9] = {0x0781, 0x0792, 0x07A3, 0x0784, 0x0795, 0x07A6, 0x0787, 0x0798, 0x07A9};

// Put arbitrary bytes on the wire and hand them to a reader, for the
// bodies no setter can build.
template <typename Emit, typename Consume> void throughLoopback(Emit emit, Consume consume) {
    Loopback link;
    link.setCodes(kPlainCode);
    emit(link.out());
    const uint length = link.out().length();
    link.pump(length);
    consume(link.in());
}

//////////////////////////////////////////////////////////////////////
// The three pins every packet gets. fill() / expectEqual() are
// overloaded per packet, so the same canonical instance feeds all three.
// Both take a non-const reference: several getters are not const, and
// emptying a stat record goes through a destructive accessor.
//////////////////////////////////////////////////////////////////////

#define CREATURE_PACKET_GOLDEN_AND_SIZE(Name)                                                        \
    TEST(Name##Test, bodyBytesMatchGolden) {                                                         \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        const std::vector<unsigned char> body = writeBody(packet, kPlainCode);                       \
        expectGolden(#Name, kPlainCode, body);                                                       \
        for (size_t i = 1; i < kEncryptCodeCount; i++)                                               \
            EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))                                     \
                << #Name " now varies with the encrypt code - add per-code goldens";                 \
    }                                                                                                \
    TEST(Name##Test, sizeMatchesTheBytesWrittenAndFitsTheFactoryMax) {                               \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        Name##Factory factory;                                                                       \
        EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size())              \
            << #Name ": getPacketSize() disagrees with the bytes write() emits; writePacket() puts " \
                     "the former on the wire, so the stream never resynchronises";                   \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize())                                \
            << #Name ": the body outgrows the read buffer the receiver sizes from the factory max";  \
        EXPECT_EQ(factory.getPacketID(), packet.getPacketID());                                      \
        EXPECT_EQ(factory.getPacketName(), packet.getPacketName());                                  \
    }

#define CREATURE_PACKET_TESTS(Name)               \
    TEST(Name##Test, roundTripsThroughLoopback) { \
        Name src;                                 \
        fill(src);                                \
        Name dst;                                 \
        roundTrip(src, dst, kPlainCode);          \
        expectEqual(src, dst);                    \
    }                                             \
    CREATURE_PACKET_GOLDEN_AND_SIZE(Name)

// A second fixture for a packet whose write() has a branch the canonical
// one does not take.
#define CREATURE_PACKET_VARIANT(Name, Variant, fillVariant)                    \
    TEST(Name##Test, Variant##BodyBytesMatchGolden) {                          \
        Name packet;                                                           \
        fillVariant(packet);                                                   \
        const std::vector<unsigned char> body = writeBody(packet, kPlainCode); \
        expectGolden(#Name "." #Variant, kPlainCode, body);                    \
        EXPECT_EQ((size_t)packet.getPacketSize(), body.size());                \
        Name##Factory factory;                                                 \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());         \
        Name dst;                                                              \
        roundTrip(packet, dst, kPlainCode);                                    \
        expectEqual(packet, dst);                                              \
    }

//////////////////////////////////////////////////////////////////////
// The stat record six of the packets embed. Its type tags are
// enumerators; its values are full-width ushorts and DWORDs with every
// byte >= 128.
//////////////////////////////////////////////////////////////////////

void fillModifyInfo(ModifyInfo& info) {
    info.addShortData(MODIFY_CURRENT_HP, 0x81A2);
    info.addShortData(MODIFY_CURRENT_MP, 0x83A4);
    info.addLongData(MODIFY_ALIGNMENT, 0x85A6B7C8);
}

// ModifyInfo exposes its lists only through the destructive popShortData
// / popLongData, so comparing two records empties both. Nothing reads
// them afterwards.
void expectModifyInfoEqual(ModifyInfo& a, ModifyInfo& b) {
    ASSERT_EQ(a.getShortCount(), b.getShortCount());
    ASSERT_EQ(a.getLongCount(), b.getLongCount());

    const int shorts = (int)a.getShortCount();
    for (int i = 0; i < shorts; i++) {
        SHORTDATA left, right;
        a.popShortData(left);
        b.popShortData(right);
        EXPECT_EQ((int)left.type, (int)right.type) << "short entry " << i;
        EXPECT_EQ(left.value, right.value) << "short entry " << i;
    }

    const int longs = (int)a.getLongCount();
    for (int i = 0; i < longs; i++) {
        LONGDATA left, right;
        a.popLongData(left);
        b.popLongData(right);
        EXPECT_EQ((int)left.type, (int)right.type) << "long entry " << i;
        EXPECT_EQ(left.value, right.value) << "long entry " << i;
    }
}

//////////////////////////////////////////////////////////////////////
// Getting on and off a motorcycle.
//////////////////////////////////////////////////////////////////////

// Tile coordinates are BYTEs throughout the protocol.
void fill(CGRideMotorCycle& packet) {
    packet.setObjectID(0x81A2B3C4);
    packet.setX(0x85);
    packet.setY(0x96);
}

void expectEqual(CGRideMotorCycle& a, CGRideMotorCycle& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
}

CREATURE_PACKET_TESTS(CGRideMotorCycle)

void fill(CGGetOffMotorCycle& packet) {
    packet.setObjectID(0x82A3B4C5);
}

void expectEqual(CGGetOffMotorCycle& a, CGGetOffMotorCycle& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(CGGetOffMotorCycle)

void fill(GCRideMotorCycle& packet) {
    packet.setObjectID(0x83A4B5C6);
    packet.setTargetObjectID(0x87A8B9CA);
}

void expectEqual(GCRideMotorCycle& a, GCRideMotorCycle& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getTargetObjectID(), b.getTargetObjectID());
}

CREATURE_PACKET_TESTS(GCRideMotorCycle)

void fill(GCRideMotorCycleOK& packet) {
    packet.setObjectID(0x8BACBDCE);
}

void expectEqual(GCRideMotorCycleOK& a, GCRideMotorCycleOK& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(GCRideMotorCycleOK)

void fill(GCRideMotorCycleFailed& packet) {
    packet.setObjectID(0x8FB0C1D2);
}

void expectEqual(GCRideMotorCycleFailed& a, GCRideMotorCycleFailed& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(GCRideMotorCycleFailed)

void fill(GCGetOffMotorCycle& packet) {
    packet.setObjectID(0x93B4C5D6);
}

void expectEqual(GCGetOffMotorCycle& a, GCGetOffMotorCycle& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(GCGetOffMotorCycle)

// An empty body: the rider is told it worked and nothing else.
void fill(GCGetOffMotorCycleOK&) {}

void expectEqual(GCGetOffMotorCycleOK&, GCGetOffMotorCycleOK&) {}

CREATURE_PACKET_TESTS(GCGetOffMotorCycleOK)

// An empty body: the refusal carries no reason.
void fill(GCGetOffMotorCycleFailed&) {}

void expectEqual(GCGetOffMotorCycleFailed&, GCGetOffMotorCycleFailed&) {}

CREATURE_PACKET_TESTS(GCGetOffMotorCycleFailed)

// The two coordinates name a tile in the zone the id picks, so they are
// BYTEs like every other tile coordinate.
void fill(GCSearchMotorcycleOK& packet) {
    packet.setZoneID(0x97B8);
    packet.setX(0x9B);
    packet.setY(0xAC);
}

void expectEqual(GCSearchMotorcycleOK& a, GCSearchMotorcycleOK& b) {
    EXPECT_EQ(a.getZoneID(), b.getZoneID());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
}

CREATURE_PACKET_TESTS(GCSearchMotorcycleOK)

void fill(GCSearchMotorcycleFail&) {}

void expectEqual(GCSearchMotorcycleFail&, GCSearchMotorcycleFail&) {}

CREATURE_PACKET_TESTS(GCSearchMotorcycleFail)

//////////////////////////////////////////////////////////////////////
// The bite and its three results.
//////////////////////////////////////////////////////////////////////

void fill(CGBloodDrain& packet) {
    packet.setObjectID(0x9DBEBFD0);
}

void expectEqual(CGBloodDrain& a, CGBloodDrain& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(CGBloodDrain)

void fill(GCBloodDrainOK1& packet) {
    packet.setObjectID(0xA1C2D3E4);
    fillModifyInfo(packet);
}

void expectEqual(GCBloodDrainOK1& a, GCBloodDrainOK1& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCBloodDrainOK1)

// The empty stat record: two zero count bytes and nothing behind them,
// the shape a bite that moved nothing sends.
void fillNoStats(GCBloodDrainOK1& packet) {
    packet.setObjectID(0xA5C6D7E8);
}

CREATURE_PACKET_VARIANT(GCBloodDrainOK1, nostats, fillNoStats)

void fill(GCBloodDrainOK2& packet) {
    packet.setObjectID(0xA9CADBEC);
    fillModifyInfo(packet);
}

void expectEqual(GCBloodDrainOK2& a, GCBloodDrainOK2& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCBloodDrainOK2)

void fill(GCBloodDrainOK3& packet) {
    packet.setObjectID(0xADCEDFE0);
    packet.setTargetObjectID(0xB1D2E3F4);
}

void expectEqual(GCBloodDrainOK3& a, GCBloodDrainOK3& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getTargetObjectID(), b.getTargetObjectID());
}

CREATURE_PACKET_TESTS(GCBloodDrainOK3)

//////////////////////////////////////////////////////////////////////
// The morph: a whole character sheet, then the zone's view of it.
//////////////////////////////////////////////////////////////////////

// InventorySlotInfo, GearSlotInfo and ExtraSlotInfo are separate classes
// over the same PCItemInfo record.
template <typename SlotInfo> void fillItemSlot(SlotInfo* pSlot, ObjectID_t objectID, int subItems) {
    pSlot->setObjectID(objectID);
    pSlot->setItemClass(0x81);
    pSlot->setItemType(0x8293);
    pSlot->addOptionType(0x84);
    pSlot->setDurability(0x8798A9BA);
    pSlot->setSilver(0x8BAC);
    pSlot->setGrade(0x8DBECFD0);
    pSlot->setEnchantLevel((EnchantLevel_t)0x91);
    pSlot->setItemNum(0x92);
    pSlot->setMainColor(0x93A4);
    for (int i = 0; i < subItems; i++) {
        SubItemInfo* pSub = new SubItemInfo();
        pSub->setObjectID((ObjectID_t)(0x95A6B7C8 + i));
        pSub->setItemClass((BYTE)(0x99 + i));
        pSub->setItemType((ItemType_t)(0x9AAB + i));
        pSub->setItemNum((ItemNum_t)(0x9C + i));
        pSub->setSlotID((SlotID_t)(0x9D + i));
        // addListElement maintains the count on every list record, so the
        // sub-item count is always derived from the list.
        pSlot->addListElement(pSub);
    }
}

void expectItemEqual(PCItemInfo& a, PCItemInfo& b, const char* what) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID()) << what;
    EXPECT_EQ(a.getItemClass(), b.getItemClass()) << what;
    EXPECT_EQ(a.getItemType(), b.getItemType()) << what;
    EXPECT_EQ(a.getOptionType(), b.getOptionType()) << what;
    EXPECT_EQ(a.getDurability(), b.getDurability()) << what;
    EXPECT_EQ(a.getSilver(), b.getSilver()) << what;
    EXPECT_EQ(a.getGrade(), b.getGrade()) << what;
    EXPECT_EQ(a.getEnchantLevel(), b.getEnchantLevel()) << what;
    EXPECT_EQ(a.getItemNum(), b.getItemNum()) << what;
    EXPECT_EQ(a.getMainColor(), b.getMainColor()) << what;
    ASSERT_EQ(a.getListNum(), b.getListNum()) << what;
    for (BYTE i = 0; i < a.getListNum(); i++) {
        SubItemInfo* x = a.popFrontListElement();
        SubItemInfo* y = b.popFrontListElement();
        EXPECT_EQ(x->getObjectID(), y->getObjectID()) << what;
        EXPECT_EQ(x->getItemClass(), y->getItemClass()) << what;
        EXPECT_EQ(x->getItemType(), y->getItemType()) << what;
        EXPECT_EQ(x->getItemNum(), y->getItemNum()) << what;
        EXPECT_EQ(x->getSlotID(), y->getSlotID()) << what;
        delete x;
        delete y;
    }
}

// Sex and hair style are enum bytes with two and three enumerators, so
// they carry their highest valid value rather than a high byte. The
// guild name is text.
void fillSlayerInfo2(PCSlayerInfo2* pInfo) {
    pInfo->setObjectID(0x81A2B3C4);
    pInfo->setName("MorphSlayer");
    pInfo->setSex(MALE);
    pInfo->setHairStyle(HAIR_STYLE3);
    pInfo->setHairColor(0x85C6);
    pInfo->setSkinColor(0x87C8);
    pInfo->setMasterEffectColor(0x89);
    pInfo->setAlignment((Alignment_t)0x8AABBCCD);
    pInfo->setSTR(kAttrs[0], ATTR_CURRENT);
    pInfo->setSTR(kAttrs[1], ATTR_MAX);
    pInfo->setSTR(kAttrs[2], ATTR_BASIC);
    pInfo->setDEX(kAttrs[3], ATTR_CURRENT);
    pInfo->setDEX(kAttrs[4], ATTR_MAX);
    pInfo->setDEX(kAttrs[5], ATTR_BASIC);
    pInfo->setINT(kAttrs[6], ATTR_CURRENT);
    pInfo->setINT(kAttrs[7], ATTR_MAX);
    pInfo->setINT(kAttrs[8], ATTR_BASIC);
    pInfo->setRank(0x8E);
    pInfo->setRankExp(0x8FA0B1C2);
    pInfo->setSTRExp(0x93A4B5C6);
    pInfo->setDEXExp(0x97A8B9CA);
    pInfo->setINTExp(0x9BACBDCE);
    pInfo->setHP(0x9FE0, ATTR_CURRENT);
    pInfo->setHP(0xA1E2, ATTR_MAX);
    pInfo->setMP(0xA3E4, ATTR_CURRENT);
    pInfo->setMP(0xA5E6, ATTR_MAX);
    pInfo->setFame(0xA7B8C9DA);
    pInfo->setGold(0xABBCCDDE);
    for (uint i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        pInfo->setSkillDomainLevel((SkillDomain)i, (SkillLevel_t)(0xB0 + i));
        pInfo->setSkillDomainExp((SkillDomain)i, (SkillExp_t)(0xB6C7D8E9 + i));
    }
    pInfo->setSight(0xBC);
    for (BYTE i = 0; i < 4; i++)
        pInfo->setHotKey(i, (SkillType_t)(0xBDCE + i * 0x0101));
    pInfo->setCompetence(0xC2);
    pInfo->setGuildID(0xC3D4);
    pInfo->setGuildName("MorphSlayerGuild");
    pInfo->setGuildMemberRank(0xC5);
    pInfo->setUnionID(0xC6D7E8F9);
    pInfo->setAdvancementLevel(0xCA);
    pInfo->setAdvancementGoalExp(0xCBDCEDFE);
    pInfo->setAttrBonus(0xCFE0);
}

void expectSlayerInfo2Equal(PCSlayerInfo2& a, PCSlayerInfo2& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getHairStyle(), b.getHairStyle());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getMasterEffectColor(), b.getMasterEffectColor());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getSTR(ATTR_CURRENT), b.getSTR(ATTR_CURRENT));
    EXPECT_EQ(a.getSTR(ATTR_MAX), b.getSTR(ATTR_MAX));
    EXPECT_EQ(a.getSTR(ATTR_BASIC), b.getSTR(ATTR_BASIC));
    EXPECT_EQ(a.getDEX(ATTR_CURRENT), b.getDEX(ATTR_CURRENT));
    EXPECT_EQ(a.getDEX(ATTR_MAX), b.getDEX(ATTR_MAX));
    EXPECT_EQ(a.getDEX(ATTR_BASIC), b.getDEX(ATTR_BASIC));
    EXPECT_EQ(a.getINT(ATTR_CURRENT), b.getINT(ATTR_CURRENT));
    EXPECT_EQ(a.getINT(ATTR_MAX), b.getINT(ATTR_MAX));
    EXPECT_EQ(a.getINT(ATTR_BASIC), b.getINT(ATTR_BASIC));
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getRankExp(), b.getRankExp());
    EXPECT_EQ(a.getSTRExp(), b.getSTRExp());
    EXPECT_EQ(a.getDEXExp(), b.getDEXExp());
    EXPECT_EQ(a.getINTExp(), b.getINTExp());
    EXPECT_EQ(a.getHP(ATTR_CURRENT), b.getHP(ATTR_CURRENT));
    EXPECT_EQ(a.getHP(ATTR_MAX), b.getHP(ATTR_MAX));
    EXPECT_EQ(a.getMP(ATTR_CURRENT), b.getMP(ATTR_CURRENT));
    EXPECT_EQ(a.getMP(ATTR_MAX), b.getMP(ATTR_MAX));
    EXPECT_EQ(a.getFame(), b.getFame());
    EXPECT_EQ(a.getGold(), b.getGold());
    for (uint i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        EXPECT_EQ(a.getSkillDomainLevel((SkillDomain)i), b.getSkillDomainLevel((SkillDomain)i)) << "domain " << i;
        EXPECT_EQ(a.getSkillDomainExp((SkillDomain)i), b.getSkillDomainExp((SkillDomain)i)) << "domain " << i;
    }
    EXPECT_EQ(a.getSight(), b.getSight());
    for (BYTE i = 0; i < 4; i++)
        EXPECT_EQ(a.getHotKey(i), b.getHotKey(i)) << "hot key " << (int)i;
    EXPECT_EQ(a.getCompetence(), b.getCompetence());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getGuildName(), b.getGuildName());
    EXPECT_EQ(a.getGuildMemberRank(), b.getGuildMemberRank());
    EXPECT_EQ(a.getUnionID(), b.getUnionID());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
    EXPECT_EQ(a.getAdvancementGoalExp(), b.getAdvancementGoalExp());
    EXPECT_EQ(a.getAttrBonus(), b.getAttrBonus());
}

// Sex is an enum byte with two enumerators, so it carries an enumerator
// rather than a high byte. The guild name is text.
void fillVampireInfo2(PCVampireInfo2* pInfo) {
    pInfo->setObjectID(0x82A3B4C5);
    pInfo->setName("MorphVampire");
    pInfo->setLevel(0x86);
    pInfo->setSex(FEMALE);
    pInfo->setBatColor(0x87C8);
    pInfo->setSkinColor(0x89CA);
    pInfo->setMasterEffectColor(0x8B);
    pInfo->setAlignment((Alignment_t)0x8CADBECF);
    pInfo->setSTR(kAttrs[8], ATTR_CURRENT);
    pInfo->setSTR(kAttrs[7], ATTR_MAX);
    pInfo->setSTR(kAttrs[6], ATTR_BASIC);
    pInfo->setDEX(kAttrs[5], ATTR_CURRENT);
    pInfo->setDEX(kAttrs[4], ATTR_MAX);
    pInfo->setDEX(kAttrs[3], ATTR_BASIC);
    pInfo->setINT(kAttrs[2], ATTR_CURRENT);
    pInfo->setINT(kAttrs[1], ATTR_MAX);
    pInfo->setINT(kAttrs[0], ATTR_BASIC);
    pInfo->setHP(0x90E1, ATTR_CURRENT);
    pInfo->setHP(0x92E3, ATTR_MAX);
    pInfo->setRank(0x94);
    pInfo->setRankExp(0x95A6B7C8);
    pInfo->setExp(0x99AABBCC);
    pInfo->setGold(0x9DAEBFD0);
    pInfo->setFame(0xA1B2C3D4);
    pInfo->setSight(0xA5);
    pInfo->setBonus(0xA6B7);
    for (BYTE i = 0; i < 8; i++)
        pInfo->setHotKey(i, (SkillType_t)(0xA8B9 + i * 0x0101));
    pInfo->setSilverDamage(0xB8C9);
    pInfo->setCompetence(0xBA);
    pInfo->setGuildID(0xBBCC);
    pInfo->setGuildName("MorphVampireGuild");
    pInfo->setGuildMemberRank(0xBD);
    pInfo->setUnionID(0xBECFD0E1);
    pInfo->setAdvancementLevel(0xC2);
    pInfo->setAdvancementGoalExp(0xC3D4E5F6);
}

void expectVampireInfo2Equal(PCVampireInfo2& a, PCVampireInfo2& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getLevel(), b.getLevel());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getBatColor(), b.getBatColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getMasterEffectColor(), b.getMasterEffectColor());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getSTR(ATTR_CURRENT), b.getSTR(ATTR_CURRENT));
    EXPECT_EQ(a.getSTR(ATTR_MAX), b.getSTR(ATTR_MAX));
    EXPECT_EQ(a.getSTR(ATTR_BASIC), b.getSTR(ATTR_BASIC));
    EXPECT_EQ(a.getDEX(ATTR_CURRENT), b.getDEX(ATTR_CURRENT));
    EXPECT_EQ(a.getDEX(ATTR_MAX), b.getDEX(ATTR_MAX));
    EXPECT_EQ(a.getDEX(ATTR_BASIC), b.getDEX(ATTR_BASIC));
    EXPECT_EQ(a.getINT(ATTR_CURRENT), b.getINT(ATTR_CURRENT));
    EXPECT_EQ(a.getINT(ATTR_MAX), b.getINT(ATTR_MAX));
    EXPECT_EQ(a.getINT(ATTR_BASIC), b.getINT(ATTR_BASIC));
    EXPECT_EQ(a.getHP(ATTR_CURRENT), b.getHP(ATTR_CURRENT));
    EXPECT_EQ(a.getHP(ATTR_MAX), b.getHP(ATTR_MAX));
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getRankExp(), b.getRankExp());
    EXPECT_EQ(a.getExp(), b.getExp());
    EXPECT_EQ(a.getGold(), b.getGold());
    EXPECT_EQ(a.getFame(), b.getFame());
    EXPECT_EQ(a.getSight(), b.getSight());
    EXPECT_EQ(a.getBonus(), b.getBonus());
    for (BYTE i = 0; i < 8; i++)
        EXPECT_EQ(a.getHotKey(i), b.getHotKey(i)) << "hot key " << (int)i;
    EXPECT_EQ(a.getSilverDamage(), b.getSilverDamage());
    EXPECT_EQ(a.getCompetence(), b.getCompetence());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getGuildName(), b.getGuildName());
    EXPECT_EQ(a.getGuildMemberRank(), b.getGuildMemberRank());
    EXPECT_EQ(a.getUnionID(), b.getUnionID());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
    EXPECT_EQ(a.getAdvancementGoalExp(), b.getAdvancementGoalExp());
}

// The slayer shape, with two inventory slots (one carrying sub-items),
// a gear slot and an extra slot.
void fillMorphSlayer(GCMorph1& packet) {
    PCSlayerInfo2* pInfo = new PCSlayerInfo2();
    fillSlayerInfo2(pInfo);
    packet.setPCInfo2(pInfo);

    InventoryInfo* pInventory = new InventoryInfo();
    InventorySlotInfo* pFirst = new InventorySlotInfo();
    fillItemSlot(pFirst, 0xD1E2F3A4, 2);
    pFirst->setInvenX(0xD5);
    pFirst->setInvenY(0xE6);
    pInventory->addListElement(pFirst);
    InventorySlotInfo* pSecond = new InventorySlotInfo();
    fillItemSlot(pSecond, 0xD7E8F9AA, 0);
    pSecond->setInvenX(0xDB);
    pSecond->setInvenY(0xEC);
    pInventory->addListElement(pSecond);
    packet.setInventoryInfo(pInventory);

    GearInfo* pGear = new GearInfo();
    GearSlotInfo* pGearSlot = new GearSlotInfo();
    fillItemSlot(pGearSlot, 0xDDEEFFA0, 0);
    pGearSlot->setSlotID(0xE1);
    pGear->addListElement(pGearSlot);
    packet.setGearInfo(pGear);

    ExtraInfo* pExtra = new ExtraInfo();
    ExtraSlotInfo* pExtraSlot = new ExtraSlotInfo();
    fillItemSlot(pExtraSlot, 0xE2F3A4B5, 1);
    pExtra->addListElement(pExtraSlot);
    packet.setExtraInfo(pExtra);
}

// The vampire shape, with all three lists empty: the count byte alone
// for each, which is what a character carrying nothing sends.
void fillMorphVampire(GCMorph1& packet) {
    PCVampireInfo2* pInfo = new PCVampireInfo2();
    fillVampireInfo2(pInfo);
    packet.setPCInfo2(pInfo);
    packet.setInventoryInfo(new InventoryInfo());
    packet.setGearInfo(new GearInfo());
    packet.setExtraInfo(new ExtraInfo());
}

void expectMorphEqual(GCMorph1& a, GCMorph1& b) {
    ASSERT_TRUE(a.getPCInfo2() != NULL);
    ASSERT_TRUE(b.getPCInfo2() != NULL);
    ASSERT_EQ(a.getPCInfo2()->getPCType(), b.getPCInfo2()->getPCType());
    switch (a.getPCInfo2()->getPCType()) {
    case PC_SLAYER:
        expectSlayerInfo2Equal(*dynamic_cast<PCSlayerInfo2*>(a.getPCInfo2()),
                               *dynamic_cast<PCSlayerInfo2*>(b.getPCInfo2()));
        break;
    case PC_VAMPIRE:
        expectVampireInfo2Equal(*dynamic_cast<PCVampireInfo2*>(a.getPCInfo2()),
                                *dynamic_cast<PCVampireInfo2*>(b.getPCInfo2()));
        break;
    default:
        ADD_FAILURE() << "GCMorph1 now carries a third pc type";
        break;
    }

    ASSERT_EQ(a.getInventoryInfo()->getListNum(), b.getInventoryInfo()->getListNum());
    for (BYTE i = 0; i < a.getInventoryInfo()->getListNum(); i++) {
        InventorySlotInfo* x = a.getInventoryInfo()->popFrontListElement();
        InventorySlotInfo* y = b.getInventoryInfo()->popFrontListElement();
        expectItemEqual(*x, *y, "inventory slot");
        EXPECT_EQ(x->getInvenX(), y->getInvenX());
        EXPECT_EQ(x->getInvenY(), y->getInvenY());
        delete x;
        delete y;
    }

    ASSERT_EQ(a.getGearInfo()->getListNum(), b.getGearInfo()->getListNum());
    for (BYTE i = 0; i < a.getGearInfo()->getListNum(); i++) {
        GearSlotInfo* x = a.getGearInfo()->popFrontListElement();
        GearSlotInfo* y = b.getGearInfo()->popFrontListElement();
        expectItemEqual(*x, *y, "gear slot");
        EXPECT_EQ(x->getSlotID(), y->getSlotID());
        delete x;
        delete y;
    }

    ASSERT_EQ(a.getExtraInfo()->getListNum(), b.getExtraInfo()->getListNum());
    for (BYTE i = 0; i < a.getExtraInfo()->getListNum(); i++) {
        ExtraSlotInfo* x = a.getExtraInfo()->popFrontListElement();
        ExtraSlotInfo* y = b.getExtraInfo()->popFrontListElement();
        expectItemEqual(*x, *y, "extra slot");
        delete x;
        delete y;
    }
}

// GCMorph1 holds its records by pointer, so the round trip is written
// out rather than taken from the macro.
void morphRoundTrip(GCMorph1& src, GCMorph1& dst) {
    Loopback loopback;
    loopback.setCodes(kPlainCode);
    src.write(loopback.out());
    loopback.pump((uint)src.getPacketSize());
    dst.read(loopback.in());
}

TEST(GCMorph1Test, roundTripsThroughLoopback) {
    GCMorph1 src;
    fillMorphSlayer(src);
    GCMorph1 dst;
    morphRoundTrip(src, dst);
    expectMorphEqual(src, dst);
}

TEST(GCMorph1Test, bodyBytesMatchGolden) {
    GCMorph1 packet;
    fillMorphSlayer(packet);
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCMorph1", kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))
            << "GCMorph1 now varies with the encrypt code - add per-code goldens";
}

TEST(GCMorph1Test, vampireBodyBytesMatchGolden) {
    GCMorph1 packet;
    fillMorphVampire(packet);
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCMorph1.vampire", kPlainCode, body);

    GCMorph1Factory factory;
    EXPECT_LE(body.size(), (size_t)factory.getPacketMaxSize())
        << "GCMorph1: the vampire shape outgrows the max its factory budgets from the slayer record";

    GCMorph1 dst;
    morphRoundTrip(packet, dst);
    expectMorphEqual(packet, dst);
}

TEST(GCMorph1Test, theBodyFitsTheFactoryMaxAndTheFactoryAgreesWithThePacket) {
    GCMorph1 packet;
    fillMorphSlayer(packet);
    GCMorph1Factory factory;
    EXPECT_LE(writeBody(packet, kPlainCode).size(), (size_t)factory.getPacketMaxSize())
        << "GCMorph1: the body outgrows the read buffer the receiver sizes from the factory max";
    EXPECT_EQ(factory.getPacketID(), packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), packet.getPacketName());
}

// The declared size counts the pc-type byte write() puts in front of the
// record. writePacket() puts the declared length on the wire first, so a
// gap there is a stream that never resynchronises.
TEST(GCMorph1Test, theDeclaredSizeCountsThePCTypeByte) {
    GCMorph1 slayer;
    fillMorphSlayer(slayer);
    EXPECT_EQ((size_t)slayer.getPacketSize(), writeBody(slayer, kPlainCode).size());

    GCMorph1 vampire;
    fillMorphVampire(vampire);
    EXPECT_EQ((size_t)vampire.getPacketSize(), writeBody(vampire, kPlainCode).size());
}

// The four records start empty, and both halves that emit them refuse a
// packet missing any one rather than following a null pointer.
TEST(GCMorph1Test, aMissingRecordIsRefusedByBothSizeAndWrite) {
    GCMorph1 empty;
    EXPECT_TRUE(empty.getPCInfo2() == NULL);
    EXPECT_TRUE(empty.getInventoryInfo() == NULL);
    EXPECT_TRUE(empty.getGearInfo() == NULL);
    EXPECT_TRUE(empty.getExtraInfo() == NULL);
    EXPECT_THROW(empty.getPacketSize(), InvalidProtocolException);
    EXPECT_THROW(writeBody(empty, kPlainCode), InvalidProtocolException);

    // The PC record set and the other three forgotten.
    GCMorph1 partial;
    PCSlayerInfo2* pInfo = new PCSlayerInfo2();
    fillSlayerInfo2(pInfo);
    partial.setPCInfo2(pInfo);
    EXPECT_THROW(partial.getPacketSize(), InvalidProtocolException);
    EXPECT_THROW(writeBody(partial, kPlainCode), InvalidProtocolException);
}

// The records belong to the packet, so reading into one that already
// holds four replaces them rather than leaking them.
TEST(GCMorph1Test, readingTwiceReplacesTheRecordsItHolds) {
    GCMorph1 src;
    fillMorphSlayer(src);

    GCMorph1 dst;
    morphRoundTrip(src, dst);
    PCInfo* pFirst = dst.getPCInfo2();
    ASSERT_TRUE(pFirst != NULL);

    GCMorph1 second;
    fillMorphVampire(second);
    morphRoundTrip(second, dst);
    ASSERT_TRUE(dst.getPCInfo2() != NULL);
    EXPECT_EQ(PC_VAMPIRE, dst.getPCInfo2()->getPCType());
    expectMorphEqual(second, dst);
}

// read() refuses a pc-type byte that is neither 'S' nor 'V', and write()
// refuses a record of a third race, so the two halves agree on the two
// shapes the packet carries.
TEST(GCMorph1Test, aThirdRaceIsRefusedOnBothSides) {
    GCMorph1 packet;
    PCOustersInfo2* pInfo = new PCOustersInfo2();
    pInfo->setName("MorphOusters");
    packet.setPCInfo2(pInfo);
    packet.setInventoryInfo(new InventoryInfo());
    packet.setGearInfo(new GearInfo());
    packet.setExtraInfo(new ExtraInfo());
    EXPECT_THROW(writeBody(packet, kPlainCode), Throwable);
}

// The zone's view of the same change, one packet per resulting race.
// The outlook fields are bit slices of one DWORD bitset and hold
// enumerators, so each carries its highest valid enumerator rather than
// a high byte; the name is text.
void fillSlayerInfo3(PCSlayerInfo3& info) {
    info.setObjectID(0x83A4B5C6);
    info.setName("MorphedSlayer");
    info.setX(0x87);
    info.setY(0x98);
    info.setDir(0xA9);
    info.setSex(MALE);
    info.setHairStyle(HAIR_STYLE3);
    info.setHelmetType(HELMET3);
    info.setJacketType(JACKET4);
    info.setPantsType(PANTS4);
    info.setWeaponType(WEAPON_MACE);
    info.setShieldType(SHIELD2);
    info.setMotorcycleType(MOTORCYCLE3);
    info.setShoulderType(3);
    info.setHairColor(0x8AB1);
    info.setSkinColor(0x8CB3);
    info.setHelmetColor(0x8EB5);
    info.setJacketColor(0x90B7, MAIN_COLOR);
    info.setJacketColor(0x92B9, SUB_COLOR);
    info.setPantsColor(0x94BB, MAIN_COLOR);
    info.setPantsColor(0x96BD, SUB_COLOR);
    info.setWeaponColor(0x98BF);
    info.setShieldColor(0x9AC1);
    info.setMotorcycleColor(0x9CC3);
    info.setShoulderColor(0x9EC5);
    info.setMasterEffectColor(0xA0);
    info.setCurrentHP(0xA1C6);
    info.setMaxHP(0xA3C8);
    info.setAttackSpeed(0xA5);
    info.setAlignment((Alignment_t)0xA6B7C8D9);
    info.setCompetence(0xAA);
    info.setGuildID(0xABBC);
    info.setUnionID(0xADBECFD0);
    info.setRank(0xB1);
    info.setAdvancementLevel(0xB2);
}

void expectSlayerInfo3Equal(const PCSlayerInfo3& a, const PCSlayerInfo3& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getHairStyle(), b.getHairStyle());
    EXPECT_EQ(a.getHelmetType(), b.getHelmetType());
    EXPECT_EQ(a.getJacketType(), b.getJacketType());
    EXPECT_EQ(a.getPantsType(), b.getPantsType());
    EXPECT_EQ(a.getWeaponType(), b.getWeaponType());
    EXPECT_EQ(a.getShieldType(), b.getShieldType());
    EXPECT_EQ(a.getMotorcycleType(), b.getMotorcycleType());
    EXPECT_EQ(a.getShoulderType(), b.getShoulderType());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getHelmetColor(), b.getHelmetColor());
    EXPECT_EQ(a.getJacketColor(MAIN_COLOR), b.getJacketColor(MAIN_COLOR));
    EXPECT_EQ(a.getJacketColor(SUB_COLOR), b.getJacketColor(SUB_COLOR));
    EXPECT_EQ(a.getPantsColor(MAIN_COLOR), b.getPantsColor(MAIN_COLOR));
    EXPECT_EQ(a.getPantsColor(SUB_COLOR), b.getPantsColor(SUB_COLOR));
    EXPECT_EQ(a.getWeaponColor(), b.getWeaponColor());
    EXPECT_EQ(a.getShieldColor(), b.getShieldColor());
    EXPECT_EQ(a.getMotorcycleColor(), b.getMotorcycleColor());
    EXPECT_EQ(a.getShoulderColor(), b.getShoulderColor());
    EXPECT_EQ(a.getMasterEffectColor(), b.getMasterEffectColor());
    EXPECT_EQ(a.getCurrentHP(), b.getCurrentHP());
    EXPECT_EQ(a.getMaxHP(), b.getMaxHP());
    EXPECT_EQ(a.getAttackSpeed(), b.getAttackSpeed());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getCompetence(), b.getCompetence());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getUnionID(), b.getUnionID());
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
}

PCSlayerInfo3 canonicalSlayerInfo3() {
    PCSlayerInfo3 info;
    fillSlayerInfo3(info);
    return info;
}

TEST(GCMorphSlayer2Test, roundTripsThroughLoopback) {
    GCMorphSlayer2 src(canonicalSlayerInfo3());
    GCMorphSlayer2 dst;
    roundTrip(src, dst, kPlainCode);
    expectSlayerInfo3Equal(src.getSlayerInfo(), dst.getSlayerInfo());
}

TEST(GCMorphSlayer2Test, bodyBytesMatchGolden) {
    GCMorphSlayer2 packet(canonicalSlayerInfo3());
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCMorphSlayer2", kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))
            << "GCMorphSlayer2 now varies with the encrypt code - add per-code goldens";
}

TEST(GCMorphSlayer2Test, sizeMatchesTheBytesWrittenAndFitsTheFactoryMax) {
    GCMorphSlayer2 packet(canonicalSlayerInfo3());
    GCMorphSlayer2Factory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size());
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
    EXPECT_EQ(factory.getPacketID(), packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), packet.getPacketName());
}

// Sex is an enum byte with two enumerators and the coat type travels in
// a single byte, so neither carries a high byte; the name is text.
void fillVampireInfo3(PCVampireInfo3& info) {
    info.setObjectID(0x84A5B6C7);
    info.setName("MorphedVampire");
    info.setX(0x88);
    info.setY(0x99);
    info.setDir(0xAA);
    info.setSex(FEMALE);
    info.setCoatType(VAMPIRE_COAT4);
    info.setBatColor(0x8BB2);
    info.setSkinColor(0x8DB4);
    info.setCoatColor(0x8FB6, MAIN_COLOR);
    info.setCoatColor(0x91B8, SUB_COLOR);
    info.setMasterEffectColor(0x93);
    info.setCurrentHP(0x94C5);
    info.setMaxHP(0x96C7);
    info.setAttackSpeed(0x98);
    info.setAlignment((Alignment_t)0x99AABBCC);
    info.setShape(0x9D);
    info.setCompetence(0x9E);
    info.setGuildID(0x9FB0);
    info.setUnionID(0xA1B2C3D4);
    info.setRank(0xA5);
    info.setAdvancementLevel(0xA6);
}

void expectVampireInfo3Equal(const PCVampireInfo3& a, const PCVampireInfo3& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getCoatType(), b.getCoatType());
    EXPECT_EQ(a.getBatColor(), b.getBatColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getCoatColor(MAIN_COLOR), b.getCoatColor(MAIN_COLOR));
    EXPECT_EQ(a.getCoatColor(SUB_COLOR), b.getCoatColor(SUB_COLOR));
    EXPECT_EQ(a.getMasterEffectColor(), b.getMasterEffectColor());
    EXPECT_EQ(a.getCurrentHP(), b.getCurrentHP());
    EXPECT_EQ(a.getMaxHP(), b.getMaxHP());
    EXPECT_EQ(a.getAttackSpeed(), b.getAttackSpeed());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getShape(), b.getShape());
    EXPECT_EQ(a.getCompetence(), b.getCompetence());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getUnionID(), b.getUnionID());
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
}

PCVampireInfo3 canonicalVampireInfo3() {
    PCVampireInfo3 info;
    fillVampireInfo3(info);
    return info;
}

TEST(GCMorphVampire2Test, roundTripsThroughLoopback) {
    GCMorphVampire2 src(canonicalVampireInfo3());
    GCMorphVampire2 dst;
    roundTrip(src, dst, kPlainCode);
    expectVampireInfo3Equal(src.getVampireInfo(), dst.getVampireInfo());
}

TEST(GCMorphVampire2Test, bodyBytesMatchGolden) {
    GCMorphVampire2 packet(canonicalVampireInfo3());
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCMorphVampire2", kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))
            << "GCMorphVampire2 now varies with the encrypt code - add per-code goldens";
}

TEST(GCMorphVampire2Test, sizeMatchesTheBytesWrittenAndFitsTheFactoryMax) {
    GCMorphVampire2 packet(canonicalVampireInfo3());
    GCMorphVampire2Factory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size());
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
    EXPECT_EQ(factory.getPacketID(), packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), packet.getPacketName());
}

// Each holds one record, replaceable only whole, so a partly-set packet
// is unreachable. What a fresh one holds is pinned here instead: the
// record's empty name, which its own write() refuses.
TEST(GCMorphSlayer2Test, aFreshPacketRefusesToWrite) {
    GCMorphSlayer2 packet;
    EXPECT_THROW(writeBody(packet, kPlainCode), Throwable);
}

TEST(GCMorphVampire2Test, aFreshPacketRefusesToWrite) {
    GCMorphVampire2 packet;
    EXPECT_THROW(writeBody(packet, kPlainCode), Throwable);
}

// Both morph packets take their record by value, so what the copy
// constructor carries is what reaches the wire. The copy is made over
// poisoned storage, so a member the constructor skips shows up as the
// poison rather than as whatever the stack happened to hold.
template <typename RecordType> RecordType copyOverPoison(unsigned char poison, const RecordType& source) {
    alignas(RecordType) unsigned char storage[sizeof(RecordType)];
    memset(storage, poison, sizeof(storage));
    RecordType* pCopy = new (storage) RecordType(source);
    RecordType result = *pCopy;
    pCopy->~RecordType();
    return result;
}

template <typename RecordType> void expectTheCopyConstructorCarriesTheUnionID(const char* what, RecordType& source) {
    ASSERT_NE((uint)0, source.getUnionID()) << what << ": the fixture must set a union id";

    EXPECT_EQ(source.getUnionID(), copyOverPoison<RecordType>(0x00, source).getUnionID()) << what;
    EXPECT_EQ(source.getUnionID(), copyOverPoison<RecordType>(0xFF, source).getUnionID()) << what;

    RecordType assigned;
    assigned = source;
    EXPECT_EQ(source.getUnionID(), assigned.getUnionID()) << what;
}

TEST(GCMorphSlayer2Test, theRecordsCopyConstructorCarriesTheUnionID) {
    PCSlayerInfo3 info = canonicalSlayerInfo3();
    expectTheCopyConstructorCarriesTheUnionID("PCSlayerInfo3", info);

    GCMorphSlayer2 packet(info);
    EXPECT_EQ(info.getUnionID(), packet.getSlayerInfo().getUnionID());

    GCMorphSlayer2 dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_EQ(info.getUnionID(), dst.getSlayerInfo().getUnionID());
}

TEST(GCMorphVampire2Test, theRecordsCopyConstructorCarriesTheUnionID) {
    PCVampireInfo3 info = canonicalVampireInfo3();
    expectTheCopyConstructorCarriesTheUnionID("PCVampireInfo3", info);

    GCMorphVampire2 packet(info);
    EXPECT_EQ(info.getUnionID(), packet.getVampireInfo().getUnionID());

    GCMorphVampire2 dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_EQ(info.getUnionID(), dst.getVampireInfo().getUnionID());
}

// The third record of the same shape carries a union id the same way,
// through every packet that holds one by value.
TEST(PCOustersInfo3Test, theCopyConstructorCarriesTheUnionID) {
    PCOustersInfo3 info;
    info.setName("MorphedOusters");
    info.setUnionID(0x9CADBECF);
    expectTheCopyConstructorCarriesTheUnionID("PCOustersInfo3", info);
}

// The lighter change: the item a creature now appears to hold. The flag
// is a bit mask, so it carries SHAPE_FLAG_QUEST with the high bit set.
void fill(GCChangeShape& packet) {
    packet.setObjectID(0x85A6B7C8);
    packet.setItemClass(0x89);
    packet.setItemType(0x8A9B);
    packet.setOptionType(0x8C);
    packet.setAttackSpeed(0x8D);
    packet.setFlag(0x80 | SHAPE_FLAG_QUEST);
}

void expectEqual(GCChangeShape& a, GCChangeShape& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getItemClass(), b.getItemClass());
    EXPECT_EQ(a.getItemType(), b.getItemType());
    EXPECT_EQ(a.getOptionType(), b.getOptionType());
    EXPECT_EQ(a.getAttackSpeed(), b.getAttackSpeed());
    EXPECT_EQ(a.getFlag(), b.getFlag());
}

CREATURE_PACKET_TESTS(GCChangeShape)

//////////////////////////////////////////////////////////////////////
// What else a player asks of their own creature.
//////////////////////////////////////////////////////////////////////

// The two target coordinates are zone coordinates, a WORD each; the four
// inventory coordinates are BYTEs.
void fill(CGAbsorbSoul& packet) {
    packet.setObjectID(0x86A7B8C9);
    packet.setTargetZoneX(0x8A9B);
    packet.setTargetZoneY(0x8C9D);
    packet.setInvenObjectID(0x8EAFC0D1);
    packet.setInvenX(0x92);
    packet.setInvenY(0xA3);
    packet.setTargetInvenX(0xB4);
    packet.setTargetInvenY(0xC5);
}

void expectEqual(CGAbsorbSoul& a, CGAbsorbSoul& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getTargetZoneX(), b.getTargetZoneX());
    EXPECT_EQ(a.getTargetZoneY(), b.getTargetZoneY());
    EXPECT_EQ(a.getInvenObjectID(), b.getInvenObjectID());
    EXPECT_EQ(a.getInvenX(), b.getInvenX());
    EXPECT_EQ(a.getInvenY(), b.getInvenY());
    EXPECT_EQ(a.getTargetInvenX(), b.getTargetInvenX());
    EXPECT_EQ(a.getTargetInvenY(), b.getTargetInvenY());
}

CREATURE_PACKET_TESTS(CGAbsorbSoul)

// An empty body: the request carries nothing but its id.
void fill(CGResurrect&) {}

void expectEqual(CGResurrect&, CGResurrect&) {}

CREATURE_PACKET_TESTS(CGResurrect)

void fill(CGSilverCoating& packet) {
    packet.setObjectID(0x96B7C8D9);
}

void expectEqual(CGSilverCoating& a, CGSilverCoating& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(CGSilverCoating)

void fill(CGTameMonster& packet) {
    packet.setObjectID(0x9ABBCCDD);
}

void expectEqual(CGTameMonster& a, CGTameMonster& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(CGTameMonster)

//////////////////////////////////////////////////////////////////////
// Becoming visible again. All three bodies are empty.
//////////////////////////////////////////////////////////////////////

void fill(CGVisible&) {}

void expectEqual(CGVisible&, CGVisible&) {}

CREATURE_PACKET_TESTS(CGVisible)

void fill(GCVisibleOK&) {}

void expectEqual(GCVisibleOK&, GCVisibleOK&) {}

CREATURE_PACKET_TESTS(GCVisibleOK)

void fill(GCVisibleFail&) {}

void expectEqual(GCVisibleFail&, GCVisibleFail&) {}

CREATURE_PACKET_TESTS(GCVisibleFail)

//////////////////////////////////////////////////////////////////////
// The corpse and the enemy list.
//////////////////////////////////////////////////////////////////////

void fill(GCRemoveCorpseHead& packet) {
    packet.setObjectID(0x9EBFD0E1);
}

void expectEqual(GCRemoveCorpseHead& a, GCRemoveCorpseHead& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

CREATURE_PACKET_TESTS(GCRemoveCorpseHead)

// The name is text, and the field carries a character name.
void fill(GCAddInjuriousCreature& packet) {
    packet.setName("Duskbiter");
}

void expectEqual(GCAddInjuriousCreature& a, GCAddInjuriousCreature& b) {
    EXPECT_EQ(a.getName(), b.getName());
}

CREATURE_PACKET_TESTS(GCAddInjuriousCreature)

// A ten-byte name, the width this packet's own bounds once stopped at.
void fillMaxName(GCAddInjuriousCreature& packet) {
    packet.setName("Nightbiter");
}

CREATURE_PACKET_VARIANT(GCAddInjuriousCreature, maxname, fillMaxName)

// The name the packet carries is a character name, so it runs to
// maxNameLength on both sides and the factory max budgets a whole one.
TEST(GCAddInjuriousCreatureTest, aWholeCharacterNameIsCarried) {
    GCAddInjuriousCreature packet;
    packet.setName(std::string(maxNameLength, 'n'));
    EXPECT_EQ(maxNameLength, packet.getName().size());

    GCAddInjuriousCreatureFactory factory;
    EXPECT_EQ((PacketSize_t)(szBYTE + maxNameLength), factory.getPacketMaxSize());
    EXPECT_EQ(packet.getPacketSize(), factory.getPacketMaxSize());
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size());

    GCAddInjuriousCreature dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_EQ(packet.getName(), dst.getName());

    // Past a character name the setter cuts rather than the packet
    // refusing to write at all.
    GCAddInjuriousCreature wide;
    wide.setName(std::string(maxNameLength + 1, 'n'));
    EXPECT_EQ(maxNameLength, wide.getName().size());
}

//////////////////////////////////////////////////////////////////////
// The zone around the creature: its light, its weather, and the quest
// element that fires on it.
//////////////////////////////////////////////////////////////////////

void fill(GCLightning& packet) {
    packet.setDelay(0xA2);
}

void expectEqual(GCLightning& a, GCLightning& b) {
    EXPECT_EQ(a.getDelay(), b.getDelay());
}

CREATURE_PACKET_TESTS(GCLightning)

void fill(GCChangeDarkLight& packet) {
    packet.setDarkLevel(0xA3);
    packet.setLightLevel(0xB4);
}

void expectEqual(GCChangeDarkLight& a, GCChangeDarkLight& b) {
    EXPECT_EQ(a.getDarkLevel(), b.getDarkLevel());
    EXPECT_EQ(a.getLightLevel(), b.getLightLevel());
}

CREATURE_PACKET_TESTS(GCChangeDarkLight)

// The weather is an enumerator, so the fixture carries one rather than a
// high byte.
void fill(GCChangeWeather& packet) {
    packet.setWeather(WEATHER_SNOWY);
    packet.setWeatherLevel(0xC5);
}

void expectEqual(GCChangeWeather& a, GCChangeWeather& b) {
    EXPECT_EQ((int)a.getWeather(), (int)b.getWeather());
    EXPECT_EQ(a.getWeatherLevel(), b.getWeatherLevel());
}

CREATURE_PACKET_TESTS(GCChangeWeather)

// The weather byte is tested raw, before it reaches an enum that declares
// fewer values than a byte carries.
TEST(GCChangeWeatherTest, aWeatherPastTheLastOneIsRefused) {
    GCChangeWeather dst;
    EXPECT_THROW(throughLoopback(
                     [](SocketEncryptOutputStream& out) {
                         out.write((BYTE)WEATHER_MAX);
                         out.write((WeatherLevel_t)0x81);
                     },
                     [&dst](SocketEncryptInputStream& in) { dst.read(in); }),
                 InvalidProtocolException);
}

// The condition byte names one of the four conditions a quest element
// fires under, so the fixture carries one of them rather than a high
// byte; the setter, write() and read() all refuse a byte past them.
void fill(GCExecuteElement& packet) {
    packet.setQuestID(0xA6B7C8D9);
    packet.setCondition(GCExecuteElement::kConditionMax - 1);
    packet.setIndex(0xABBC);
}

void expectEqual(GCExecuteElement& a, GCExecuteElement& b) {
    EXPECT_EQ(a.getQuestID(), b.getQuestID());
    EXPECT_EQ(a.getCondition(), b.getCondition());
    EXPECT_EQ(a.getIndex(), b.getIndex());
}

CREATURE_PACKET_TESTS(GCExecuteElement)

TEST(GCExecuteElementTest, aConditionPastTheFourIsRefusedOnEverySide) {
    GCExecuteElement packet;
    EXPECT_THROW(packet.setCondition(GCExecuteElement::kConditionMax), InvalidProtocolException);

    GCExecuteElement dst;
    EXPECT_THROW(throughLoopback(
                     [](SocketEncryptOutputStream& out) {
                         out.write((DWORD)0xA6B7C8D9);
                         out.write((BYTE)GCExecuteElement::kConditionMax);
                         out.write((WORD)0xABBC);
                     },
                     [&dst](SocketEncryptInputStream& in) { dst.read(in); }),
                 InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// Regeneration.
//////////////////////////////////////////////////////////////////////

void fill(GCHPRecoveryStartToSelf& packet) {
    packet.setDelay(0xAD);
    packet.setPeriod(0xAEBF);
    packet.setQuantity(0xB0C1);
}

void expectEqual(GCHPRecoveryStartToSelf& a, GCHPRecoveryStartToSelf& b) {
    EXPECT_EQ(a.getDelay(), b.getDelay());
    EXPECT_EQ(a.getPeriod(), b.getPeriod());
    EXPECT_EQ(a.getQuantity(), b.getQuantity());
}

CREATURE_PACKET_TESTS(GCHPRecoveryStartToSelf)

void fill(GCHPRecoveryStartToOthers& packet) {
    packet.setObjectID(0xB2C3D4E5);
    packet.setDelay(0xB6);
    packet.setPeriod(0xB7C8);
    packet.setQuantity(0xB9CA);
}

void expectEqual(GCHPRecoveryStartToOthers& a, GCHPRecoveryStartToOthers& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getDelay(), b.getDelay());
    EXPECT_EQ(a.getPeriod(), b.getPeriod());
    EXPECT_EQ(a.getQuantity(), b.getQuantity());
}

CREATURE_PACKET_TESTS(GCHPRecoveryStartToOthers)

void fill(GCMPRecoveryStart& packet) {
    packet.setDelay(0xBB);
    packet.setPeriod(0xBCCD);
    packet.setQuantity(0xBEDF);
}

void expectEqual(GCMPRecoveryStart& a, GCMPRecoveryStart& b) {
    EXPECT_EQ(a.getDelay(), b.getDelay());
    EXPECT_EQ(a.getPeriod(), b.getPeriod());
    EXPECT_EQ(a.getQuantity(), b.getQuantity());
}

CREATURE_PACKET_TESTS(GCMPRecoveryStart)

//////////////////////////////////////////////////////////////////////
// The counter and the knockback.
//////////////////////////////////////////////////////////////////////

void fill(GCCrossCounterOK1& packet) {
    packet.setObjectID(0xC0D1E2F3);
    packet.setSkillType(0xC4D5);
    fillModifyInfo(packet);
}

void expectEqual(GCCrossCounterOK1& a, GCCrossCounterOK1& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCCrossCounterOK1)

// The empty stat record: the counter that moved nothing.
void fillNoStats(GCCrossCounterOK1& packet) {
    packet.setObjectID(0xC6D7E8F9);
    packet.setSkillType(0xCADB);
}

CREATURE_PACKET_VARIANT(GCCrossCounterOK1, nostats, fillNoStats)

void fill(GCCrossCounterOK2& packet) {
    packet.setObjectID(0xCCDDEEFF);
    packet.setSkillType(0xD0E1);
    fillModifyInfo(packet);
}

void expectEqual(GCCrossCounterOK2& a, GCCrossCounterOK2& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCCrossCounterOK2)

void fill(GCCrossCounterOK3& packet) {
    packet.setObjectID(0xD2E3F4A5);
    packet.setTargetObjectID(0xD6E7F8A9);
    packet.setSkillType(0xDAEB);
}

void expectEqual(GCCrossCounterOK3& a, GCCrossCounterOK3& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getTargetObjectID(), b.getTargetObjectID());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
}

CREATURE_PACKET_TESTS(GCCrossCounterOK3)

// The success flag is a bool, so it cannot follow the >= 128 rule.
// setXYDir takes (x, y, dir).
void fill(GCKnocksTargetBackOK1& packet) {
    packet.setSkillType(0xDCED);
    packet.setXYDir(0xDE, 0xEF, 0xF0);
    packet.setObjectID(0xE1F2A3B4);
    packet.setBulletNum(0xE5);
    packet.setSkillSuccess(true);
    fillModifyInfo(packet);
}

void expectEqual(GCKnocksTargetBackOK1& a, GCKnocksTargetBackOK1& b) {
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getBullet(), b.getBullet());
    EXPECT_EQ(a.getSkillSuccess(), b.getSkillSuccess());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCKnocksTargetBackOK1)

// The empty stat record, and the other value of the success flag.
void fillNoStats(GCKnocksTargetBackOK1& packet) {
    packet.setSkillType(0xE6F7);
    packet.setXYDir(0xE8, 0xF9, 0x8A);
    packet.setObjectID(0xEBFCADBE);
    packet.setBulletNum(0xEF);
    packet.setSkillSuccess(false);
}

CREATURE_PACKET_VARIANT(GCKnocksTargetBackOK1, nostats, fillNoStats)

void fill(GCKnocksTargetBackOK2& packet) {
    packet.setSkillType(0xF0A1);
    packet.setXYDir(0xF2, 0x83, 0x94);
    packet.setObjectID(0xF5A6B7C8);
    fillModifyInfo(packet);
}

void expectEqual(GCKnocksTargetBackOK2& a, GCKnocksTargetBackOK2& b) {
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    expectModifyInfoEqual(a, b);
}

CREATURE_PACKET_TESTS(GCKnocksTargetBackOK2)

void fill(GCKnocksTargetBackOK4& packet) {
    packet.setTargetObjectID(0xF9AABBCC);
    packet.setSkillType(0xFDAE);
    packet.setXYDir(0x8F, 0x90, 0xA1);
}

void expectEqual(GCKnocksTargetBackOK4& a, GCKnocksTargetBackOK4& b) {
    EXPECT_EQ(a.getTargetObjectID(), b.getTargetObjectID());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
}

CREATURE_PACKET_TESTS(GCKnocksTargetBackOK4)

// The success flag is a bool, so it cannot follow the >= 128 rule.
void fill(GCKnocksTargetBackOK5& packet) {
    packet.setObjectID(0xA2B3C4D5);
    packet.setTargetObjectID(0xA6B7C8D9);
    packet.setSkillSuccess(true);
    packet.setSkillType(0xAABB);
    packet.setXYDir(0xAC, 0xBD, 0xCE);
}

void expectEqual(GCKnocksTargetBackOK5& a, GCKnocksTargetBackOK5& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getTargetObjectID(), b.getTargetObjectID());
    EXPECT_EQ(a.getSkillSuccess(), b.getSkillSuccess());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
}

CREATURE_PACKET_TESTS(GCKnocksTargetBackOK5)

// The success flag arrives as a byte and is narrowed, so a byte that is
// neither 0 nor 1 is refused rather than stored in a bool.
TEST(GCKnocksTargetBackOK1Test, aSuccessFlagThatIsNotABoolIsRefused) {
    GCKnocksTargetBackOK1 dst;
    EXPECT_THROW(throughLoopback(
                     [](SocketEncryptOutputStream& out) {
                         out.write((SkillType_t)0xBCCD);
                         out.write((Dir_t)0x81);
                         out.write((Coord_t)0x82);
                         out.write((Coord_t)0x83);
                         out.write((ObjectID_t)0x84A5B6C7);
                         out.write((Bullet_t)0x85);
                         out.write((BYTE)2);
                     },
                     [&dst](SocketEncryptInputStream& in) { dst.read(in); }),
                 InvalidProtocolException);
}

TEST(GCKnocksTargetBackOK5Test, aSuccessFlagThatIsNotABoolIsRefused) {
    GCKnocksTargetBackOK5 dst;
    EXPECT_THROW(throughLoopback(
                     [](SocketEncryptOutputStream& out) {
                         out.write((ObjectID_t)0x84A5B6C7);
                         out.write((ObjectID_t)0x88A9BACB);
                         out.write((BYTE)2);
                     },
                     [&dst](SocketEncryptInputStream& in) { dst.read(in); }),
                 InvalidProtocolException);
}

void fill(GCPetUseSkill& packet) {
    packet.setAttacker(0xCFD0E1F2);
    packet.setTarget(0xD3E4F5A6);
}

void expectEqual(GCPetUseSkill& a, GCPetUseSkill& b) {
    EXPECT_EQ(a.getAttacker(), b.getAttacker());
    EXPECT_EQ(a.getTarget(), b.getTarget());
}

CREATURE_PACKET_TESTS(GCPetUseSkill)

//////////////////////////////////////////////////////////////////////
// What the default constructor leaves.
//////////////////////////////////////////////////////////////////////

// Each packet is built twice over storage poisoned with a different
// byte, so a member the constructor left alone would reach the wire as
// that byte and the two bodies would differ. `prep` sets only what
// write() refuses to run without; it touches no plain scalar.
template <typename PacketType, typename Prep>
std::vector<unsigned char> bodyOverPoison(unsigned char poison, Prep prep) {
    alignas(PacketType) unsigned char storage[sizeof(PacketType)];
    memset(storage, poison, sizeof(storage));
    PacketType* pPacket = new (storage) PacketType();
    prep(*pPacket);
    std::vector<unsigned char> body = writeBody(*pPacket, kPlainCode);
    pPacket->~PacketType();
    return body;
}

template <typename PacketType, typename Prep> void expectEveryMemberIsInitialised(const char* what, Prep prep) {
    EXPECT_EQ(bodyOverPoison<PacketType>(0x00, prep), bodyOverPoison<PacketType>(0xFF, prep))
        << what << ": its default constructor leaves a member write() emits uninitialised";
}

template <typename PacketType> void expectEveryMemberIsInitialised(const char* what) {
    expectEveryMemberIsInitialised<PacketType>(what, [](PacketType&) {});
}

TEST(CreatureConstructorTest, everyPacketInitialisesEveryMemberItWrites) {
    expectEveryMemberIsInitialised<GCGetOffMotorCycleOK>("GCGetOffMotorCycleOK");
    expectEveryMemberIsInitialised<GCGetOffMotorCycleFailed>("GCGetOffMotorCycleFailed");
    expectEveryMemberIsInitialised<GCSearchMotorcycleFail>("GCSearchMotorcycleFail");
    expectEveryMemberIsInitialised<CGResurrect>("CGResurrect");
    expectEveryMemberIsInitialised<CGVisible>("CGVisible");
    expectEveryMemberIsInitialised<GCVisibleOK>("GCVisibleOK");
    expectEveryMemberIsInitialised<GCVisibleFail>("GCVisibleFail");
    expectEveryMemberIsInitialised<GCAddInjuriousCreature>("GCAddInjuriousCreature",
                                                           [](GCAddInjuriousCreature& p) { p.setName("who"); });
    expectEveryMemberIsInitialised<CGRideMotorCycle>("CGRideMotorCycle");
    expectEveryMemberIsInitialised<CGGetOffMotorCycle>("CGGetOffMotorCycle");
    expectEveryMemberIsInitialised<GCRideMotorCycle>("GCRideMotorCycle");
    expectEveryMemberIsInitialised<GCRideMotorCycleOK>("GCRideMotorCycleOK");
    expectEveryMemberIsInitialised<GCRideMotorCycleFailed>("GCRideMotorCycleFailed");
    expectEveryMemberIsInitialised<GCGetOffMotorCycle>("GCGetOffMotorCycle");
    expectEveryMemberIsInitialised<GCSearchMotorcycleOK>("GCSearchMotorcycleOK");
    expectEveryMemberIsInitialised<CGBloodDrain>("CGBloodDrain");
    expectEveryMemberIsInitialised<GCBloodDrainOK1>("GCBloodDrainOK1");
    expectEveryMemberIsInitialised<GCBloodDrainOK2>("GCBloodDrainOK2");
    expectEveryMemberIsInitialised<GCBloodDrainOK3>("GCBloodDrainOK3");
    expectEveryMemberIsInitialised<GCChangeShape>("GCChangeShape");
    expectEveryMemberIsInitialised<CGAbsorbSoul>("CGAbsorbSoul");
    expectEveryMemberIsInitialised<CGSilverCoating>("CGSilverCoating");
    expectEveryMemberIsInitialised<CGTameMonster>("CGTameMonster");
    expectEveryMemberIsInitialised<GCRemoveCorpseHead>("GCRemoveCorpseHead");
    expectEveryMemberIsInitialised<GCLightning>("GCLightning");
    expectEveryMemberIsInitialised<GCChangeDarkLight>("GCChangeDarkLight");
    expectEveryMemberIsInitialised<GCChangeWeather>("GCChangeWeather");
    expectEveryMemberIsInitialised<GCExecuteElement>("GCExecuteElement");
    expectEveryMemberIsInitialised<GCHPRecoveryStartToSelf>("GCHPRecoveryStartToSelf");
    expectEveryMemberIsInitialised<GCHPRecoveryStartToOthers>("GCHPRecoveryStartToOthers");
    expectEveryMemberIsInitialised<GCMPRecoveryStart>("GCMPRecoveryStart");
    expectEveryMemberIsInitialised<GCCrossCounterOK1>("GCCrossCounterOK1");
    expectEveryMemberIsInitialised<GCCrossCounterOK2>("GCCrossCounterOK2");
    expectEveryMemberIsInitialised<GCCrossCounterOK3>("GCCrossCounterOK3");
    expectEveryMemberIsInitialised<GCKnocksTargetBackOK1>("GCKnocksTargetBackOK1");
    expectEveryMemberIsInitialised<GCKnocksTargetBackOK2>("GCKnocksTargetBackOK2");
    expectEveryMemberIsInitialised<GCKnocksTargetBackOK4>("GCKnocksTargetBackOK4");
    expectEveryMemberIsInitialised<GCKnocksTargetBackOK5>("GCKnocksTargetBackOK5");
    expectEveryMemberIsInitialised<GCPetUseSkill>("GCPetUseSkill");

    // The two morph packets hold a record, replaceable only whole: the
    // prep copies the one the packet built, names it, and puts it back,
    // so what the record's own default constructor left is what write()
    // emits.
    expectEveryMemberIsInitialised<GCMorphSlayer2>("GCMorphSlayer2", [](GCMorphSlayer2& p) {
        PCSlayerInfo3 info = p.getSlayerInfo();
        info.setName("M");
        p.setSlayerInfo(info);
    });
    expectEveryMemberIsInitialised<GCMorphVampire2>("GCMorphVampire2", [](GCMorphVampire2& p) {
        PCVampireInfo3 info = p.getVampireInfo();
        info.setName("M");
        p.setVampireInfo(info);
    });
}

// GCMorph1 is not in the list above: it holds its four records by
// pointer, and what a fresh one holds is pinned by the refusal in
// GCMorph1Test.aMissingRecordIsRefusedByBothSizeAndWrite instead.

} // namespace
