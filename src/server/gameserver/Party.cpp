//////////////////////////////////////////////////////////////////////////////
// Filename    : PartyInvite.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Party.h"

#include <list>

#include <source_location>

#include "CreatureUtil.h"
#include "Effect.h"
#include "EffectCanEnterGDRLair.h"
#include "EffectManager.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCOtherGuildName.h"
#include "GCOtherModifyInfo.h"
#include "GCPartyInvite.h"
#include "GCPartyJoined.h"
#include "GCPartyLeave.h"
#include "GCStatusCurrentHP.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Slayer.h"
#include "StringStream.h"
#include "Vampire.h"
#include "Zone.h"
#include "skill/EffectActivation.h"
#include "skill/EffectDetectHidden.h"
#include "skill/EffectDetectInvisibility.h"
#include "skill/EffectExpansion.h"
#include "skill/EffectGnomesWhisper.h"
#include "skill/EffectHolyArmor.h"
#include "skill/EffectRevealer.h"
#include "skill/SkillUtil.h"

//////////////////////////////////////////////////////////////////////////////
//
// class PartyInviteInfo member methods
//
//////////////////////////////////////////////////////////////////////////////

string PartyInviteInfo::toString(void) const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "PartyInviteInfo(" << "Host:" << m_HostName << ",Guest:" << m_GuestName << ")";
    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//
// class PartyInviteInfoManager member methods
//
//////////////////////////////////////////////////////////////////////////////

PartyInviteInfoManager::PartyInviteInfoManager()

{
    __BEGIN_TRY

    m_Mutex.setName("PartyInviteInfoManager");

    __END_CATCH
}

PartyInviteInfoManager::~PartyInviteInfoManager()

{
    __BEGIN_TRY

    unordered_map<string, PartyInviteInfo*>::iterator itr = m_InfoMap.begin();
    for (; itr != m_InfoMap.end(); itr++) {
        PartyInviteInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }

    m_InfoMap.clear();

    __END_CATCH_NO_RETHROW
}

bool PartyInviteInfoManager::hasInviteInfo(const string& HostName)

{
    __BEGIN_TRY

    unordered_map<string, PartyInviteInfo*>::iterator itr = m_InfoMap.find(HostName);
    if (itr == m_InfoMap.end()) {
        return false;
    }

    return true;

    __END_CATCH
}

bool PartyInviteInfoManager::canInvite(Creature* pHost, Creature* pGuest)

{
    __BEGIN_TRY

    Assert(pHost != NULL && pGuest != NULL);

    // Only PCs may invite each other.
    if (!pHost->isPC() || !pGuest->isPC())
        return false;

    // Members of different races cannot invite each other.
    if (!isSameRace(pHost, pGuest))
        return false;

    // Cannot invite while either side is already inviting or being invited.
    PartyInviteInfo* pHostInfo = getInviteInfo(pHost->getName());
    PartyInviteInfo* pGuestInfo = getInviteInfo(pGuest->getName());
    if (pHostInfo != NULL || pGuestInfo != NULL)
        return false;

    return true;

    __END_CATCH
}

bool PartyInviteInfoManager::isInviting(Creature* pHost, Creature* pGuest)

{
    __BEGIN_TRY

    Assert(pHost != NULL && pGuest != NULL);

    PartyInviteInfo* pHostInfo = getInviteInfo(pHost->getName());
    PartyInviteInfo* pGuestInfo = getInviteInfo(pGuest->getName());

    if (pHostInfo == NULL || pGuestInfo == NULL)
        return false;

    // The two records must point at each other.
    // A(Host)      | B(Guest)
    // Host  : shit | Host  : fuck
    // Guest : fuck | Guest : shit
    if ((pHostInfo->getGuestName() == pGuestInfo->getHostName()) &&
        (pGuestInfo->getGuestName() == pHostInfo->getHostName()))
        return true;

    return false;

    __END_CATCH
}

void PartyInviteInfoManager::initInviteInfo(Creature* pHost, Creature* pGuest)

{
    __BEGIN_TRY

    if (hasInviteInfo(pHost->getName()) || hasInviteInfo(pGuest->getName())) {
        // An overlap has been seen here: party invite information is about to be
        // initialized by a CGPartyInvite packet, but invite information already
        // exists. This should throw an error, but the cause is unknown, so both
        // sides' information is cancelled instead.
        cancelInvite(pHost, pGuest);
        return;
    }

    PartyInviteInfo* pHostInfo = new PartyInviteInfo;
    pHostInfo->setHostName(pHost->getName());
    pHostInfo->setGuestName(pGuest->getName());

    if (!addInviteInfo(pHostInfo)) {
        delete pHostInfo;
    }

    PartyInviteInfo* pGuestInfo = new PartyInviteInfo;
    pGuestInfo->setHostName(pGuest->getName());
    pGuestInfo->setGuestName(pHost->getName());

    if (!addInviteInfo(pGuestInfo)) {
        delete pHostInfo;
    }

    __END_CATCH
}

void PartyInviteInfoManager::cancelInvite(Creature* pHost, Creature* pGuest)

{
    __BEGIN_TRY

    Assert(pHost != NULL && pGuest != NULL);

    int nCondition = 0;

    // Both must be player characters.
    if (!pHost->isPC() || !pGuest->isPC())
        nCondition = 1;
    if (!isSameRace(pHost, pGuest))
        nCondition = 2;
    if (!isInviting(pHost, pGuest))
        nCondition = 4;

    if (nCondition != 0) {
        cerr << "PartyInviteInfoManager::cancelInvite() : Error = " << nCondition << endl;
        // The opposite of the situation in initInviteInfo() happens here as
        // well, so the throw is commented out.
        // throw Error("PartyInviteInfoManager::cancelInvite()");
    }

    deleteInviteInfo(pHost->getName());
    deleteInviteInfo(pGuest->getName());

    __END_CATCH
}

void PartyInviteInfoManager::cancelInvite(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    PartyInviteInfo* pInfo = getInviteInfo(pCreature->getName());

    if (pInfo != NULL) {
        Zone* pZone = pCreature->getZone();

        const string& HostName = pInfo->getHostName();
        const string& GuestName = pInfo->getGuestName();

        Creature* pTargetCreature = NULL;

        pTargetCreature = pZone->getCreature(GuestName);

        // If the invited player is in the same zone, tell that player the invite
        // was refused.
        GCPartyInvite gcPartyInvite;
        gcPartyInvite.setTargetObjectID(pCreature->getObjectID());
        gcPartyInvite.setCode(GC_PARTY_INVITE_REJECT);

        if (pTargetCreature != NULL) {
            Player* pTargetPlayer = pTargetCreature->getPlayer();
            Assert(pTargetPlayer != NULL);
            pTargetPlayer->sendPacket(&gcPartyInvite);
        }

        deleteInviteInfo(HostName);
        deleteInviteInfo(GuestName);
    }

    __END_CATCH
}

bool PartyInviteInfoManager::addInviteInfo(PartyInviteInfo* pInfo)

