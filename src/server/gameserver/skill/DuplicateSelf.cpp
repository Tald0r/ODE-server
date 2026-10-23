//////////////////////////////////////////////////////////////////////////////
// Filename    : DuplicateSelf.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DuplicateSelf.h"

#include "GCFakeMove.h"
#include "GCSay.h"
#include "GCSkillToTileOK5.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "MonsterManager.h"
#include "MonsterSummonInfo.h"
#include "ZoneUtil.h"


//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
DuplicateSelf::DuplicateSelf() {
    // Hard-coded pairs; move them into a table if that becomes awkward.
    m_DuplicateMonsterTypes[432] = 435; // Bathory clone
    m_DuplicateMonsterTypes[434] = 436; // Tepes clone
}

//////////////////////////////////////////////////////////////////////////////
// Vampire self handler
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void DuplicateSelf::execute(Monster* pMonster)

{
    MonsterType_t MType = pMonster->getMonsterType();

    unordered_map<MonsterType_t, MonsterType_t>::const_iterator itr = m_DuplicateMonsterTypes.find(MType);

    // Without a clone MonsterType there is nothing to duplicate.
    if (itr == m_DuplicateMonsterTypes.end()) {
        return;
    }

    MonsterType_t DuplicateMType = itr->second;

    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

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
            GCSkillToTileOK5 _GCSkillToTileOK5;

            _GCSkillToTileOK5.setObjectID(pMonster->getObjectID());
            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setX(x);
            _GCSkillToTileOK5.setY(y);
            _GCSkillToTileOK5.setDuration(0);

            pZone->broadcastPacket(x, y, &_GCSkillToTileOK5);

            //--------------------------------------------------------
            // Check which surrounding creatures are hit and knocked back.
            //--------------------------------------------------------
            // Add the monster to the zone.
            SUMMON_INFO summonInfo;
            summonInfo.scanEnemy = true;
            summonInfo.hasItem = false;
            summonInfo.initHPPercent = pMonster->getHP(ATTR_CURRENT) * 100 / pMonster->getHP(ATTR_MAX);

            int numFake = min((1 + rand() % 3), pMonster->getINT() / 100);

            MonsterManager* pMonsterManager = pZone->getMonsterManager();
            Assert(pMonsterManager != NULL);

            list<Monster*> summonedMonsters;

            for (int i = 0; i < numFake; i++) {
                int X = max(0, min((int)pZone->getWidth() - 1, (x - 5 + rand() % 11)));
                int Y = max(0, min((int)pZone->getHeight() - 1, (y - 5 + rand() % 11)));

                try {
                    pMonsterManager->addMonsters(X, Y, DuplicateMType, 1, summonInfo, &summonedMonsters);
                } catch (Throwable& t) {
                    cerr << t.toString() << endl;
                }
            }

            // Show the afterimages.
            list<Monster*>::const_iterator iMonster = summonedMonsters.begin();

            for (; iMonster != summonedMonsters.end(); iMonster++) {
                Monster* pFakeMonster = *iMonster;

                GCFakeMove gcFakeMove;
                gcFakeMove.setObjectID(pMonster->getObjectID());
                gcFakeMove.setXY(pFakeMonster->getX(), pFakeMonster->getY());

                pZone->broadcastPacket(x, y, &gcFakeMove);
            }

            // Move the monster itself somewhere as well.
            // Up to 50 attempts.
            for (int i = 0; i < 50; i++) {
                int X = max(0, min((int)pZone->getWidth() - 1, (x - 8 + rand() % 11)));
                int Y = max(0, min((int)pZone->getHeight() - 1, (y - 8 + rand() % 11)));

                if (pZone->moveFastMonster(pMonster, x, y, X, Y, getSkillType())) {
                    break;
                }
            }
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

DuplicateSelf g_DuplicateSelf;
