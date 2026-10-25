//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_zone_scan_test.cpp
// Description : Golden byte fixtures, loopback round trips and size
//               pins for the packets that populate a player's view of
//               a zone the moment Zone::addPC places the character,
//               and for the packets that later add or remove a single
//               object from that view.
//
//               The set is taken from the code that runs in that
//               window: Zone::addPC and the Zone::scan() it calls, the
//               two helpers scan() delegates to
//               (Zone::createMonsterAddPacket and sendRelicEffect),
//               and Zone::moveFastPC, which reframes an existing view
//               one object at a time. Eighteen packets, each with the
//               reason it is here:
//
//               GCAddSlayer      what scan() sends the arriving player
//                                for every visible Slayer, and what
//                                addPC broadcasts to the players who
//                                can see the arrival.
//               GCAddVampire     the same for a visible Vampire that
//                                is not hiding; its FromFlag says the
//                                character stepped out of a portal.
//               GCAddOusters     the same for a visible Ousters.
//               GCAddMonster     the default branch of
//                                createMonsterAddPacket, the packet
//                                for an ordinary or invisible monster.
//               GCAddBurrowingCreature  the branch above it: a monster
//                                or a Vampire carrying the HIDE
//                                effect, which the client draws as a
//                                mound rather than a creature.
//               GCAddBat         createMonsterAddPacket's branch for a
//               GCAddWolf        monster under TRANSFORM_TO_BAT and
//                                TRANSFORM_TO_WOLF.
//               GCAddNPC         what scan() sends for every visible
//                                NPC.
//               GCAddSlayerCorpse   the four corpse packets scan()
//               GCAddVampireCorpse  sends for an ITEM_CLASS_CORPSE on
//               GCAddOustersCorpse  a visible tile, one per race plus
//               GCAddMonsterCorpse  the monster corpse, which also
//                                carries the kill and treasure state.
//               GCAddEffect      what sendRelicEffect() puts on a
//                                monster corpse right after
//                                GCAddMonsterCorpse: the relic, shrine,
//                                blood bible, castle symbol, flag and
//                                sweeper markers a corpse can hold.
//               GCAddEffectToTile   what scan() sends for a sanctuary
//                                centre tile and for every effect that
//                                answers isBroadcastingEffect().
//               GCAddVampirePortal  what scan() sends for a
//                                VAMPIRE_PORTAL effect standing on a
//                                visible tile.
//               GCDeleteObject   the removal half: moveFastPC and every
//                                creature, item and corpse teardown
//                                path take one object back out of the
//                                view with it.
//               GCDeleteEffectFromTile  the same removal for a tile
//                                effect, the counterpart of
//                                GCAddEffectToTile.
//               GCFastMove       what moveFastPC sends the moving
//                                player before it walks the old and new
//                                viewports emitting the adds and
//                                deletes above.
//
//               Deliberately excluded:
//
//               GCSetPosition, which Zone::addPC sends before the scan,
//               is already pinned by
//               tests/packet_gameserver_handshake_test.cpp.
//
//               GCAddNewItemToZone and GCAddInstalledMineToZone, the
//               two packets scan() sends for a non-corpse item and for
//               an installed mine, use the encrypter and are pinned at
//               encrypt codes 0..5 by tests/packet_encrypter_test.cpp.
//               No packet in this file calls readEncrypt/writeEncrypt,
//               so the goldens here are recorded at code 0 only, and
//               every golden test also asserts the bytes do not vary
//               with the code, so adopting the encrypter fails loudly
//               instead of silently voiding the pin.
//
//               GCAddMonsterFromBurrowing, GCAddVampireFromBurrowing,
//               GCAddMonsterFromTransformation and
//               GCAddVampireFromTransformation belong to the unburrow
//               and untransform transitions, not to view population;
//               they carry the same optional effect record as the
//               creature adds here, so the last section pins that one
//               fallback for them, and that GCAddMonsterFromBurrowing
//               reads exactly what its write() emits.
//               GCAddHelicopter (waypoint travel),
//               GCAddInjuriousCreature (a skill result) and
//               GCRemoveEffect (effect expiry on an object already in
//               view) are likewise sent from other paths; scan() builds
//               none of them.
//
//               Each packet gets three pins (ZONE_SCAN_PACKET_TESTS):
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
//               The creature-add packets carry optional records, and
//               extra goldens cover the branches one fixture cannot:
//               GCAddSlayer.bare (no pet, a NICK_NONE nickname, a
//               closed store, an empty effect list) against the
//               canonical GCAddSlayer (pet, NICK_CUSTOM string, open
//               store with a sign, three effects); GCAddVampire carries
//               a NICK_BUILT_IN index and GCAddOusters a
//               NICK_CUSTOM_FORCED string; GCAddMonster.noname and
//               GCAddMonsterCorpse.noname take the empty-name branch of
//               their write(); GCAddVampirePortal.noowner takes the
//               zero-length owner branch.
//
//               Fixture values are distinct per field and >= 128 in
//               every byte the width allows. Three groups cannot follow
//               that rule and say so at the point of use: enum-valued
//               bytes, whose domain is a handful of small values; the
//               Slayer outlook bitset, whose fields are 2 to 5 bits
//               wide and hold enumerators; and the names, which each
//               packet caps well below 128 characters.
//
//               The write/read disagreements this set found are fixed,
//               and the section at the end pins what each one produces
//               now: PCSlayerInfo3 lets its name refusals out,
//               GCAddEffect::read() consumes exactly what write()
//               emits, the Vampire and Ousters record maxima count the
//               alignment field they carry, a coat type that does not
//               fit the wire byte is refused, the monster names, the
//               shop sign, the pet nickname, the portal owner and the
//               NPC name are bounded against the width their max size
//               budgets, GCNPCInfo caps its record list at the 255 its
//               count byte carries, the corpse packets initialise their
//               treasure count, the creature-add packets survive a
//               missing effect record, and none of the three frees the
//               pet and nickname records the creature owns.
//
//////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "EffectInfo.h"
#include "Exception.h"
#include "GCAddBat.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCAddVampirePortal.h"
#include "GCAddWolf.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCFastMove.h"
#include "GCNPCInfo.h"
#include "NPCInfo.h"
#include "NicknameInfo.h"
#include "PCOustersInfo3.h"
#include "PCSlayerInfo3.h"
#include "PCVampireInfo3.h"
#include "PetInfo.h"
#include "StoreInfo.h"
#include "TestStreams.h"
#include "WireString.h"

using wiretest::expectGolden;
using wiretest::kEncryptCodeCount;
using wiretest::kEncryptCodes;
using wiretest::Loopback;
using wiretest::roundTrip;
using wiretest::writeBody;