{
    __BEGIN_TRY

    unordered_map<string, PartyInviteInfo*>::iterator itr = m_InfoMap.find(pInfo->getHostName());
    if (itr != m_InfoMap.end()) {
        cerr << "PartyInviteInfoManager::addInviteInfo() : DuplicatedException" << endl;
        // throw DuplicatedException("PartyInviteInfoManager::addInviteInfo() : DuplicatedException");

        return false;
    }

    m_InfoMap[pInfo->getHostName()] = pInfo;

    return true;

    __END_CATCH
}

void PartyInviteInfoManager::deleteInviteInfo(const string& HostName)

{
    __BEGIN_TRY

    unordered_map<string, PartyInviteInfo*>::iterator itr = m_InfoMap.find(HostName);
    if (itr != m_InfoMap.end()) {
        m_InfoMap.erase(itr);
    }

    __END_CATCH
}

PartyInviteInfo* PartyInviteInfoManager::getInviteInfo(const string& HostName)

{
    __BEGIN_TRY

    unordered_map<string, PartyInviteInfo*>::iterator itr = m_InfoMap.find(HostName);

    if (itr == m_InfoMap.end()) {
        return NULL;
    }


    return itr->second;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//
// class Party member methods
//
//////////////////////////////////////////////////////////////////////////////

Party::Party(Creature::CreatureClass CClass)

{
    __BEGIN_TRY

    // Fix the creature class that may belong to the party...
    m_CreatureClass = CClass;

    m_bFamilyPay = false;

    // Name the mutex. (for debugging)
    m_Mutex.setName("Party");

    __END_CATCH
}

Party::~Party()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_MemberMap.clear();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH_NO_RETHROW
}

// Find a party member by name and return it.
Creature* Party::getMember(const string& name) const

{
    __BEGIN_TRY

    // cout << "Party::getMember() : BEGIN" << endl;

    Creature* pCreature = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.find(name);
    if (itr == m_MemberMap.end()) {
        cerr << "Party::getMember() : NoSuchElementException" << endl;
        throw NoSuchElementException("Party::getMember() : NoSuchElementException");
    }

    pCreature = itr->second;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::getMember() : END" << endl;

    return pCreature;

    __END_CATCH
}

// Add a member.
void Party::addMember(Creature* pCreature)

{
    __BEGIN_TRY

    // cout << "Party::addMember() : BEGIN" << endl;

    // Not a race that may belong to the party...
    if (pCreature->getCreatureClass() != m_CreatureClass) {
        cerr << "Party::addMember() : Invalid Creature Class" << endl;
        throw Error("Party::addMember() : Invalid Creature Class");
    }

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::iterator itr = m_MemberMap.find(pCreature->getName());
    if (itr == m_MemberMap.end()) {
        m_MemberMap[pCreature->getName()] = pCreature;
    } else {
        // A creature already in the party is left as it is.
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::addMember() : END" << endl;

    __END_CATCH
}

// Delete a member from the party.
void Party::deleteMember(const string& name)

{
    __BEGIN_TRY

    // cout << "Party::deleteMember() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::iterator itr = m_MemberMap.find(name);
    if (itr == m_MemberMap.end()) {
        // cerr << "Party::deleteMember() : NoSuchElementException" << endl;
        // throw NoSuchElementException("Party::deleteMember() : NoSuchElementException");

        return;
    }

    //	itr->second->removeFlag( Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR );
    m_MemberMap.erase(itr);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::deleteMember() : END" << endl;

    __END_CATCH
}

// Check whether the party has a member with the given name.
bool Party::hasMember(const string& name) const

{
    __BEGIN_TRY

    // cout << "Party::hasMember() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.find(name);
    if (itr == m_MemberMap.end()) {
        // cout << "Party::hasMember() : END" << endl;

        return false;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::hasMember() : END" << endl;

    return true;

    __END_CATCH
}

// Used only by the global party manager...
// Before the party is broken up, set the members' party IDs to 0 and
// delete the party with that ID from the local party managers.
void Party::destroyParty(void)

{
    __BEGIN_TRY

    // cout << "Party::destroyParty() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);
        pCreature->setPartyID(0);
        //		pCreature->removeFlag( Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR );

        // cout << "Cleared the party ID of creature [" << pCreature->getName() << "]." << endl;

        // Delete the matching party object from each zone's local party manager.
        Zone* pZone = pCreature->getZone();
        if (pZone != NULL) {
            LocalPartyManager* pLocalPartyManager = pZone->getLocalPartyManager();
            Assert(pLocalPartyManager != NULL);
            pLocalPartyManager->deletePartyMember(m_ID, pCreature);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::destroyParty() : END" << endl;

    __END_CATCH
}

// Send a packet to the party members.
void Party::broadcastPacket(Packet* pPacket, Creature* pOwner) {
    __BEGIN_TRY

    // cout << "Party::broadcastPacket() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);

        if (pCreature != pOwner)
            pCreature->getPlayer()->sendPacket(pPacket);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::broadcastPacket() : END" << endl;

    __END_CATCH
}

// Build the GCPartyJoined packet that goes out to the party members
// when a new member is added.
void Party::makeGCPartyJoined(GCPartyJoined* pGCPartyJoined) const

{
    __BEGIN_TRY

    // cout << "Party::makeGCPartyJoined() : BEGIN" << endl;

    Assert(pGCPartyJoined != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);

        PARTY_MEMBER_INFO* pInfo = new PARTY_MEMBER_INFO;

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            pInfo->name = pSlayer->getName();
            pInfo->sex = pSlayer->getSex();
            pInfo->hair_style = pSlayer->getHairStyle();
            pInfo->ip = pSlayer->getIP();
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            pInfo->name = pVampire->getName();
            pInfo->sex = pVampire->getSex();
            pInfo->hair_style = 0;
            pInfo->ip = pVampire->getIP();
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            pInfo->name = pOusters->getName();
            pInfo->sex = pOusters->getSex();
            pInfo->hair_style = 0;
            pInfo->ip = pOusters->getIP();
        } else {
            Assert(false);
        }

        pGCPartyJoined->addMemberInfo(pInfo);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "Party::makeGCPartyJoined() : END" << endl;

    __END_CATCH
}

int Party::getSize(void) const

{
    __BEGIN_TRY

    return m_MemberMap.size();

    __END_CATCH
}

unordered_map<string, Creature*> Party::getMemberMap(void) {
    __BEGIN_TRY

    return m_MemberMap;

    __END_CATCH
}

int Party::getAdjacentMemberSize(Creature* pLeader) const

{
    __BEGIN_TRY

    // cout << "Party::getAdjacentMemberSize() : BEGIN" << endl;

    Zone* pZone = pLeader->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    int rValue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);

        // The party count includes the caller, so no check is made for
        // whether this is the same creature or a different one.
        Zone* pTZone = pCreature->getZone();

        // Matching zone pointers mean the creatures are in the same zone.
        if (pTZone == pZone && pCreature->getDistance(cx, cy) <= 8)
            rValue++;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // The caller is included, so the result must be at least 1.
    // Assert(rValue >= 1);
    if (rValue == 0)
        rValue = 1;

    // cout << "Party::getAdjacentMemberSize() : END" << endl;

    return rValue;

    __END_CATCH
}

int Party::getAdjacentMemberSize_LOCKED(Creature* pLeader) const

{
    __BEGIN_TRY

    // cout << "Party::getAdjacentMemberSize() : BEGIN" << endl;

    Zone* pZone = pLeader->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    int rValue = 0;

    //__ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);

        // The party count includes the caller, so no check is made for
        // whether this is the same creature or a different one.
        Zone* pTZone = pCreature->getZone();

        // Matching zone pointers mean the creatures are in the same zone.
        if (pTZone == pZone && pCreature->getDistance(cx, cy) <= 8)
            rValue++;
    }

    //__LEAVE_CRITICAL_SECTION(m_Mutex)

    // The caller is included, so the result must be at least 1.
    Assert(rValue >= 1);

    // cout << "Party::getAdjacentMemberSize() : END" << endl;

    return rValue;

    __END_CATCH
}

