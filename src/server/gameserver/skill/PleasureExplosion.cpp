//////////////////////////////////////////////////////////////////////////////
// Filename    : PleasureExplosion.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PleasureExplosion.h"

#include "Creature.h"
#include "CreatureUtil.h"
#include "GCAddEffectToTile.h"
#include "GCModifyInformation.h"
#include "GCSkillToSelfOK2.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Tile.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
PleasureExplosion::PleasureExplosion() {}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void PleasureExplosion::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        ZoneCoord_t x = pMonster->getX();
        ZoneCoord_t y = pMonster->getY();

        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        bool bRangeCheck = checkZoneLevelToUseSkill(pMonster);

        if (bRangeCheck) {
            for (int i = -2; i <= 2; ++i)
                for (int j = -2; j <= 2; ++j) {
                    ZoneCoord_t tx = x + i;
                    ZoneCoord_t ty = y + j;
                    if (!isValidZoneCoord(pZone, tx, ty))
                        continue;
                    Tile& rTile = pZone->getTile(tx, ty);
                    forward_list<Object*>& oList = rTile.getObjectList();
                    forward_list<Object*>::iterator itr = oList.begin();

                    for (; itr != oList.end(); ++itr) {
                        if ((*itr)->getObjectClass() != Object::OBJECT_CLASS_CREATURE)
                            continue;
                        Creature* pCreature = dynamic_cast<Creature*>(*itr);
                        if (pCreature == NULL || !pCreature->isPC())
                            continue;
                        addSimpleCreatureEffect(pCreature, Effect::EFFECT_CLASS_PLEASURE_EXPLOSION, 300);
                        cout << pCreature->getName() << ", please" << endl;

                        if (pCreature->isSlayer()) {
                            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                            pSlayer->setMP(0);
                            GCModifyInformation gcMI;
                            gcMI.addLongData(MODIFY_CURRENT_MP, 0);
                            pSlayer->getPlayer()->sendPacket(&gcMI);
                        } else if (pCreature->isOusters()) {
                            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                            pOusters->setMP(0);
                            GCModifyInformation gcMI;
                            gcMI.addLongData(MODIFY_CURRENT_MP, 0);
                            pOusters->getPlayer()->sendPacket(&gcMI);
                        }
                    }
                }

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2);

            GCAddEffectToTile gcAE;
            gcAE.setXY(x, y);
            gcAE.setEffectID(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION);
            gcAE.setObjectID(0);
            gcAE.setDuration(0);
            pZone->broadcastPacket(x, y, &gcAE);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

PleasureExplosion g_PleasureExplosion;
