//////////////////////////////////////////////////////////////////////////////
// Filename    : IceField.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "IceField.h"

#include "Creature.h"
#include "EffectIceField.h"
#include "GCAddEffect.h"
#include "GCSkillFailed1.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GCSkillToTileOK6.h"
#include "GameContext.h"
#include "RankBonus.h"

IceField::IceField() {
    m_IceFieldMask[0][0].set(0, 0);
    m_MaskNum[0] = 1;

    m_IceFieldMask[1][0].set(-1, 0);
    m_IceFieldMask[1][1].set(0, 0);
    m_IceFieldMask[1][2].set(1, 0);
    m_MaskNum[1] = 3;

    m_IceFieldMask[2][0].set(-1, 0);
    m_IceFieldMask[2][1].set(1, 0);
    m_IceFieldMask[2][2].set(0, 0);
    m_IceFieldMask[2][3].set(0, 1);
    m_IceFieldMask[2][4].set(0, -1);
    m_MaskNum[2] = 5;

    m_IceFieldMask[3][0].set(-1, -1);
    m_IceFieldMask[3][1].set(-1, 0);
    m_IceFieldMask[3][2].set(-1, 1);
    m_IceFieldMask[3][3].set(0, -1);
    m_IceFieldMask[3][4].set(0, 0);
    m_IceFieldMask[3][5].set(0, 1);
    m_IceFieldMask[3][6].set(1, -1);
    m_IceFieldMask[3][7].set(1, 0);
    m_IceFieldMask[3][8].set(1, 1);
    m_MaskNum[3] = 9;
};

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void IceField::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Zone* pZone = pOusters->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pOusters, pTargetCreature)) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        execute(pOusters, pTargetCreature->getX(), pTargetCreature->getY(), pOustersSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void IceField::execute(Ousters* pOusters, ZoneCoord_t X, ZoneCoord_t Y, OustersSkillSlot* pOustersSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pOustersSkillSlot->getSkillType());
            return;
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;
        GCSkillToTileOK6 _GCSkillToTileOK6;

        SkillType_t SkillType = pOustersSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t myX = pOusters->getX();
        ZoneCoord_t myY = pOusters->getY();

        // Computes the effect duration.
        SkillInput input(pOusters, pOustersSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        int RequiredMP = (int)pSkillInfo->getConsumeMP() + pOustersSkillSlot->getExpLevel();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, X, Y, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        bool bTileCheck = false;
        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (rect.ptInRect(X, Y))
            bTileCheck = true;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToTileOK1);

            int grade = 0;

            if (input.SkillLevel <= 10)
                grade = 0;
            else if (input.SkillLevel <= 20)
                grade = 1;
            else if (input.SkillLevel < 30)
                grade = 2;
            else
                grade = 3;

            list<Creature*> cList; // denier list

            for (int i = 0; i < m_MaskNum[grade]; i++) {
                POINT& pt = m_IceFieldMask[grade][i];

                int tileX = X + pt.x;
                int tileY = Y + pt.y;

                if (rect.ptInRect(tileX, tileY)) {
                    Tile& tile = pZone->getTile(tileX, tileY);
                    if (tile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION))
                        continue;

                    // If an effect can be added to this tile...
                    if (tile.canAddEffect()) {
                        // Delete the same effect if one is already there.
                        Effect* pOldEffect = tile.getEffect(Effect::EFFECT_CLASS_ICE_FIELD);
                        if (pOldEffect != NULL) {
                            ObjectID_t effectID = pOldEffect->getObjectID();
                            pZone->deleteEffect(effectID); // fix me
                        }

                        // Create the effect class.
                        EffectIceField* pEffect = new EffectIceField(pZone, tileX, tileY);
                        pEffect->setCasterName(pOusters->getName());
                        pEffect->setCasterID(pOusters->getObjectID());
                        pEffect->setDeadline(output.Duration);
                        pEffect->setDuration(output.Range);
                        pEffect->setNextTime(0);
                        pEffect->setTick(output.Tick);

                        // An Effect attached to a Tile must have its ObjectID registered.
                        ObjectRegistry& objectregister = pZone->getObjectRegistry();
                        objectregister.registerObject(pEffect);
                        pZone->addEffect(pEffect);
                        tile.addEffect(pEffect);

                        const forward_list<Object*>& oList = tile.getObjectList();
                        for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                            Object* pTarget = *itr;
                            Creature* pTargetCreature = NULL;
                            if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE &&
                                ((pTargetCreature = dynamic_cast<Creature*>(pTarget))->isSlayer() ||
                                 pTargetCreature->isVampire()) &&
                                !checkZoneLevelToHitTarget(pTargetCreature)) {
                                cList.push_back(pTargetCreature);
                                _GCSkillToTileOK2.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK4.addCListElement(pTargetCreature->getObjectID());
                                _GCSkillToTileOK5.addCListElement(pTargetCreature->getObjectID());

                                pEffect->affect(pTargetCreature);
                            }
                        }
                    }
                }
            }

            _GCSkillToTileOK1.setSkillType(SkillType);
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setDuration(output.Duration);
            _GCSkillToTileOK1.setRange(grade);

            _GCSkillToTileOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK2.setSkillType(SkillType);
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setDuration(output.Duration);
            _GCSkillToTileOK2.setRange(grade);

            _GCSkillToTileOK3.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK3.setSkillType(SkillType);
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(SkillType);
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setRange(grade);
            _GCSkillToTileOK4.setDuration(output.Duration);

            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.setSkillType(SkillType);
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(grade);
            _GCSkillToTileOK5.setDuration(output.Duration);

            _GCSkillToTileOK6.setOrgXY(myX, myY);
            _GCSkillToTileOK6.setSkillType(SkillType);
            _GCSkillToTileOK6.setX(X);
            _GCSkillToTileOK6.setY(Y);
            _GCSkillToTileOK6.setDuration(output.Duration);
            _GCSkillToTileOK6.setRange(grade);

            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                if (canSee(pTargetCreature, pOusters))
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK2);
                else
                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToTileOK6);
            }

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            cList.push_back(pOusters);

            list<Creature*> watcherList = pZone->getWatcherList(myX, myY, pOusters);

            // Watchers outside cList that cannot see the caster are sent OK4 and added to cList.
            // are sent OK4 and added to cList.
            for (list<Creature*>::const_iterator itr = watcherList.begin(); itr != watcherList.end(); itr++) {
                bool bBelong = false;
                for (list<Creature*>::const_iterator tItr = cList.begin(); tItr != cList.end(); tItr++)
                    if (*itr == *tItr)
                        bBelong = true;

                Creature* pWatcher = (*itr);
                if (bBelong == false && canSee(pWatcher, pOusters) == false) {
                    if (!pWatcher->isPC()) {
                        continue;
                    }
                    pWatcher->getPlayer()->sendPacket(&_GCSkillToTileOK4);
                    cList.push_back(*itr);
                }
            }

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList, false);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);

            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

IceField g_IceField;