namespace {

// The unencrypted branch. Every packet in this file reads and writes its
// body with plain read()/write() calls, so this is the only code whose
// bytes differ from any other.
const uchar kPlainCode = 0;

//////////////////////////////////////////////////////////////////////
// The three pins every packet gets. fill() / expectEqual() are
// overloaded per packet, as in packet_login_test.cpp, so the same
// canonical instance feeds all three.
//////////////////////////////////////////////////////////////////////

#define ZONE_SCAN_PACKET_TESTS(Name)                                                                 \
    TEST(Name##Test, roundTripsThroughLoopback) {                                                    \
        Name src;                                                                                    \
        fill(src);                                                                                   \
        Name dst;                                                                                    \
        roundTrip(src, dst, kPlainCode);                                                             \
        expectEqual(src, dst);                                                                       \
    }                                                                                                \
    TEST(Name##Test, bodyBytesMatchGolden) {                                                         \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        const std::vector<unsigned char> body = writeBody(packet, kPlainCode);                       \
        expectGolden(#Name, kPlainCode, body);                                                       \
        for (size_t i = 1; i < kEncryptCodeCount; i++)                                               \
            EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))                                     \
                << #Name " now varies with the encrypt code — add per-code goldens";                 \
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

//////////////////////////////////////////////////////////////////////
// Shared optional records.
//////////////////////////////////////////////////////////////////////

// The creature-add packets take ownership of the EffectInfo they are
// handed, so every fixture allocates a fresh one.
EffectInfo* makeEffectInfo(int count) {
    EffectInfo* pInfo = new EffectInfo();
    for (int i = 0; i < count; i++)
        pInfo->addListElement((EffectID_t)(0x81A2 + i * 0x0101), (WORD)(0x93B4 + i * 0x0101));
    return pInfo;
}

// EffectInfo exposes its list only through the destructive
// popFrontListElement(), so comparing two of them empties both. Nothing
// reads them afterwards.
void expectEffectInfoEqual(EffectInfo* a, EffectInfo* b) {
    ASSERT_TRUE(a != NULL);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ(a->getListNum(), b->getListNum());
    EXPECT_EQ(a->getSize(), b->getSize());
    const int words = (int)a->getListNum() * 2;
    for (int i = 0; i < words; i++)
        EXPECT_EQ(a->popFrontListElement(), b->popFrontListElement()) << "effect word " << i;
}

void fillPetInfo(PetInfo& info) {
    // Pet type is an enum byte with a handful of enumerators, so it
    // carries a valid enumerator rather than a high byte.
    info.setPetType(PET_PIXIE);
    info.setPetCreatureType(0x8394);
    info.setPetLevel(0x85);
    info.setPetExp(0x8697A8B9);
    info.setPetHP(0x8CAD);
    info.setPetAttr(0x8E);
    info.setPetAttrLevel(0x8F);
    info.setPetOption(0x90);
    info.setFoodType(0x91A2);
    info.setGamble(0x93);
    info.setCutHead(0x94);
    info.setAttack(0x95);
    info.setNickname("GoldScanPet");
}

// write() clears the summon flag on every pet it sends, so the value the
// fixture sets is not part of the comparison.
void expectPetInfoEqual(PetInfo* a, PetInfo* b) {
    if (a == NULL || b == NULL) {
        EXPECT_TRUE(a == NULL) << "one side carries a pet and the other does not";
        EXPECT_TRUE(b == NULL) << "one side carries a pet and the other does not";
        return;
    }
    EXPECT_EQ(a->getPetType(), b->getPetType());
    EXPECT_EQ(a->getPetCreatureType(), b->getPetCreatureType());
    EXPECT_EQ(a->getPetLevel(), b->getPetLevel());
    EXPECT_EQ(a->getPetExp(), b->getPetExp());
    EXPECT_EQ(a->getPetHP(), b->getPetHP());
    EXPECT_EQ(a->getPetAttr(), b->getPetAttr());
    EXPECT_EQ(a->getPetAttrLevel(), b->getPetAttrLevel());
    EXPECT_EQ(a->getPetOption(), b->getPetOption());
    EXPECT_EQ(a->getFoodType(), b->getFoodType());
    EXPECT_EQ(a->canGamble(), b->canGamble());
    EXPECT_EQ(a->canCutHead(), b->canCutHead());
    EXPECT_EQ(a->canAttack(), b->canAttack());
    EXPECT_EQ(a->getNickname(), b->getNickname());
}

void expectNicknameInfoEqual(NicknameInfo* a, NicknameInfo* b) {
    ASSERT_TRUE(a != NULL);
    ASSERT_TRUE(b != NULL);
    EXPECT_EQ(a->getNicknameID(), b->getNicknameID());
    ASSERT_EQ(a->getNicknameType(), b->getNicknameType());
    switch (a->getNicknameType()) {
    case NicknameInfo::NICK_NONE:
        break;
    case NicknameInfo::NICK_BUILT_IN:
    case NicknameInfo::NICK_QUEST:
    case NicknameInfo::NICK_FORCED:
        EXPECT_EQ(a->getNicknameIndex(), b->getNicknameIndex());
        break;
    default:
        EXPECT_EQ(a->getNickname(), b->getNickname());
        break;
    }
}

void expectStoreOutlookEqual(const StoreOutlook& a, const StoreOutlook& b) {
    EXPECT_EQ(a.isOpen(), b.isOpen());
    if (a.isOpen() != 0)
        EXPECT_EQ(a.getSign(), b.getSign());
}

//////////////////////////////////////////////////////////////////////
// The three PC records the creature-add and corpse packets share.
//////////////////////////////////////////////////////////////////////

// The outlook fields are 2 to 5 bit wide slices of one DWORD bitset and
// hold enumerators, so each carries its highest valid enumerator rather
// than a high byte. The nine colours are WORDs and do carry high bytes.
void fillSlayerInfo(PCSlayerInfo3& info) {
    info.setObjectID(0x8A9BACBD);
    info.setName("GoldScanSlayerName");
    info.setX(0x8C);
    info.setY(0x9D);
    info.setDir(0xAE);
    info.setSex(MALE);
    info.setHairStyle(HAIR_STYLE3);
    info.setHelmetType(HELMET3);
    info.setJacketType(JACKET4);
    info.setPantsType(PANTS4);
    info.setWeaponType(WEAPON_MACE);
    info.setShieldType(SHIELD2);
    info.setMotorcycleType(MOTORCYCLE3);
    info.setShoulderType(3);
    info.setHairColor(0x81C2);
    info.setSkinColor(0x83C4);
    info.setHelmetColor(0x85C6);
    info.setJacketColor(0x87C8, MAIN_COLOR);
    info.setJacketColor(0x89CA, SUB_COLOR);
    info.setPantsColor(0x8BCC, MAIN_COLOR);
    info.setPantsColor(0x8DCE, SUB_COLOR);
    info.setWeaponColor(0x8FD0);
    info.setShieldColor(0x91D2);
    info.setMotorcycleColor(0x93D4);
    info.setShoulderColor(0x95D6);
    info.setMasterEffectColor(0x97);
    info.setCurrentHP(0x98D9);
    info.setMaxHP(0x9ADB);
    info.setAttackSpeed(0x9C);
    info.setAlignment((Alignment_t)0x9DDEEFA0);
    info.setCompetence(0xA1);
    info.setGuildID(0xA2B3);
    info.setUnionID(0xA4B5C6D7);
    info.setRank(0xA8);
    info.setAdvancementLevel(0xA9);
}

void expectSlayerInfoEqual(const PCSlayerInfo3& a, const PCSlayerInfo3& b) {
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

// Sex is an enum byte with two enumerators and the coat type travels in
// a single byte, so neither carries a high byte.
void fillVampireInfo(PCVampireInfo3& info) {
    info.setObjectID(0x8B9CADBE);
    info.setName("GoldScanVamp");
    info.setX(0x8D);
    info.setY(0x9E);
    info.setDir(0xAF);
    info.setSex(MALE);
    info.setCoatType(VAMPIRE_COAT4);
    info.setBatColor(0x82C3);
    info.setSkinColor(0x84C5);
    info.setCoatColor(0x86C7, MAIN_COLOR);
    info.setCoatColor(0x88C9, SUB_COLOR);
    info.setMasterEffectColor(0x8A);
    info.setCurrentHP(0x8BDC);
    info.setMaxHP(0x8DDE);
    info.setAttackSpeed(0x8F);
    info.setAlignment((Alignment_t)0x90A1B2C3);
    info.setShape(0x94);
    info.setCompetence(0x95);
    info.setGuildID(0x96A7);
    info.setUnionID(0x98A9BACB);
    info.setRank(0x9C);
    info.setAdvancementLevel(0x9D);
}

void expectVampireInfoEqual(const PCVampireInfo3& a, const PCVampireInfo3& b) {
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

// Coat, arm and sylph type share one packed byte and hold enumerators,
// so they carry their highest valid enumerator rather than a high byte.
void fillOustersInfo(PCOustersInfo3& info) {
    info.setObjectID(0x8C9DAEBF);
    info.setName("GoldScanOust");
    info.setX(0x8E);
    info.setY(0x9F);
    info.setDir(0xB0);
    info.setSex(MALE);
    info.setCoatType(OUSTERS_COAT4);
    info.setArmType(OUSTERS_ARM_CHAKRAM);
    info.setSylphType(OUSTERS_SYLPH1);
    info.setCoatColor(0x83C4);
    info.setHairColor(0x85C6);
    info.setArmColor(0x87C8);
    info.setBootsColor(0x89CA);
    info.setMasterEffectColor(0x8B);
    info.setCurrentHP(0x8CDD);
    info.setMaxHP(0x8EDF);
    info.setAttackSpeed(0x90);
    info.setAlignment((Alignment_t)0x91A2B3C4);
    info.setCompetence(0x95);
    info.setGuildID(0x96A7);
    info.setUnionID(0x98A9BACB);
    info.setRank(0x9C);
    info.setAdvancementLevel(0x9D);
}

void expectOustersInfoEqual(const PCOustersInfo3& a, const PCOustersInfo3& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getCoatType(), b.getCoatType());
    EXPECT_EQ(a.getArmType(), b.getArmType());
    EXPECT_EQ(a.getSylphType(), b.getSylphType());
    EXPECT_EQ(a.getCoatColor(), b.getCoatColor());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getArmColor(), b.getArmColor());
    EXPECT_EQ(a.getBootsColor(), b.getBootsColor());
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

//////////////////////////////////////////////////////////////////////
// GCAddSlayer / GCAddVampire / GCAddOusters.
//
// A filled packet owns only the effect record; the pet and nickname
// records belong to whoever installed them (a read packet owns its own).
// The fixtures hold those two as members that outlive the packet — members
// are destroyed in reverse declaration order, so `packet` goes first.
//
// A NULL nickname pointer is not goldened separately: write() then
// emits a default NicknameInfo, the same NICK_NONE shape the explicit
// record below pins. The three nickname shapes are pinned through
// explicitly constructed records.
//////////////////////////////////////////////////////////////////////

struct SlayerAddFixture {
    PetInfo pet;
    NicknameInfo nickname;
    StoreInfo store;
    GCAddSlayer packet;
};

void fillSlayerAdd(SlayerAddFixture& f) {
    PCSlayerInfo3 info;
    fillSlayerInfo(info);
    f.packet.setSlayerInfo(info);
    f.packet.setEffectInfo(makeEffectInfo(3));

    fillPetInfo(f.pet);
    f.packet.setPetInfo(&f.pet);

    f.nickname.setNicknameID(0xAABB);
    f.nickname.setNicknameType(NicknameInfo::NICK_CUSTOM);
    f.nickname.setNickname("GoldScanNick");
    f.packet.setNicknameInfo(&f.nickname);

    f.store.setOpen(0xCC);
    f.store.setSign("GoldScanSlayerStoreSign");
    f.packet.setStoreInfo(&f.store);
}

// The branch every ordinary character takes: no pet, no nickname, a
// closed store and no effects.
void fillSlayerAddBare(SlayerAddFixture& f) {
    PCSlayerInfo3 info;
    fillSlayerInfo(info);
    f.packet.setSlayerInfo(info);
    f.packet.setEffectInfo(makeEffectInfo(0));

    f.nickname.setNicknameID(0xB1C2);
    f.nickname.setNicknameType(NicknameInfo::NICK_NONE);
    f.packet.setNicknameInfo(&f.nickname);

    f.store.setOpen(0);
    f.packet.setStoreInfo(&f.store);
}

void expectSlayerAddEqual(GCAddSlayer& a, GCAddSlayer& b) {
    expectSlayerInfoEqual(a.getSlayerInfo(), b.getSlayerInfo());
    expectEffectInfoEqual(a.getEffectInfo(), b.getEffectInfo());
    expectPetInfoEqual(a.getPetInfo(), b.getPetInfo());
    expectNicknameInfoEqual(a.getNicknameInfo(), b.getNicknameInfo());
    expectStoreOutlookEqual(a.getStoreOutlook(), b.getStoreOutlook());
}

void expectSlayerAddPins(GCAddSlayer& packet, const char* goldenName) {
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden(goldenName, kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))
            << goldenName << " now varies with the encrypt code — add per-code goldens";

    GCAddSlayerFactory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size())
        << goldenName
        << ": getPacketSize() disagrees with the bytes write() emits; writePacket() puts the former "
           "on the wire, so the stream never resynchronises";
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize())
        << goldenName << ": the body outgrows the read buffer the receiver sizes from the factory max";
    EXPECT_EQ(factory.getPacketID(), packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), packet.getPacketName());
}

