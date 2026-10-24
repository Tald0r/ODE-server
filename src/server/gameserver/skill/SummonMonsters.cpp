//////////////////////////////////////////////////////////////////////////////
// Filename    : SummonMonsters.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SummonMonsters.h"

#include "GCSay.h"
#include "GCSkillToTileOK5.h"
#include "GameContext.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "MonsterSummonInfo.h"
#include "ZoneUtil.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
SummonMonsters::SummonMonsters() {}

//////////////////////////////////////////////////////////////////////////////
// Vampire self handler
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void SummonMonsters::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);
    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);


    try {
        if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            return;
        }
        if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pMonster, true);
        }


        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);

        if (bRangeCheck) // && bMoveModeCheck)
        {
            //--------------------------------------------------------
            // Check which surrounding creatures are hit and knocked back.
            //--------------------------------------------------------

            SUMMON_INFO2 summonInfo;

            bool hasInfo = pMonster->getMonsterSummonInfo(summonInfo);

            if (!hasInfo || summonInfo.pMonsters == NULL) {
                // There is nothing to summon.
                executeSkillFailNormal(pMonster, getSkillType(), NULL);

                // The master tried to summon in the master lair.
                if (pZone->isMasterLair() && pMonster->isMaster()) {
                    MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
                    Assert(pMasterLairManager != NULL);

                    // When there is nothing left to summon,
                    // the master steps out to fight on its own.
                    pMasterLairManager->setMasterReady();
                }
            }

            if (pMonster->isMaster() && pZone->isMasterLair()) {
                MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
                Assert(pMasterLairManager != NULL);
                // Minion combat summons at the configured coordinates.

                MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(pZone->getZoneID());
                Assert(pInfo != NULL);

                if (!pMasterLairManager->isMasterReady()) {
                    x = pInfo->getSummonX();
                    y = pInfo->getSummonY();

                    GCSay gcSay;
                    gcSay.setObjectID(pMonster->getObjectID());
                    gcSay.setColor(MASTER_SAY_COLOR);
                    gcSay.setMessage(pInfo->getRandomMasterSummonSay());
                    if (!gcSay.getMessage().empty())
                        pZone->broadcastPacket(pMonster->getX(), pMonster->getY(), &gcSay);
                }

                // Monsters summoned in the master lair drop no items.
                // by sigi. 2002.11.21
                summonInfo.hasItem = false;
            }

            summonInfo.scanEnemy = true;
            summonInfo.clanType = SUMMON_INFO::CLAN_TYPE_GROUP;
            summonInfo.clanID = pMonster->getClanType(); // Follows the owner's clan.
            summonInfo.X = x;
            summonInfo.Y = y;
            summonInfo.regenType = REGENTYPE_PORTAL;

            // Add the monster to the zone.
            addMonstersToZone(pZone, summonInfo);

            GCSkillToTileOK5 _GCSkillToTileOK5;

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setX(x);
            _GCSkillToTileOK5.setY(y);
            _GCSkillToTileOK5.setDuration(0);

            pZone->broadcastPacket(x, y, &_GCSkillToTileOK5);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

SummonMonsters g_SummonMonsters;