// Raise the attribute experience of the leader and the party members.
// The leader's gain is put into LeaderModifyInfo, and the other members'
// gains are sent in packets built separately.
int Party::shareAttrExp(Creature* pLeader, int amount, int STRMultiplier, int DEXMultiplier, int INTMultiplier,
                        ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    Assert(pLeader != NULL);
    Assert(pLeader->isSlayer());

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;
    int LevelSum = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members that will gain experience.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);

        // This function is called only through the local party manager,
        // so this party can be assumed to be a local party.
        // Inside a local party there is no need to check for the same zone,
        // so only the distance is checked.
        // Doing this distance computation on every calculation is a little
        // heavy, but members in the same zone arguably ought to get the
        // experience bonus.
        if (pCreature->getDistance(cx, cy) <= 8) {
            // Assert(pCreature->getZone() == pLeader->getZone());

            // Somewhere -- probably PCManager::killCreature -- the creature's
            // Zone changes, so the zones are compared here.
            // The exact place has not been found.
            if (pCreature->getZone() == pLeader->getZone()) {
                MemberList.push_back(pCreature);

                // While looking for nearby party members, also accumulate their level sum.
                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                    LevelSum += pSlayer->getSlayerLevel();
                } else if (pCreature->isVampire()) {
                    // Can a Vampire be in a Slayer party?
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                    LevelSum += pVampire->getLevel();
                }
            }
        }
    }

    // Amplify the experience by the number of party members.
    int nMemberSize = MemberList.size();

    // cout << "Number of party members : " << nMemberSize << endl;
    // cout << "Original experience : " << amount << endl;

    if (nMemberSize == 1) {
        __CRITICAL_SECTION_LOCK.unlock();

        Assert(pLeader->isSlayer());
        Slayer* pLeaderSlayer = dynamic_cast<Slayer*>(pLeader);

        // With a single member (no other member nearby) raise it alone and return.
        divideAttrExp(pLeaderSlayer, amount, STRMultiplier, DEXMultiplier, INTMultiplier, LeaderModifyInfo,
                      nMemberSize); // number of party members

        return 0;
    }

    switch (nMemberSize) {
    case 2:
        amount = getPercentValue(amount, 150);
        break;
    case 3:
        amount = getPercentValue(amount, 195);
        break;
    case 4:
        amount = getPercentValue(amount, 225);
        break;
    case 5:
        amount = getPercentValue(amount, 250);
        break;
    case 6:
        amount = getPercentValue(amount, 270);
        break;
    default:
        break;
    }

    // cout << "Amplified experience : " << amount << endl;
    // cout << "Level sum of the party members : " << LevelSum << endl;

    // Raise the experience of each party member.
    list<Creature*>::iterator itr = MemberList.begin();
    for (; itr != MemberList.end(); itr++) {
        Creature* pCreature = (*itr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        int myQuota = (int)((float)amount * (float)pSlayer->getSlayerLevel() / (float)LevelSum);

        // cout << "My share : " << myQuota << endl;

        if (pCreature->getName() != pLeader->getName()) {
            // cout << "[" << pCreature->getName() << "] is not the leader, so send a packet." << endl;

            Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
            if (pWeapon != NULL) {
                Item::ItemClass IClass = pWeapon->getItemClass();
                int _STR = 0, _DEX = 0, _INT = 0;
                switch (IClass) {
                case Item::ITEM_CLASS_SWORD:
                case Item::ITEM_CLASS_BLADE:
                    _STR = 8;
                    _DEX = 1;
                    _INT = 1;
                    break;
                case Item::ITEM_CLASS_SG:
                case Item::ITEM_CLASS_SMG:
                case Item::ITEM_CLASS_AR:
                case Item::ITEM_CLASS_SR:
                    _STR = 1;
                    _DEX = 8;
                    _INT = 1;
                    break;
                case Item::ITEM_CLASS_MACE:
                case Item::ITEM_CLASS_CROSS:
                    _STR = 1;
                    _DEX = 1;
                    _INT = 8;
                    break;
                default:
                    Assert(false);
                    break;
                }

                // Not the leader...
                GCModifyInformation gcModifyInformation;
                divideAttrExp(pSlayer, myQuota, _STR, _DEX, _INT, gcModifyInformation, nMemberSize);

                pSlayer->getPlayer()->sendPacket(&gcModifyInformation);
            }
        } else {
            // cout << "[" << pCreature->getName() << "] is the leader, so only prepare the packet." << endl;

            // For the leader, only store it so that it can be sent later.
            divideAttrExp(pSlayer, myQuota, STRMultiplier, DEXMultiplier, INTMultiplier, LeaderModifyInfo, nMemberSize);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return 0;

    __END_CATCH
}

int Party::shareVampireExp(Creature* pLeader, int amount, ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    Assert(pLeader != NULL);
    Assert(pLeader->isVampire());

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;
    int LevelSum = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members that will gain experience.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);

        // This function is called only through the local party manager,
        // so this party can be assumed to be a local party.
        // Inside a local party there is no need to check for the same zone,
        // so only the distance is checked.
        // Doing this distance computation on every calculation is a little
        // heavy, but members in the same zone arguably ought to get the
        // experience bonus.
        // A member in bat form gets no party experience.
        if (pCreature->getDistance(cx, cy) <= 8 && !pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
            MemberList.push_back(pCreature);

            // While looking for nearby party members, also accumulate their level sum.
            if (pCreature->isSlayer()) {
                // Can a Slayer be in a Vampire party?
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                LevelSum += pSlayer->getSlayerLevel();
            } else if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                LevelSum += pVampire->getLevel();
            }
        }
    }

    // Amplify the experience by the number of party members.
    int nMemberSize = MemberList.size();

    // cout << "Number of party members : " << nMemberSize << endl;
    // cout << "Original experience : " << amount << endl;

    if (nMemberSize == 1) {
        __CRITICAL_SECTION_LOCK.unlock();

        Assert(pLeader->isVampire());
        Vampire* pLeaderVampire = dynamic_cast<Vampire*>(pLeader);

        // With a single member (no other member nearby) raise it alone and return.
        increaseVampExp(pLeaderVampire, amount, LeaderModifyInfo);
        return 0;
    }

    switch (nMemberSize) {
    case 2:
        amount = getPercentValue(amount, 150);
        break;
    case 3:
        amount = getPercentValue(amount, 195);
        break;
    case 4:
        amount = getPercentValue(amount, 225);
        break;
    case 5:
        amount = getPercentValue(amount, 250);
        break;
    case 6:
        amount = getPercentValue(amount, 270);
        break;
    default:
        break;
    }

    // cout << "Amplified experience : " << amount << endl;
    // cout << "Level sum of the party members : " << LevelSum << endl;

    // Raise the experience of each party member.
    list<Creature*>::iterator itr = MemberList.begin();
    for (; itr != MemberList.end(); itr++) {
        Creature* pCreature = (*itr);
        Assert(pCreature != NULL);
        Assert(pCreature->isVampire());

        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        int myQuota = (int)((float)amount * (float)pVampire->getLevel() / (float)LevelSum);

        // cout << "My share : " << myQuota << endl;

        if (pCreature != pLeader) {
            // cout << "Not the leader, so send a packet." << endl;

            // Not the leader...
            GCModifyInformation gcModifyInformation;
            increaseVampExp(pVampire, myQuota, gcModifyInformation);
            pVampire->getPlayer()->sendPacket(&gcModifyInformation);
        } else {
            // cout << "This is the leader, so no packet is sent." << endl;

            // For the leader, only store it so that it can be sent later.
            increaseVampExp(pVampire, myQuota, LeaderModifyInfo);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return 0;

    __END_CATCH
}

int Party::shareOustersExp(Creature* pLeader, int amount, ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    Assert(pLeader != NULL);
    Assert(pLeader->isOusters());

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;
    int LevelSum = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members that will gain experience.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);

        // This function is called only through the local party manager,
        // so this party can be assumed to be a local party.
        // Inside a local party there is no need to check for the same zone,
        // so only the distance is checked.
        // Doing this distance computation on every calculation is a little
        // heavy, but members in the same zone arguably ought to get the
        // experience bonus.
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);

            // While looking for nearby party members, also accumulate their level sum.
            if (pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                LevelSum += pOusters->getLevel();
            }
        }
    }

    // Amplify the experience by the number of party members.
    int nMemberSize = MemberList.size();

    // cout << "Number of party members : " << nMemberSize << endl;
    // cout << "Original experience : " << amount << endl;

    if (nMemberSize == 1) {
        __CRITICAL_SECTION_LOCK.unlock();

        Assert(pLeader->isOusters());
        Ousters* pLeaderOusters = dynamic_cast<Ousters*>(pLeader);

        // With a single member (no other member nearby) raise it alone and return.
        increaseOustersExp(pLeaderOusters, amount, LeaderModifyInfo);
        return 0;
    }

    switch (nMemberSize) {
    case 2:
        amount = getPercentValue(amount, 150);
        break;
    case 3:
        amount = getPercentValue(amount, 195);
        break;
    case 4:
        amount = getPercentValue(amount, 225);
        break;
    case 5:
        amount = getPercentValue(amount, 250);
        break;
    case 6:
        amount = getPercentValue(amount, 270);
        break;
    default:
        break;
    }

    // Raise the experience of each party member.
    list<Creature*>::iterator itr = MemberList.begin();
    for (; itr != MemberList.end(); itr++) {
        Creature* pCreature = (*itr);
        Assert(pCreature != NULL);
        Assert(pCreature->isOusters());

        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        int myQuota = (int)((float)amount * (float)pOusters->getLevel() / (float)LevelSum);

        if (pCreature != pLeader) {
            // cout << "Not the leader, so send a packet." << endl;

            // Not the leader...
            GCModifyInformation gcModifyInformation;
            increaseOustersExp(pOusters, myQuota, gcModifyInformation);
            pOusters->getPlayer()->sendPacket(&gcModifyInformation);
        } else {
            // cout << "This is the leader, so no packet is sent." << endl;

            // For the leader, only store it so that it can be sent later.
            increaseOustersExp(pOusters, myQuota, LeaderModifyInfo);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return 0;

    __END_CATCH
}

void Party::shareRankExp(Creature* pLeader, int otherLevel)

{
    __BEGIN_TRY

    Assert(pLeader != NULL);
    Assert(pLeader->isPC());

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;
    int LevelSum = 0;

    int LevelSum2 = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members that will gain experience.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);

        // This function is called only through the local party manager,
        // so this party can be assumed to be a local party.
        // Inside a local party there is no need to check for the same zone,
        // so only the distance is checked.
        // Doing this distance computation on every calculation is a little
        // heavy, but members in the same zone arguably ought to get the
        // experience bonus.
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);

            // While looking for nearby party members, also accumulate their level sum.
            if (pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                LevelSum += pSlayer->getSlayerLevel();

                LevelSum2 += pSlayer->getHighestSkillDomainLevel();
            } else if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                LevelSum += pVampire->getLevel();

                LevelSum2 += pVampire->getLevel();
            } else if (pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                LevelSum += pOusters->getLevel();

                LevelSum2 += pOusters->getLevel();
            }
        }
    }

    // Amplify the experience by the number of party members.
    int nMemberSize = MemberList.size();

    // Compute the experience from the average level of the party members.
    int amount = (int)computeRankExp(LevelSum2 / nMemberSize, otherLevel);

    // cout << "Number of party members : " << nMemberSize << endl;
    // cout << "Original experience : " << amount << endl;

    if (nMemberSize == 1) {
        __CRITICAL_SECTION_LOCK.unlock();

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pLeader);

        // With a single member (no other member nearby) raise it alone and return.
        pPC->increaseRankExp(amount);
        return;
    }

    switch (nMemberSize) {
    case 2:
        amount = getPercentValue(amount, 150);
        break;
    case 3:
        amount = getPercentValue(amount, 195);
        break;
    case 4:
        amount = getPercentValue(amount, 225);
        break;
    case 5:
        amount = getPercentValue(amount, 250);
        break;
    case 6:
        amount = getPercentValue(amount, 270);
        break;
    default:
        break;
    }

    // cout << "Amplified experience : " << amount << endl;
    // cout << "Level sum of the party members : " << LevelSum << endl;

    // Raise the experience of each party member.
    list<Creature*>::iterator itr = MemberList.begin();
    for (; itr != MemberList.end(); itr++) {
        Creature* pCreature = (*itr);
        Assert(pCreature != NULL);

        // Give each nearby member a share of the experience in proportion to its level.
        int level = 0;
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            level = pSlayer->getSlayerLevel();

            int myQuota = amount * level / LevelSum;
            pSlayer->increaseRankExp(myQuota);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            level = pVampire->getLevel();

            int myQuota = amount * level / LevelSum;
            pVampire->increaseRankExp(myQuota);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            level = pOusters->getLevel();

            int myQuota = amount * level / LevelSum;
            pOusters->increaseRankExp(myQuota);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return;

    __END_CATCH
}