TEST(GCAddSlayerTest, roundTripsThroughLoopback) {
    SlayerAddFixture f;
    fillSlayerAdd(f);
    GCAddSlayer dst;
    roundTrip(f.packet, dst, kPlainCode);
    expectSlayerAddEqual(f.packet, dst);
}

// The slayer outlook's three shield bits past the named shields are
// refused on read; SHIELD_MAX, the sentinel, stands in for 3..7.
TEST(GCAddSlayerTest, refusesShieldBitsPastTheEnumerators) {
    SlayerAddFixture f;
    fillSlayerAdd(f);
    PCSlayerInfo3 info = f.packet.getSlayerInfo();
    info.setShieldType(SHIELD_MAX);
    f.packet.setSlayerInfo(info);
    GCAddSlayer dst;
    EXPECT_THROW(roundTrip(f.packet, dst, kPlainCode), InvalidProtocolException);
}

TEST(GCAddSlayerTest, bodyBytesMatchGoldenAndSizeMatchesTheBytesWritten) {
    SlayerAddFixture f;
    fillSlayerAdd(f);
    expectSlayerAddPins(f.packet, "GCAddSlayer");
}

TEST(GCAddSlayerTest, bareRoundTripsThroughLoopback) {
    SlayerAddFixture f;
    fillSlayerAddBare(f);
    GCAddSlayer dst;
    roundTrip(f.packet, dst, kPlainCode);
    EXPECT_TRUE(f.packet.getPetInfo() == NULL);
    EXPECT_TRUE(dst.getPetInfo() == NULL) << "a PET_NONE record must read back as no pet";
    expectSlayerAddEqual(f.packet, dst);
}

TEST(GCAddSlayerTest, bareBodyBytesMatchGoldenAndSizeMatchesTheBytesWritten) {
    SlayerAddFixture f;
    fillSlayerAddBare(f);
    expectSlayerAddPins(f.packet, "GCAddSlayer.bare");
}

struct VampireAddFixture {
    PetInfo pet;
    NicknameInfo nickname;
    StoreInfo store;
    GCAddVampire packet;
};

void fillVampireAdd(VampireAddFixture& f) {
    PCVampireInfo3 info;
    fillVampireInfo(info);
    f.packet.setVampireInfo(info);
    f.packet.setEffectInfo(makeEffectInfo(2));

    fillPetInfo(f.pet);
    f.packet.setPetInfo(&f.pet);

    // The indexed nickname shape.
    f.nickname.setNicknameID(0xB2C3);
    f.nickname.setNicknameType(NicknameInfo::NICK_BUILT_IN);
    f.nickname.setNicknameIndex(0xB4D5);
    f.packet.setNicknameInfo(&f.nickname);

    f.store.setOpen(0xD6);
    f.store.setSign("GoldScanVampStoreSign");
    f.packet.setStoreInfo(&f.store);

    // What makeGCAddVampire sets when the character stepped out of a
    // personal portal.
    f.packet.setFromFlag(1);
}

TEST(GCAddVampireTest, roundTripsThroughLoopback) {
    VampireAddFixture f;
    fillVampireAdd(f);
    GCAddVampire dst;
    roundTrip(f.packet, dst, kPlainCode);
    expectVampireInfoEqual(f.packet.getVampireInfo(), dst.getVampireInfo());
    expectEffectInfoEqual(f.packet.getEffectInfo(), dst.getEffectInfo());
    expectPetInfoEqual(f.packet.getPetInfo(), dst.getPetInfo());
    expectNicknameInfoEqual(f.packet.getNicknameInfo(), dst.getNicknameInfo());
    expectStoreOutlookEqual(f.packet.getStoreOutlook(), dst.getStoreOutlook());
    EXPECT_EQ(f.packet.getFromFlag(), dst.getFromFlag());
}

TEST(GCAddVampireTest, bodyBytesMatchGoldenAndSizeMatchesTheBytesWritten) {
    VampireAddFixture f;
    fillVampireAdd(f);

    const std::vector<unsigned char> body = writeBody(f.packet, kPlainCode);
    expectGolden("GCAddVampire", kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(f.packet, kEncryptCodes[i]))
            << "GCAddVampire now varies with the encrypt code — add per-code goldens";

    GCAddVampireFactory factory;
    EXPECT_EQ((size_t)f.packet.getPacketSize(), body.size())
        << "GCAddVampire: getPacketSize() disagrees with the bytes write() emits; writePacket() puts "
           "the former on the wire, so the stream never resynchronises";
    EXPECT_LE(f.packet.getPacketSize(), factory.getPacketMaxSize())
        << "GCAddVampire: the body outgrows the read buffer the receiver sizes from the factory max";
    EXPECT_EQ(factory.getPacketID(), f.packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), f.packet.getPacketName());
}

// PCVampireInfo3::write() lets its name refusal out, unlike
// PCSlayerInfo3::write() below. The empty-name half of the same refusal
// is not exercised here: setName() asserts on it before write() ever
// sees the record.
TEST(GCAddVampireTest, refusesOversizedNames) {
    SocketEncryptOutputStream oStream(NULL);

    VampireAddFixture tooLong;
    fillVampireAdd(tooLong);
    tooLong.packet.getVampireInfo().setName(std::string(21, 'v'));
    EXPECT_THROW(tooLong.packet.write(oStream), InvalidProtocolException);
}

struct OustersAddFixture {
    NicknameInfo nickname;
    StoreInfo store;
    GCAddOusters packet;
};

void fillOustersAdd(OustersAddFixture& f) {
    PCOustersInfo3 info;
    fillOustersInfo(info);
    f.packet.setOustersInfo(info);
    f.packet.setEffectInfo(makeEffectInfo(1));

    // The forced-string nickname shape.
    f.nickname.setNicknameID(0xB5C6);
    f.nickname.setNicknameType(NicknameInfo::NICK_CUSTOM_FORCED);
    f.nickname.setNickname("GoldScanForced");
    f.packet.setNicknameInfo(&f.nickname);

    f.store.setOpen(0xD7);
    f.store.setSign("GoldScanOustStoreSign");
    f.packet.setStoreInfo(&f.store);
}

TEST(GCAddOustersTest, roundTripsThroughLoopback) {
    OustersAddFixture f;
    fillOustersAdd(f);
    GCAddOusters dst;
    roundTrip(f.packet, dst, kPlainCode);
    expectOustersInfoEqual(f.packet.getOustersInfo(), dst.getOustersInfo());
    expectEffectInfoEqual(f.packet.getEffectInfo(), dst.getEffectInfo());
    expectPetInfoEqual(f.packet.getPetInfo(), dst.getPetInfo());
    expectNicknameInfoEqual(f.packet.getNicknameInfo(), dst.getNicknameInfo());
    expectStoreOutlookEqual(f.packet.getStoreOutlook(), dst.getStoreOutlook());
}

TEST(GCAddOustersTest, bodyBytesMatchGoldenAndSizeMatchesTheBytesWritten) {
    OustersAddFixture f;
    fillOustersAdd(f);
    GCAddOusters& packet = f.packet;

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCAddOusters", kPlainCode, body);
    for (size_t i = 1; i < kEncryptCodeCount; i++)
        EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))
            << "GCAddOusters now varies with the encrypt code — add per-code goldens";

    GCAddOustersFactory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size())
        << "GCAddOusters: getPacketSize() disagrees with the bytes write() emits; writePacket() puts "
           "the former on the wire, so the stream never resynchronises";
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize())
        << "GCAddOusters: the body outgrows the read buffer the receiver sizes from the factory max";
    EXPECT_EQ(factory.getPacketID(), packet.getPacketID());
    EXPECT_EQ(factory.getPacketName(), packet.getPacketName());
}

//////////////////////////////////////////////////////////////////////
// The corpse packets.
//////////////////////////////////////////////////////////////////////