void Party::shareAdvancementExp(Creature* pLeader, int amount)

{
    __BEGIN_TRY

    Assert(pLeader != NULL);
    Assert(pLeader->isPC());

    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;
    int LevelSum = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members that will gain experience.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        PlayerCreature* pCreature = dynamic_cast<PlayerCreature*>(mitr->second);
        Assert(pCreature != NULL);

        // This function is called only through the local party manager,
        // so this party can be assumed to be a local party.
        // Inside a local party there is no need to check for the same zone,
        // so only the distance is checked.
        // Doing this distance computation on every calculation is a little
        // heavy, but members in the same zone arguably ought to get the
        // experience bonus.
        if (pCreature->isAdvanced() && pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
            LevelSum += pCreature->getLevel() + pCreature->getAdvancementClassLevel();
        }
    }

    // Amplify the experience by the number of party members.
    int nMemberSize = MemberList.size();

    // cout << "Number of party members : " << nMemberSize << endl;
    // cout << "Original experience : " << amount << endl;

    if (nMemberSize == 1) {
        __CRITICAL_SECTION_LOCK.unlock();

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pLeader);

        // With a single member (no other member nearby) raise it alone and return.
        if (pPC->isAdvanced())
            pPC->increaseAdvancementClassExp(amount);
        return;
    }

    switch (nMemberSize) {
    case 2:
        amount = getPercentValue(amount, 150);
        break;
    case 3:
        amount = getPercentValue(amount, 195);
        break;
    case 4:
        amount = getPercentValue(amount, 225);
        break;
    case 5:
        amount = getPercentValue(amount, 250);
        break;
    case 6:
        amount = getPercentValue(amount, 270);
        break;
    default:
        break;
    }

    // cout << "Amplified experience : " << amount << endl;
    // cout << "Level sum of the party members : " << LevelSum << endl;

    // Raise the experience of each party member.
    list<Creature*>::iterator itr = MemberList.begin();
    for (; itr != MemberList.end(); itr++) {
        PlayerCreature* pCreature = dynamic_cast<PlayerCreature*>(*itr);
        Assert(pCreature != NULL);

        // Give each nearby member a share of the experience in proportion to its level.
        int myQuota = amount * pCreature->getLevel() / LevelSum;
        pCreature->increaseAdvancementClassExp(myQuota);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return;

    __END_CATCH
}