void fill(GCAddSlayerCorpse& p) {
    fillSlayerInfo(p.getSlayerInfo());
    p.setTreasureCount(0x8B);
}
void expectEqual(const GCAddSlayerCorpse& a, const GCAddSlayerCorpse& b) {
    expectSlayerInfoEqual(a.getSlayerInfo(), b.getSlayerInfo());
    EXPECT_EQ(a.getTreasureCount(), b.getTreasureCount());
}
ZONE_SCAN_PACKET_TESTS(GCAddSlayerCorpse)

void fill(GCAddVampireCorpse& p) {
    fillVampireInfo(p.getVampireInfo());
    p.setTreasureCount(0x9C);
}
void expectEqual(const GCAddVampireCorpse& a, const GCAddVampireCorpse& b) {
    expectVampireInfoEqual(a.getVampireInfo(), b.getVampireInfo());
    EXPECT_EQ(a.getTreasureCount(), b.getTreasureCount());
}
ZONE_SCAN_PACKET_TESTS(GCAddVampireCorpse)

void fill(GCAddOustersCorpse& p) {
    fillOustersInfo(p.getOustersInfo());
    p.setTreasureCount(0xAD);
}
void expectEqual(const GCAddOustersCorpse& a, const GCAddOustersCorpse& b) {
    expectOustersInfoEqual(a.getOustersInfo(), b.getOustersInfo());
    EXPECT_EQ(a.getTreasureCount(), b.getTreasureCount());
}
ZONE_SCAN_PACKET_TESTS(GCAddOustersCorpse)

void fill(GCAddMonsterCorpse& p) {
    p.setObjectID(0x8D9EAFB0);
    p.setMonsterType(0x81C2);
    p.setMonsterName("GoldScanMonsterCorpse");
    p.setX(0x83);
    p.setY(0x94);
    p.setDir(0xA5);
    p.sethasHead(true);
    p.setTreasureCount(0xB6);
    p.setLastKiller(0x87C8D9EA);
}
void expectEqual(const GCAddMonsterCorpse& a, const GCAddMonsterCorpse& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getMonsterType(), b.getMonsterType());
    EXPECT_EQ(a.getMonsterName(), b.getMonsterName());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.gethasHead(), b.gethasHead());
    EXPECT_EQ(a.getTreasureCount(), b.getTreasureCount());
    EXPECT_EQ(a.getLastKiller(), b.getLastKiller());
}
ZONE_SCAN_PACKET_TESTS(GCAddMonsterCorpse)

// The other branch of write(): a corpse with no name emits a zero
// length byte and nothing else for the name.
TEST(GCAddMonsterCorpseTest, namelessBodyBytesMatchGoldenAndRoundTrips) {
    GCAddMonsterCorpse packet;
    fill(packet);
    packet.setMonsterName("");

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCAddMonsterCorpse.noname", kPlainCode, body);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    GCAddMonsterCorpse dst;
    roundTrip(packet, dst, kPlainCode);
    expectEqual(packet, dst);
}

//////////////////////////////////////////////////////////////////////
// The creature packets scan() sends for a monster, a hidden creature
// and an NPC.
//////////////////////////////////////////////////////////////////////

void fill(GCAddMonster& p) {
    p.setObjectID(0x8E9FB0C1);
    p.setMonsterType(0x82C3);
    p.setMonsterName("GoldScanMonsterName");
    p.setMainColor(0x84C5);
    p.setSubColor(0x86C7);
    p.setX(0x88);
    p.setY(0x99);
    p.setDir(0xAA);
    p.setEffectInfo(makeEffectInfo(2));
    p.setCurrentHP(0x8BCC);
    p.setMaxHP(0x8DCE);
    p.setFromFlag(0x8F);
}
void expectEqual(GCAddMonster& a, GCAddMonster& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getMonsterType(), b.getMonsterType());
    EXPECT_EQ(a.getMonsterName(), b.getMonsterName());
    EXPECT_EQ(a.getMainColor(), b.getMainColor());
    EXPECT_EQ(a.getSubColor(), b.getSubColor());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getCurrentHP(), b.getCurrentHP());
    EXPECT_EQ(a.getMaxHP(), b.getMaxHP());
    EXPECT_EQ(a.getFromFlag(), b.getFromFlag());
    expectEffectInfoEqual(a.getEffectInfo(), b.getEffectInfo());
}
ZONE_SCAN_PACKET_TESTS(GCAddMonster)

// The other branch of write(): an unnamed monster emits a zero length
// byte and nothing else for the name.
TEST(GCAddMonsterTest, namelessBodyBytesMatchGoldenAndRoundTrips) {
    GCAddMonster packet;
    fill(packet);
    packet.setMonsterName("");

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCAddMonster.noname", kPlainCode, body);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    GCAddMonster dst;
    roundTrip(packet, dst, kPlainCode);
    expectEqual(packet, dst);
}

void fill(GCAddBurrowingCreature& p) {
    p.setObjectID(0x8FA0B1C2);
    p.setName("GoldScanBurrow");
    p.setX(0x83);
    p.setY(0x94);
}
void expectEqual(const GCAddBurrowingCreature& a, const GCAddBurrowingCreature& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
}
ZONE_SCAN_PACKET_TESTS(GCAddBurrowingCreature)

void fill(GCAddBat& p) {
    p.setObjectID(0x90A1B2C3);
    p.setName("GoldScanBat");
    p.setItemType(0x84C5);
    p.setXYDir(0x86, 0x97, 0xA8);
    p.setCurrentHP(0x89CA);
    p.setMaxHP(0x8BCC);
    p.setGuildID(0x8DCE);
    p.setColor(0x8FD0);
}
void expectEqual(const GCAddBat& a, const GCAddBat& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getItemType(), b.getItemType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getCurrentHP(), b.getCurrentHP());
    EXPECT_EQ(a.getMaxHP(), b.getMaxHP());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getColor(), b.getColor());
}
ZONE_SCAN_PACKET_TESTS(GCAddBat)

void fill(GCAddWolf& p) {
    p.setObjectID(0x91A2B3C4);
    p.setName("GoldScanWolf");
    p.setMainColor(0x85C6);
    p.setItemType(0x87C8);
    p.setXYDir(0x89, 0x9A, 0xAB);
    p.setCurrentHP(0x8CCD);
    p.setMaxHP(0x8ECF);
    p.setGuildID(0x90D1);
}
void expectEqual(const GCAddWolf& a, const GCAddWolf& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getMainColor(), b.getMainColor());
    EXPECT_EQ(a.getItemType(), b.getItemType());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
    EXPECT_EQ(a.getCurrentHP(), b.getCurrentHP());
    EXPECT_EQ(a.getMaxHP(), b.getMaxHP());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
}
ZONE_SCAN_PACKET_TESTS(GCAddWolf)

void fill(GCAddNPC& p) {
    p.setObjectID(0x92A3B4C5);
    p.setName("GoldScanNPCName");
    p.setNPCID(0x86C7);
    p.setSpriteType(0x88C9);
    p.setMainColor(0x8ACB);
    p.setSubColor(0x8CCD);
    p.setX(0x8E);
    p.setY(0x9F);
    p.setDir(0xB0);
}
void expectEqual(const GCAddNPC& a, const GCAddNPC& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getNPCID(), b.getNPCID());
    EXPECT_EQ(a.getSpriteType(), b.getSpriteType());
    EXPECT_EQ(a.getMainColor(), b.getMainColor());
    EXPECT_EQ(a.getSubColor(), b.getSubColor());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getDir(), b.getDir());
}
ZONE_SCAN_PACKET_TESTS(GCAddNPC)

// The three name-carrying creature packets whose write() lets its own
// refusals out. A caller that hands one an empty or over-length name
// gets an exception, not a short body.
TEST(ZoneScanNamesTest, creaturePacketsRefuseEmptyOrOversizedNames) {
    SocketEncryptOutputStream oStream(NULL);

    GCAddNPC npc;
    fill(npc);
    npc.setName("");
    EXPECT_THROW(npc.write(oStream), InvalidProtocolException);
    npc.setName(std::string(41, 'n'));
    EXPECT_THROW(npc.write(oStream), InvalidProtocolException);

    GCAddBurrowingCreature burrow;
    fill(burrow);
    burrow.setName("");
    EXPECT_THROW(burrow.write(oStream), InvalidProtocolException);
    burrow.setName(std::string(21, 'b'));
    EXPECT_THROW(burrow.write(oStream), InvalidProtocolException);

    GCAddBat bat;
    fill(bat);
    bat.setName("");
    EXPECT_THROW(bat.write(oStream), InvalidProtocolException);
    bat.setName(std::string(21, 'x'));
    EXPECT_THROW(bat.write(oStream), InvalidProtocolException);

    GCAddWolf wolf;
    fill(wolf);
    wolf.setName("");
    EXPECT_THROW(wolf.write(oStream), InvalidProtocolException);
    wolf.setName(std::string(21, 'w'));
    EXPECT_THROW(wolf.write(oStream), InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// The effect and portal packets scan() sends for a tile, and the
// relic markers sendRelicEffect() puts on a monster corpse.
//////////////////////////////////////////////////////////////////////

void fill(GCAddEffect& p) {
    p.setObjectID(0x93A4B5C6);
    p.setEffectID(0x87C8);
    p.setDuration(0x89CA);
}
void expectEqual(const GCAddEffect& a, const GCAddEffect& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getEffectID(), b.getEffectID());
    EXPECT_EQ(a.getDuration(), b.getDuration());
}
ZONE_SCAN_PACKET_TESTS(GCAddEffect)

void fill(GCAddEffectToTile& p) {
    p.setObjectID(0x94A5B6C7);
    p.setXY(0x88, 0x99);
    p.setEffectID(0x8ACB);
    p.setDuration(0x8CCD);
}
void expectEqual(const GCAddEffectToTile& a, const GCAddEffectToTile& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getEffectID(), b.getEffectID());
    EXPECT_EQ(a.getDuration(), b.getDuration());
}
ZONE_SCAN_PACKET_TESTS(GCAddEffectToTile)

void fill(GCAddVampirePortal& p) {
    p.setObjectID(0x95A6B7C8);
    p.setOwnerID("GoldScanPortalOwner");
    p.setDuration(0x89CA);
    p.setX(0x8B);
    p.setY(0x9C);
    p.setTargetZoneID(0x8DCE);
    p.setTargetX(0x8F);
    p.setTargetY(0xA0);
    p.setCreateFlag(0xB1);
}
void expectEqual(const GCAddVampirePortal& a, const GCAddVampirePortal& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getOwnerID(), b.getOwnerID());
    EXPECT_EQ(a.getDuration(), b.getDuration());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getTargetZoneID(), b.getTargetZoneID());
    EXPECT_EQ(a.getTargetX(), b.getTargetX());
    EXPECT_EQ(a.getTargetY(), b.getTargetY());
    EXPECT_EQ(a.getCreateFlag(), b.getCreateFlag());
}
ZONE_SCAN_PACKET_TESTS(GCAddVampirePortal)

// The other branch of write(): an ownerless portal emits a zero length
// byte and nothing else for the owner.
TEST(GCAddVampirePortalTest, ownerlessBodyBytesMatchGoldenAndRoundTrips) {
    GCAddVampirePortal packet;
    fill(packet);
    packet.setOwnerID("");

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("GCAddVampirePortal.noowner", kPlainCode, body);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    GCAddVampirePortal dst;
    roundTrip(packet, dst, kPlainCode);
    expectEqual(packet, dst);
}

//////////////////////////////////////////////////////////////////////
// The removal half, and the packet that reframes a view in place.
//////////////////////////////////////////////////////////////////////

void fill(GCDeleteObject& p) {
    p.setObjectID(0x96A7B8C9);
}
void expectEqual(const GCDeleteObject& a, const GCDeleteObject& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}
ZONE_SCAN_PACKET_TESTS(GCDeleteObject)

void fill(GCDeleteEffectFromTile& p) {
    p.setObjectID(0x97A8B9CA);
    p.setXY(0x8B, 0x9C);
    p.setEffectID(0x8DCE);
}
void expectEqual(const GCDeleteEffectFromTile& a, const GCDeleteEffectFromTile& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getX(), b.getX());
    EXPECT_EQ(a.getY(), b.getY());
    EXPECT_EQ(a.getEffectID(), b.getEffectID());
}
ZONE_SCAN_PACKET_TESTS(GCDeleteEffectFromTile)

void fill(GCFastMove& p) {
    p.setObjectID(0x98A9BACB);
    p.setXY(0x8C, 0x9D, 0xAE, 0xBF);
    p.setSkillType(0x90D1);
}
void expectEqual(const GCFastMove& a, const GCFastMove& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getFromX(), b.getFromX());
    EXPECT_EQ(a.getFromY(), b.getFromY());
    EXPECT_EQ(a.getToX(), b.getToX());
    EXPECT_EQ(a.getToY(), b.getToY());
    EXPECT_EQ(a.getSkillType(), b.getSkillType());
}
ZONE_SCAN_PACKET_TESTS(GCFastMove)

//////////////////////////////////////////////////////////////////////
// The refusals and bounds these packets enforce, and the records they
// leave alone. Each pins a write/read disagreement this set found.
//////////////////////////////////////////////////////////////////////

// PCSlayerInfo3::write() lets its refusals out, the way PCVampireInfo3
// and PCOustersInfo3 do. A swallowed refusal stopped the PC record
// after the four bytes of object id already on the wire, while
// GCAddSlayer::getPacketSize() — which writePacket() sends ahead of the
// body — still counted the whole record.
TEST(GCAddSlayerTest, refusesOversizedSlayerNames) {
    SocketEncryptOutputStream oStream(NULL);

    SlayerAddFixture f;
    fillSlayerAdd(f);
    f.packet.getSlayerInfo().setName(std::string(21, 's'));
    EXPECT_THROW(f.packet.write(oStream), InvalidProtocolException);
}