void Party::shareRevealer(Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    if (!pCaster->isFlag(Effect::EFFECT_CLASS_REVEALER)) {
        throw Error("No Revealer effect is applied");
    }

    // Get the caster's Revealer skill level.
    Slayer* pSlayer = dynamic_cast<Slayer*>(pCaster);
    Assert(pSlayer != NULL);
    SkillSlot* pSkillSlot = pSlayer->getSkill(SKILL_REVEALER);
    Assert(pSkillSlot != NULL);
    ExpLevel_t ExpLevel = pSkillSlot->getExpLevel();

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            EffectRevealer* pEffectRevealer = new EffectRevealer(pSlayer);
            pEffectRevealer->setSkillLevel(ExpLevel);
            pEffectRevealer->setDeadline(Duration);
            EffectManager* pEffectManager = pSlayer->getEffectManager();
            pEffectManager->addEffect(pEffectRevealer);
            pSlayer->setFlag(Effect::EFFECT_CLASS_REVEALER);

            pZone->updateMineScan(pSlayer);
            //			pZone->updateInvisibleScan( pSlayer );
            pZone->updateHiddenScan(pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_REVEALER);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Party::shareActivation(Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            EffectActivation* pEffectActivation = new EffectActivation(pSlayer);
            pEffectActivation->setDeadline(Duration);
            EffectManager* pEffectManager = pSlayer->getEffectManager();
            pEffectManager->addEffect(pEffectActivation);
            pSlayer->setFlag(Effect::EFFECT_CLASS_ACTIVATION);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_ACTIVATION);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Party::shareGnomesWhisper(Creature* pCaster, int Duration, int SkillLevel)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isOusters());

        if (pCreature != pCaster) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            // Build the effect class and attach it.
            EffectGnomesWhisper* pEffect = new EffectGnomesWhisper(pOusters);
            pEffect->setDeadline(Duration);
            pEffect->setLevel(SkillLevel);
            pOusters->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER);

            pZone->updateDetectScan(pOusters);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_GNOMES_WHISPER);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Party::shareHolyArmor(Creature* pCaster, int DefBonus, int SkillLevel)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster) {
            int Duration = (30 + SkillLevel / 2) * 10;
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            // Build the effect class and attach it.
            EffectHolyArmor* pEffect = new EffectHolyArmor(pSlayer);
            pEffect->setDeadline(Duration);
            pEffect->setDefBonus(DefBonus);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_HOLY_ARMOR);

            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            pSlayer->sendModifyInfo(prev);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_HOLY_ARMOR);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

bool Party::shareWaterElementalHeal(Creature* pCaster, int HealPoint)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    bool ret = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 13) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return false;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isOusters());

        Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pTargetOusters != NULL);

        if (pTargetOusters != pCaster && pTargetOusters->getHP() < pTargetOusters->getHP(ATTR_MAX) &&
            pTargetOusters->getHP() > 0) {
            ret = true;
            GCModifyInformation gcMI;
            HP_t final = min((int)pTargetOusters->getHP(ATTR_MAX), pTargetOusters->getHP() + HealPoint);
            if (final > pTargetOusters->getHP(ATTR_MAX) - pTargetOusters->getSilverDamage()) {
                pTargetOusters->setSilverDamage(pTargetOusters->getHP(ATTR_MAX) - final);
                gcMI.addShortData(MODIFY_SILVER_DAMAGE, pTargetOusters->getSilverDamage());
            }

            if (pTargetOusters->getHP() != final) {
                pTargetOusters->setHP(final);
                gcMI.addShortData(MODIFY_CURRENT_HP, final);
            }

            GCStatusCurrentHP gcHP;
            gcHP.setObjectID(pTargetOusters->getObjectID());
            gcHP.setCurrentHP(final);

            pZone->broadcastPacket(pTargetOusters->getX(), pTargetOusters->getY(), &gcHP);

            pTargetOusters->getPlayer()->sendPacket(&gcMI);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pTargetOusters->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_WATER_ELEMENTAL_HEALED);
            gcAddEffect.setDuration(0);
            pZone->broadcastPacket(pTargetOusters->getX(), pTargetOusters->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return ret;

    __END_CATCH
}

void Party::shareGDRLairEnter(Creature* pLeader)

{
    __BEGIN_TRY

    //	Zone*       pZone = pLeader->getZone();
    ZoneCoord_t cx = pLeader->getX();
    ZoneCoord_t cy = pLeader->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);

        //		pCreature->setFlag( Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR );
        if (!pCreature->isFlag(Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR)) {
            EffectCanEnterGDRLair* pEffect = new EffectCanEnterGDRLair(pCreature);
            pEffect->setDeadline(216000);

            pCreature->setFlag(pEffect->getEffectClass());
            pCreature->addEffect(pEffect);

            pEffect->create(pCreature->getName());

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pCreature->getObjectID());
            gcAddEffect.setEffectID(pEffect->getSendEffectClass());
            gcAddEffect.setDuration(21600);

            pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
        }
        //			addSimpleCreatureEffect( pCreature, Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR, 216000 );
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void Party::shareDetectHidden(Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            EffectDetectHidden* pEffectDetectHidden = new EffectDetectHidden(pSlayer);
            pEffectDetectHidden->setDeadline(Duration);
            EffectManager* pEffectManager = pSlayer->getEffectManager();
            pEffectManager->addEffect(pEffectDetectHidden);
            pSlayer->setFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN);

            pZone->updateHiddenScan(pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_DETECT_HIDDEN);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Party::shareDetectInvisibility(Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            EffectDetectInvisibility* pEffectDetectInvisibility = new EffectDetectInvisibility(pSlayer);
            pEffectDetectInvisibility->setDeadline(Duration);
            EffectManager* pEffectManager = pSlayer->getEffectManager();
            pEffectManager->addEffect(pEffectDetectInvisibility);
            pSlayer->setFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY);

            pZone->updateInvisibleScan(pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_DETECT_INVISIBILITY);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Party::shareExpansion(Creature* pCaster, int Duration, int Percent)

{
    __BEGIN_TRY

    Zone* pZone = pCaster->getZone();
    ZoneCoord_t cx = pCaster->getX();
    ZoneCoord_t cy = pCaster->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        Assert(pCreature != NULL);
        Assert(pCreature->isSlayer());

        if (pCreature != pCaster && pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            // If the same effect already exists, the previous one must be deleted.
            if (pSlayer->isFlag(Effect::EFFECT_CLASS_EXPANSION)) {
                pSlayer->deleteEffect(Effect::EFFECT_CLASS_EXPANSION);
            }

            EffectExpansion* pEffectExpansion = new EffectExpansion(pSlayer);
            pEffectExpansion->setDeadline(Duration);
            pEffectExpansion->setHPBonus(Percent);
            pSlayer->addEffect(pEffectExpansion);
            pSlayer->setFlag(Effect::EFFECT_CLASS_EXPANSION);

            // The effect has been attached, so recompute the attributes.
            // Then tell the owner what changed.
            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            pSlayer->sendRealWearingInfo();
            pSlayer->sendModifyInfo(prev);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_EXPANSION);
            gcAddEffect.setDuration(Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

            // Max HP has probably changed, so broadcast HP as well.
            GCOtherModifyInfo gcOtherModifyInfo;
            makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void Party::dissectCorpse(Creature* pDissecter, MonsterCorpse* pCorpse) {
    __BEGIN_TRY

    cout << std::source_location::current().function_name() << endl;
    if (getSize() != 2)
        return;
    cout << "dissectCorpse!" << endl;

    ZoneCoord_t cx = pDissecter->getX();
    ZoneCoord_t cy = pDissecter->getY();

    list<Creature*> MemberList;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Collect the list of nearby party members the effect will be applied to.
    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature != NULL);
        if (pCreature->getDistance(cx, cy) <= 8) {
            MemberList.push_back(pCreature);
        }
    }

    if (MemberList.size() == 1) {
        return;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    list<Creature*>::iterator litr = MemberList.begin();
    for (; litr != MemberList.end(); litr++) {
        Creature* pCreature = (*litr);
        if (pCreature == pDissecter)
            continue;
        Assert(pCreature != NULL);
        Assert(pCreature->isPC());

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        pPC->getGQuestManager()->partyDissect(pCorpse);
    }

    __END_CATCH
}

void Party::eventPartyCrash() {
    __BEGIN_TRY

    cout << std::source_location::current().function_name() << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(mitr->second);
        Assert(pPC != NULL);
        pPC->getGQuestManager()->eventPartyCrash();
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void Party::refreshFamilyPay() {
    bool oldFamilyPay = m_bFamilyPay;
    m_bFamilyPay = false;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator mitr = m_MemberMap.begin();
    for (; mitr != m_MemberMap.end(); mitr++) {
        Creature* pCreature = mitr->second;
        Assert(pCreature->isPC());
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
        Assert(pGamePlayer != NULL);

        if (pGamePlayer->isFamilyPayAvailable()) {
            m_bFamilyPay = true;
            break;
        }
    }

    // When the family plan state changes, apply it to every party member.
    // Members who subscribe to the family plan themselves are excluded.
    if (oldFamilyPay != m_bFamilyPay) {
        mitr = m_MemberMap.begin();

        for (; mitr != m_MemberMap.end(); mitr++) {
            Creature* pCreature = mitr->second;
            Assert(pCreature->isPC());
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
            Assert(pGamePlayer != NULL);

            if (!pGamePlayer->isFamilyPayAvailable()) {
                if (m_bFamilyPay) {
                    // Apply the family plan.
                    pGamePlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_FREE_PASS);
                } else {
                    // Report that the family plan no longer applies.
                    pGamePlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_FREE_PASS_END);
                }
            }
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

string Party::toString(void) const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "Party(" << "ID:" << m_ID << ",CClass:" << m_CreatureClass << ",Member(";

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::const_iterator itr = m_MemberMap.begin();
    for (; itr != m_MemberMap.end(); itr++) {
        Creature* pCreature = itr->second;
        Assert(pCreature != NULL);
        msg << pCreature->getName() << ",";
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    msg << "))";


    return msg.toString();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//
// class PartyManager member methods
//
//////////////////////////////////////////////////////////////////////////////

PartyManager::PartyManager()

{
    __BEGIN_TRY

    m_Mutex.setName("PartyManager");

    __END_CATCH
}

PartyManager::~PartyManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

bool PartyManager::createParty(int ID, Creature::CreatureClass CClass)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Look for a party with a duplicate ID.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(ID);
    if (itr != m_PartyMap.end()) {
        return false;
    }

    Party* pParty = new Party(CClass);
    pParty->setID(ID);

    m_PartyMap[ID] = pParty;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return true;

    __END_CATCH
}

Party* PartyManager::getParty(int ID) // by sigi. 2002.10.14

{
    __BEGIN_TRY

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        return NULL;
    }

    return itr->second;

    __END_CATCH
}


bool PartyManager::addPartyMember(int ID, Creature* pCreature) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        // Create it here if there is none.
        Party* pNewParty = new Party(pCreature->getCreatureClass());
        pNewParty->setID(ID);

        m_PartyMap[ID] = pNewParty;

        // A freshly created party is empty, so this check never fires.
        if (pNewParty->getSize() >= PARTY_MAX_SIZE) {
            return false;
        }

        pNewParty->addMember(pCreature);
    } else {
        Party* pParty = itr->second;
        Assert(pParty != NULL);

        if (pParty->getSize() >= PARTY_MAX_SIZE) {
            return false;
        }

        pParty->addMember(pCreature);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return true;

    __END_CATCH
}

bool PartyManager::deletePartyMember(int ID, Creature* pCreature)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        return false;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->deleteMember(pCreature->getName());

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return true;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//
// class LocalPartyManager member methods
//
//////////////////////////////////////////////////////////////////////////////

LocalPartyManager::LocalPartyManager()

{
    __BEGIN_TRY

    m_Mutex.setName("LocalPartyManager");

    __END_CATCH
}

LocalPartyManager::~LocalPartyManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void LocalPartyManager::heartbeat(void)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    unordered_map<int, Party*>::iterator before = m_PartyMap.end();
    unordered_map<int, Party*>::iterator current = m_PartyMap.begin();

    while (current != m_PartyMap.end()) {
        Party* pParty = current->second;
        Assert(pParty != NULL);

        if (pParty->getSize() == 0) {
            // cout << "Local party size reached 0, deleting party object [" << pParty->getID() << "]." << endl;

            SAFE_DELETE(pParty);

            m_PartyMap.erase(current);

            if (before == m_PartyMap.end()) // first element
            {
                current = m_PartyMap.begin();
            } else // !first element
            {
                current = before;
                current++;
            }
        } else {
            before = current++;
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

int LocalPartyManager::getAdjacentMemberSize(int PartyID, Creature* pLeader) const

{
    __BEGIN_TRY

    int size = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    size = pParty->getAdjacentMemberSize(pLeader);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return size;

    __END_CATCH
}

int LocalPartyManager::shareAttrExp(int PartyID, Creature* pLeader, int amount, int STRMultiplier, int DEXMultiplier,
                                    int INTMultiplier, ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    int rvalue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    rvalue = pParty->shareAttrExp(pLeader, amount, STRMultiplier, DEXMultiplier, INTMultiplier, LeaderModifyInfo);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return rvalue;

    __END_CATCH
}

int LocalPartyManager::shareVampireExp(int PartyID, Creature* pLeader, int amount, ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    int rvalue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    rvalue = pParty->shareVampireExp(pLeader, amount, LeaderModifyInfo);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return rvalue;

    __END_CATCH
}

int LocalPartyManager::shareOustersExp(int PartyID, Creature* pLeader, int amount, ModifyInfo& LeaderModifyInfo) const

{
    __BEGIN_TRY

    int rvalue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    rvalue = pParty->shareOustersExp(pLeader, amount, LeaderModifyInfo);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return rvalue;

    __END_CATCH
}

int LocalPartyManager::shareRankExp(int PartyID, Creature* pLeader, int amount) const

{
    __BEGIN_TRY

    // int rvalue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareRankExp(pLeader, amount);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return 0;

    __END_CATCH
}

void LocalPartyManager::shareRevealer(int PartyID, Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareRevealer(pCaster, Duration);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareDetectHidden(int PartyID, Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareDetectHidden(pCaster, Duration);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareDetectInvisibility(int PartyID, Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareDetectInvisibility(pCaster, Duration);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareExpansion(int PartyID, Creature* pCaster, int Duration, int percent)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareExpansion(pCaster, Duration, percent);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareActivation(int PartyID, Creature* pCaster, int Duration)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareActivation(pCaster, Duration);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareGnomesWhisper(int PartyID, Creature* pCaster, int Duration, int SkillLevel)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareGnomesWhisper(pCaster, Duration, SkillLevel);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

void LocalPartyManager::shareHolyArmor(int PartyID, Creature* pCaster, int DefBonus, int SkillLevel)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareHolyArmor(pCaster, DefBonus, SkillLevel);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

bool LocalPartyManager::shareWaterElementalHeal(int PartyID, Creature* pCaster, int HealPoint)

{
    __BEGIN_TRY

    bool ret = false;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return false;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    ret = pParty->shareWaterElementalHeal(pCaster, HealPoint);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return ret;

    __END_CATCH
}

void LocalPartyManager::shareGDRLairEnter(int PartyID, Creature* pLeader)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareGDRLairEnter(pLeader);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    __END_CATCH
}

int LocalPartyManager::shareAdvancementExp(int PartyID, Creature* pLeader, int amount) const {
    __BEGIN_TRY

    // int rvalue = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex);

    // Look for the matching party.
    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.find(PartyID);
    if (itr == m_PartyMap.end()) {
        return 0;
    }

    Party* pParty = itr->second;
    Assert(pParty != NULL);

    pParty->shareAdvancementExp(pLeader, amount);

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    return 0;

    __END_CATCH
}

string LocalPartyManager::toString(void) const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "LocalPartyManager(";

    __ENTER_CRITICAL_SECTION(m_Mutex);

    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.begin();
    for (; itr != m_PartyMap.end(); itr++) {
        Party* pParty = itr->second;
        Assert(pParty != NULL);
        msg << pParty->toString() << ",";
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex);

    msg << ")";
    return msg.toString();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//
// class GlobalPartyManager member methods
//
//////////////////////////////////////////////////////////////////////////////

GlobalPartyManager::GlobalPartyManager()

{
    __BEGIN_TRY

    m_PartyIDRegistry = 0;
    m_Mutex.setName("GlobalPartyManager");

    __END_CATCH
}

GlobalPartyManager::~GlobalPartyManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

bool GlobalPartyManager::canAddMember(int ID)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<int, Party*>::iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        return false;
    }

    Party* pParty = itr->second;

    if (pParty->getSize() >= PARTY_MAX_SIZE) {
        return false;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return true;

    __END_CATCH
}

bool GlobalPartyManager::addPartyMember(int ID, Creature* pCreature) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // cout << "GlobalPartyManager::addPartyMember() : BEGIN" << endl;

    // First find the party and check the number of members.
    unordered_map<int, Party*>::iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        // cerr << "GlobalPartyManager::addPartyMember() : NoSuchElementException" << endl;
        // throw NoSuchElementException("GlobalPartyManager::addPartyMember() : NoSuchElementException");

        return false;
    }

    Party* pParty = itr->second;

    if (pParty->getSize() >= PARTY_MAX_SIZE) {
        // cout << "Party max size exceeded" << endl;
        // cout << "GlobalPartyManager::addPartyMember() : END" << endl;
        return false;
    }

    if (pParty->getSize() == 2) {
        pParty->eventPartyCrash();
    }

    // Add the party member.
    pParty->addMember(pCreature);
    pCreature->setPartyID(pParty->getID());

    // Tell the other members that a member has been added.
    // When two players first form a party, one member is added and a party
    // list holding a single name would go out to that player.
    // Then the second member joins and a list holding two names goes out to
    // both of them in turn.
    // So nothing is sent while the party holds one member, which keeps the
    // party list from going out twice.
    if (pParty->getSize() != 1) {
        GCPartyJoined gcPartyJoined;
        pParty->makeGCPartyJoined(&gcPartyJoined);
        pParty->broadcastPacket(&gcPartyJoined);

        unordered_map<string, Creature*> memberMap = pParty->getMemberMap();
        unordered_map<string, Creature*>::iterator itr = memberMap.begin();
        GCOtherGuildName gcOtherGuildName;

        for (; itr != memberMap.end(); ++itr) {
            Creature* pTargetCreature = itr->second;
            if (pTargetCreature != NULL && pTargetCreature->isPC()) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);

                if (pPC != NULL && pPC->getGuildID() != pPC->getCommonGuildID()) {
                    gcOtherGuildName.setObjectID(pPC->getObjectID());
                    gcOtherGuildName.setGuildID(pPC->getGuildID());
                    gcOtherGuildName.setGuildName(pPC->getGuildName());

                    pParty->broadcastPacket(&gcOtherGuildName);
                }
            }
        }
    }

    // Family plan handling.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    if (pGamePlayer != NULL) {
        if (pParty->isFamilyPay() && !pGamePlayer->isFamilyPayAvailable()) {
            // In a family plan party, grant the pay zone pass.
            pGamePlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_FREE_PASS);
        } else if (pGamePlayer->isFamilyPayAvailable()) {
            // A joining family plan member turns the party into a family plan party.
            pParty->refreshFamilyPay();
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "GlobalPartyManager::addPartyMember() : END" << endl;

    return true;

    __END_CATCH
}