// GCAddEffect::read() consumes exactly what write() emits: the leading
// flag byte it used to take is gone, so the packet round trips through
// ZONE_SCAN_PACKET_TESTS above. The body is the three fields and
// nothing else.
TEST(GCAddEffectTest, theBodyIsTheThreeFieldsAndNothingElse) {
    GCAddEffect packet;
    fill(packet);

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    EXPECT_EQ((size_t)(szObjectID + szEffectID + szDuration), body.size());
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());
}

// PCVampireInfo3::getMaxSize() counts the alignment field its getSize()
// puts on the wire. Alignment_t is an int, so every carrier of the
// record used to budget four bytes too few, and a name of 17 characters
// or more declared a body larger than the read buffer the receiver
// sizes from the factory max.
TEST(GCAddVampireCorpseTest, theLongestNameStillFitsTheFactoryMax) {
    GCAddVampireCorpse packet;
    fill(packet);
    packet.getVampireInfo().setName(std::string(20, 'v'));

    GCAddVampireCorpseFactory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size());
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
}

// The same field in PCOustersInfo3::getMaxSize().
TEST(GCAddOustersCorpseTest, theLongestNameStillFitsTheFactoryMax) {
    GCAddOustersCorpse packet;
    fill(packet);
    packet.getOustersInfo().setName(std::string(20, 'o'));

    GCAddOustersCorpseFactory factory;
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size());
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
}

// PCVampireInfo3 holds the coat type in an ItemType_t, which is a WORD,
// while the wire and getSize() carry one byte. Every vampire coat item
// type the game data defines fits that byte; one that does not is
// refused instead of losing its high byte and dressing the character in
// a different coat.
TEST(GCAddVampireCorpseTest, refusesACoatTypeThatDoesNotFitTheWireByte) {
    SocketEncryptOutputStream oStream(NULL);

    GCAddVampireCorpse tooWide;
    fill(tooWide);
    tooWide.getVampireInfo().setCoatType(0x8194);
    EXPECT_THROW(tooWide.write(oStream), InvalidProtocolException);

    GCAddVampireCorpse src;
    fill(src);
    src.getVampireInfo().setCoatType(0xFF);
    GCAddVampireCorpse dst;
    roundTrip(src, dst, kPlainCode);
    EXPECT_EQ(0xFF, (int)dst.getVampireInfo().getCoatType());
}

// GCAddMonster and GCAddMonsterCorpse bound the monster name on both
// sides against the 32 and 128 characters their factory maxima budget.
// Unbounded, the length byte wrapped past 255 and the body outgrew the
// read buffer the receiver sizes from that max.
TEST(ZoneScanBoundsTest, monsterNamesAreBoundedAgainstTheFactoryMax) {
    SocketEncryptOutputStream oStream(NULL);

    GCAddMonster monster;
    fill(monster);
    monster.setMonsterName(std::string(GCAddMonster::kMaxNameSize + 1, 'm'));
    EXPECT_THROW(monster.write(oStream), InvalidProtocolException);

    monster.setMonsterName(std::string(GCAddMonster::kMaxNameSize, 'm'));
    GCAddMonsterFactory monsterFactory;
    EXPECT_LE(monster.getPacketSize(), monsterFactory.getPacketMaxSize());

    GCAddMonsterCorpse corpse;
    fill(corpse);
    corpse.setMonsterName(std::string(GCAddMonsterCorpse::kMaxNameSize + 1, 'c'));
    EXPECT_THROW(corpse.write(oStream), InvalidProtocolException);

    corpse.setMonsterName(std::string(GCAddMonsterCorpse::kMaxNameSize, 'c'));
    GCAddMonsterCorpseFactory corpseFactory;
    EXPECT_LE(corpse.getPacketSize(), corpseFactory.getPacketMaxSize());
}

// The two optional strings a creature-add packet carries for a value a
// player typed are cut to the cap their record's max size budgets, the
// way the nickname setter is, so a shop or a pet named past the cap
// cannot overrun the read buffer of every client that sees the owner.
TEST(ZoneScanBoundsTest, playerTypedStringsAreCutToTheirRecordMax) {
    StoreOutlook sign;
    sign.setOpen(1);
    sign.setSign(std::string(MAX_SIGN_SIZE + 40, 'g'));
    EXPECT_EQ((size_t)MAX_SIGN_SIZE, sign.getSign().size());
    EXPECT_LE(sign.getSize(), StoreOutlook::getMaxSize());

    PetInfo pet;
    fillPetInfo(pet);
    pet.setNickname(std::string(PetInfo::kMaxNicknameSize + 40, 'p'));
    EXPECT_EQ((size_t)PetInfo::kMaxNicknameSize, pet.getNickname().size());
    EXPECT_LE(pet.getSize(), PetInfo::getMaxSize());
}

// GCAddVampirePortal bounds the owner name against the 20 its factory
// max budgets.
TEST(ZoneScanBoundsTest, thePortalOwnerIsBoundedAgainstTheFactoryMax) {
    SocketEncryptOutputStream oStream(NULL);

    GCAddVampirePortal packet;
    fill(packet);
    packet.setOwnerID(std::string(GCAddVampirePortal::kMaxOwnerIDSize + 1, 'o'));
    EXPECT_THROW(packet.write(oStream), InvalidProtocolException);

    packet.setOwnerID(std::string(GCAddVampirePortal::kMaxOwnerIDSize, 'o'));
    GCAddVampirePortalFactory factory;
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
}

// NPCInfo bounds its name against the 30 its own max size budgets, and
// GCNPCInfo refuses a record past the 255 its count byte carries — the
// shape GCUpdateInfo::addNPCInfo already has. The records belong to the
// zone either way, so a refused one is simply not listed.
TEST(ZoneScanBoundsTest, npcRecordsAreBoundedAgainstTheirMaxSize) {
    SocketEncryptOutputStream oStream(NULL);

    NPCInfo info;
    info.setName(std::string(NPCInfo::kMaxNameSize + 1, 'n'));
    EXPECT_THROW(info.write(oStream), InvalidProtocolException);

    info.setName(std::string(NPCInfo::kMaxNameSize, 'n'));
    EXPECT_LE(info.getSize(), NPCInfo::getMaxSize());

    std::vector<NPCInfo> records(GCNPCInfo::kMaxNPCInfos + 1);
    GCNPCInfo packet;
    for (size_t i = 0; i < records.size(); i++) {
        records[i].setName("GoldScanNPC");
        packet.addNPCInfo(&records[i]);
    }

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());
    EXPECT_EQ((int)GCNPCInfo::kMaxNPCInfos, (int)body[0]);

    GCNPCInfoFactory factory;
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());
}

// The corpse packets initialise the treasure count in the info-taking
// constructor as well as in the default one, so the byte they put on
// the wire is a count rather than whatever the allocation held.
TEST(ZoneScanCorpseTest, theInfoConstructorsInitialiseTheTreasureCount) {
    PCSlayerInfo3 slayerInfo;
    fillSlayerInfo(slayerInfo);
    EXPECT_EQ(0, (int)GCAddSlayerCorpse(slayerInfo).getTreasureCount());

    PCVampireInfo3 vampireInfo;
    fillVampireInfo(vampireInfo);
    EXPECT_EQ(0, (int)GCAddVampireCorpse(vampireInfo).getTreasureCount());

    PCOustersInfo3 oustersInfo;
    fillOustersInfo(oustersInfo);
    EXPECT_EQ(0, (int)GCAddOustersCorpse(oustersInfo).getTreasureCount());
}