bool GlobalPartyManager::deletePartyMember(int ID, Creature* pCreature)

{
    __BEGIN_TRY

    // cout << "GlobalPartyManager::deletePartyMember() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<int, Party*>::iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        // m_Mutex.unlock();

        cerr << "GlobalPartyManager::deletePartyMember() : NoSuchElementException" << endl;
        // throw NoSuchElementException("GlobalPartyManager::deletePartyMember() : NoSuchElementException");

        // The caller does not handle the NoSuch case either.
        return false;
    }

    Party* pParty = itr->second;

    // Tell the members that a member has been expelled from the party.
    GCPartyLeave gcPartyLeave;
    gcPartyLeave.setExpellee(pCreature->getName());
    gcPartyLeave.setExpeller("");
    pParty->broadcastPacket(&gcPartyLeave);

    pParty->eventPartyCrash();

    // GCPartyLeave must reach the leaving member as well, so the packet is
    // broadcast first and the member is deleted from the party afterwards.
    pParty->deleteMember(pCreature->getName());
    pCreature->setPartyID(0);

    // Family plan handling.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    if (pGamePlayer != NULL) {
        if (pGamePlayer->isFamilyPayAvailable()) {
            // When a family plan member leaves, recompute the party's family plan state.
            pParty->refreshFamilyPay();
        } else if (pParty->isFamilyPay()) {
            // In a family plan party, end the family plan for the member.
            pGamePlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_FREE_PASS_END);
        }
    }

    // Delete the party once its size drops to 1.
    if (pParty->getSize() == 1) {
        m_PartyMap.erase(itr);

        // Set the remaining members' party IDs to 0 and
        // delete the party from each local party manager.
        pParty->destroyParty();

        // Delete the object.
        SAFE_DELETE(pParty);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "GlobalPartyManager::deletePartyMember() : END" << endl;

    return true;

    __END_CATCH
}

bool GlobalPartyManager::expelPartyMember(int ID, Creature* pExpeller, const string& ExpelleeName)

{
    __BEGIN_TRY

    // cout << "GlobalPartyManager::expelPartyMember() : BEGIN" << endl;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // First find the party.
    unordered_map<int, Party*>::iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        cerr << "GlobalPartyManager::expelPartyMember() : NoSuchElementException" << endl;

        // The caller does not handle the NoSuch case either.
        // throw NoSuchElementException("GlobalPartyManager::expelPartyMember() : NoSuchElementException");

        return false;
    }

    Party* pParty = itr->second;

    // Check that the expeller is in this party.
    if (!pParty->hasMember(pExpeller->getName())) {
        // An error...?
        // cout << "The expeller is not in the party" << endl;
        // cout << "GlobalPartyManager::expelPartyMember() : END" << endl;
        return false;
    }

    // Check that the target of the expulsion is in the party.
    if (!pParty->hasMember(ExpelleeName)) {
        // An error...?
        // cout << "The one being expelled is not in the party" << endl;
        // cout << "GlobalPartyManager::expelPartyMember() : END" << endl;
        return false;
    }

    // Tell the members that a member has been expelled from the party.
    GCPartyLeave gcPartyLeave;
    gcPartyLeave.setExpellee(ExpelleeName);
    gcPartyLeave.setExpeller(pExpeller->getName());
    pParty->broadcastPacket(&gcPartyLeave);

    pParty->eventPartyCrash();

    // cout << "Tell the members that a member has been expelled from the party." << endl;

    // Delete the expelled member from the party.
    // * NOTE *
    // The member is not deleted first: the packet is sent and the deletion
    // follows, because the packet that goes to the expelled member and the
    // packet that tells the other members are the same packet.
    Creature* pExpellee = pParty->getMember(ExpelleeName);
    pExpellee->setPartyID(0);
    pParty->deleteMember(ExpelleeName);

    // Family plan handling.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pExpellee->getPlayer());
    if (pGamePlayer != NULL) {
        if (pGamePlayer->isFamilyPayAvailable()) {
            // When a family plan member leaves, recompute the party's family plan state.
            pParty->refreshFamilyPay();
        } else if (pParty->isFamilyPay()) {
            // In a family plan party, end the family plan for the member.
            pGamePlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_FREE_PASS_END);
        }
    }

    // cout << "Deleted [" << pExpellee->getName() << "] from the party." << endl;

    // Delete the party once its size drops to 1.
    if (pParty->getSize() == 1) {
        // cout << "Party size reached 1, deleting the party." << endl;

        m_PartyMap.erase(itr);

        // cout << "Erase itr" << endl;

        // Set the remaining members' party IDs to 0 and
        // delete the party from each local party manager.
        pParty->destroyParty();

        // cout << "After Party::destroyParty()" << endl;

        // Delete the object.
        SAFE_DELETE(pParty);

        // cout << "After object deletion" << endl;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // cout << "GlobalPartyManager::expelPartyMember() : END" << endl;

    return true;

    __END_CATCH
}

void GlobalPartyManager::refreshFamilyPay(int ID) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    // First find the party.
    unordered_map<int, Party*>::iterator itr = m_PartyMap.find(ID);
    if (itr == m_PartyMap.end()) {
        cerr << "GlobalPartyManager::refreshFamilyPay() : NoSuchElementException" << endl;

        return;
    }

    Party* pParty = itr->second;

    pParty->refreshFamilyPay();

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

int GlobalPartyManager::registerParty(void)

{
    __BEGIN_TRY

    int PartyID = 0;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    PartyID = ++m_PartyIDRegistry;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return PartyID;

    __END_CATCH
}

string GlobalPartyManager::toString(void) const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GlobalPartyManager(";

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<int, Party*>::const_iterator itr = m_PartyMap.begin();
    for (; itr != m_PartyMap.end(); itr++) {
        Party* pParty = itr->second;
        Assert(pParty != NULL);
        msg << pParty->toString() << ",";
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    msg << ")";
    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//
// Global functions for convenience...
//
//////////////////////////////////////////////////////////////////////////////
void deleteAllPartyInfo(Creature* pCreature)

{
    __BEGIN_TRY

    // cout << "DeleteAllPartyInfo BEGIN" << endl;

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
    Assert(pPIIM != NULL);

    // When the class is deleted the matching party invite information must be
    // deleted, and the other side of the invitation must be told as well.
    PartyInviteInfo* pInviteInfo = pPIIM->getInviteInfo(pCreature->getName());
    if (pInviteInfo != NULL) {
        pPIIM->cancelInvite(pCreature);
    }

    int PartyID = pCreature->getPartyID();

    // If the creature belongs to a party, remove it from the party and
    // tell the other party members.
    if (PartyID != 0) {
        // Delete from the global party and notify the members.
        de::gameContext().parties().deletePartyMember(PartyID, pCreature);

        // Delete the information from the local party manager of the current zone.
        // Zone::deleteCreature() already removes a creature, inside
        // LocalPartyManager, from the party it belongs to when the creature
        // leaves that zone, so deleting here should not be needed.
        //
        // The exact place is unknown, but somewhere the local party seems not to
        // clear the pointer reliably.
        // So this part, once commented out, is enabled again.
        Zone* pZone = pCreature->getZone();
        if (pZone != NULL) {
            // Delete from the local party.
            LocalPartyManager* pLocalPartyManager = pZone->getLocalPartyManager();
            Assert(pLocalPartyManager != NULL);
            pLocalPartyManager->deletePartyMember(PartyID, pCreature);
        }

        // The global party already sets the party ID to 0, but it is set to 0
        // once more to be sure.
        pCreature->setPartyID(0);
    }

    // cout << "DeleteAllPartyInfo END" << endl;

    __END_CATCH
}