// A packet with no effect record installed writes an empty list and
// counts the same empty list, rather than dereferencing NULL in both
// getPacketSize() and write().
TEST(ZoneScanEffectRecordTest, aMissingEffectRecordWritesAnEmptyList) {
    EffectInfo emptyList;
    EXPECT_EQ((PacketSize_t)szBYTE, emptyList.getSize());

    GCAddMonster monster;
    fill(monster);
    delete monster.getEffectInfo();
    monster.setEffectInfo(NULL);
    EXPECT_EQ((size_t)monster.getPacketSize(), writeBody(monster, kPlainCode).size());

    SlayerAddFixture slayer;
    fillSlayerAdd(slayer);
    delete slayer.packet.getEffectInfo();
    slayer.packet.setEffectInfo(NULL);
    EXPECT_EQ((size_t)slayer.packet.getPacketSize(), writeBody(slayer.packet, kPlainCode).size());

    VampireAddFixture vampire;
    fillVampireAdd(vampire);
    delete vampire.packet.getEffectInfo();
    vampire.packet.setEffectInfo(NULL);
    EXPECT_EQ((size_t)vampire.packet.getPacketSize(), writeBody(vampire.packet, kPlainCode).size());

    OustersAddFixture ousters;
    fillOustersAdd(ousters);
    delete ousters.packet.getEffectInfo();
    ousters.packet.setEffectInfo(NULL);
    EXPECT_EQ((size_t)ousters.packet.getPacketSize(), writeBody(ousters.packet, kPlainCode).size());
}

// The unburrow and untransform packets carry the same optional effect
// record. A default-constructed instance is the one every factory hands
// a reader, so sizing, writing or printing one before it has read
// anything must not dereference NULL.
template <typename AddPacket> void expectSurvivesAMissingEffectRecord(AddPacket& packet, const char* what) {
    ASSERT_TRUE(packet.getEffectInfo() == NULL) << what;
    EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size()) << what;
    EXPECT_FALSE(packet.toString().empty()) << what;

    AddPacket dst;
    roundTrip(packet, dst, kPlainCode);
    ASSERT_TRUE(dst.getEffectInfo() != NULL) << what;
    EXPECT_EQ(0, (int)dst.getEffectInfo()->getListNum()) << what;
    EXPECT_EQ(packet.getPacketSize(), dst.getPacketSize()) << what;
}

template <typename MonsterPacket> void fillTransitionMonster(MonsterPacket& p) {
    p.setObjectID(0x91A2B3C4);
    p.setMonsterType(0x85C6);
    p.setMonsterName("GoldTransitionMonster");
    p.setMainColor(0x87C8);
    p.setSubColor(0x89CA);
    p.setX(0x8B);
    p.setY(0x9C);
    p.setDir(5);
    p.setCurrentHP(0x8ECF);
    p.setMaxHP(0x90D1);
}

// Each fixture picks one of the eight directions Dir2String holds,
// because toString() indexes that array.
TEST(ZoneScanEffectRecordTest, theTransitionPacketsSurviveAMissingEffectRecord) {
    GCAddMonsterFromBurrowing monsterBurrow;
    fillTransitionMonster(monsterBurrow);
    expectSurvivesAMissingEffectRecord(monsterBurrow, "GCAddMonsterFromBurrowing");

    GCAddMonsterFromTransformation monsterMorph;
    fillTransitionMonster(monsterMorph);
    expectSurvivesAMissingEffectRecord(monsterMorph, "GCAddMonsterFromTransformation");

    GCAddVampireFromBurrowing vampireBurrow;
    fillVampireInfo(vampireBurrow.getVampireInfo());
    vampireBurrow.getVampireInfo().setDir(5);
    expectSurvivesAMissingEffectRecord(vampireBurrow, "GCAddVampireFromBurrowing");

    GCAddVampireFromTransformation vampireMorph;
    fillVampireInfo(vampireMorph.getVampireInfo());
    vampireMorph.getVampireInfo().setDir(5);
    expectSurvivesAMissingEffectRecord(vampireMorph, "GCAddVampireFromTransformation");
}

// None of the three creature-add packets frees the pet or the nickname
// record it was handed: the fill sites install the creature's own, and
// a packet that freed them would leave the creature holding a dangling
// pointer. The records here live on the stack, so a destructor that
// deleted them would not survive this test.
TEST(ZoneScanOwnershipTest, theCreatureAddPacketsLeaveInstalledRecordsAlone) {
    PetInfo pet;
    fillPetInfo(pet);

    NicknameInfo nickname;
    nickname.setNicknameType(NicknameInfo::NICK_NONE);

    {
        GCAddSlayer packet;
        packet.setEffectInfo(makeEffectInfo(0));
        packet.setPetInfo(&pet);
        packet.setNicknameInfo(&nickname);
    }
    {
        GCAddVampire packet;
        packet.setEffectInfo(makeEffectInfo(0));
        packet.setPetInfo(&pet);
        packet.setNicknameInfo(&nickname);
    }
    {
        GCAddOusters packet;
        packet.setEffectInfo(makeEffectInfo(0));
        packet.setPetInfo(&pet);
        packet.setNicknameInfo(&nickname);
    }

    EXPECT_EQ((int)PET_PIXIE, (int)pet.getPetType());
    EXPECT_EQ("GoldScanPet", pet.getNickname());
    EXPECT_EQ((int)NicknameInfo::NICK_NONE, (int)nickname.getNicknameType());
}

// The record's own read half, the one GCMyStoreInfo uses. The sign
// travels behind a length byte, so it comes back whole at the width the
// record's max size budgets and when it is absent, and a longer one is
// cut in the setter rather than sent past that budget.
TEST(StoreInfoTest, theSignTravelsBehindItsLengthByte) {
    const std::string full(MAX_SIGN_SIZE, 's');

    StoreInfo src;
    src.setOpen(1);
    src.setSign(full);

    Loopback link;
    link.setCodes(0);
    src.write(link.out(), false);
    link.pump(src.getSize(false));

    StoreInfo dst;
    dst.read(link.in(), false);
    EXPECT_EQ(full, dst.getSign());

    StoreInfo empty;
    empty.setOpen(1);

    Loopback emptyLink;
    emptyLink.setCodes(0);
    empty.write(emptyLink.out(), false);
    emptyLink.pump(empty.getSize(false));

    StoreInfo emptyBack;
    emptyBack.read(emptyLink.in(), false);
    EXPECT_TRUE(emptyBack.getSign().empty());

    StoreInfo tooLong;
    tooLong.setOpen(1);
    tooLong.setSign(std::string(MAX_SIGN_SIZE + 1, 's'));
    EXPECT_EQ((size_t)MAX_SIGN_SIZE, tooLong.getSign().size());
    EXPECT_LE(tooLong.getSize(false), StoreInfo::getMaxSize());
}

} // namespace
