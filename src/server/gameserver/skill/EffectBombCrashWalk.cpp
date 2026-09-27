//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBombCrashWalk.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBombCrashWalk.h"

#include "EffectBombCrashWalkToEnemy.h"
#include "GCModifyInformation.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillInfo.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"


int BombCrashWalkDamageModify[5][5] = {
    {50, 50, 50, 50, 50}, {50, 75, 75, 75, 50}, {50, 75, 100, 75, 50}, {50, 75, 75, 75, 50}, {50, 50, 50, 50, 50}};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBombCrashWalk::EffectBombCrashWalk(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    __BEGIN_TRY

    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_Damage = 0;
    m_UserObjectID = 0;
    m_StormTime = 0;
    m_Tick = 0;
    m_SkillType = SKILL_MAX;
    m_bLarge = true;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalk::affect()

{
    __BEGIN_TRY

    Assert(m_pZone != NULL);

    // Get the skill user.
    // !! It can be NULL because the caster may already have left the zone.
    // by bezz. 2003.1.4
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);
    // Return if the caster is gone.
    if (pCastCreature == NULL)
        return;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCastCreature);

    SkillSlot* pSkillSlot = pSlayer->hasSkill(m_SkillType);
    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(m_SkillType);
    SkillDomainType_t DomainType = pSkillInfo->getDomainType();
    SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

    int iMode = 0;
    if (SkillLevel < 33) {
        iMode = 0;
    } else if (SkillLevel < 66) {
        iMode = 1;
    } else if (SkillLevel <= 100) {
        iMode = 2;
    }


    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);
    if (!rect.ptInRect(m_X, m_Y))
        return;

    GCSkillToObjectOK1 _GCSkillToObjectOK1;
    GCSkillToObjectOK2 _GCSkillToObjectOK2;
    GCSkillToObjectOK4 _GCSkillToObjectOK4;

    bool bHit = false;
    Damage_t maxDamage = 0;

    int diff = 1;
    if (m_bLarge)
        diff = 2;

    // Collect the tiles the effect covers.
    // The center tile plus the splash tiles.
    for (int oX = -diff; oX <= diff; oX++)
        for (int oY = -diff; oY <= diff; oY++) {
            int tileX = m_X + oX;
            int tileY = m_Y + oY;
            if (!rect.ptInRect(tileX, tileY))
                continue;

            Tile& tile = m_pZone->getTile(tileX, tileY);

            const forward_list<Object*>& oList = tile.getObjectList();
            forward_list<Object*>::const_iterator itr = oList.begin();
            for (; itr != oList.end(); itr++) {
                Object* pObject = *itr;
                Assert(pObject != NULL);

                if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    Creature* pTargetCreature = dynamic_cast<Creature*>(pObject);
                    Assert(pTargetCreature != NULL);

                    if (pTargetCreature->getObjectID() == m_UserObjectID ||
                        !canAttack(pCastCreature, pTargetCreature) ||
                        pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                        continue;
                    }

                    bool bPK = verifyPK(pSlayer, pTargetCreature);
                    bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);
                    bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, SkillLevel);


                    if (bPK && bZoneLevelCheck && bHitRoll) {
                        // Compute the final damage from the base damage plus the skill bonus.
                        bool bCriticalHit = false;
                        Damage_t FinalDamage = 0;
                        FinalDamage += computeDamage(pSlayer, pTargetCreature, SkillLevel / 2, bCriticalHit);
                        FinalDamage += m_Damage;

                        // Scale the damage by the tile's place in the blast pattern.
                        int DamageModifier = BombCrashWalkDamageModify[oX + 2][oY + 2];
                        Damage_t TileDamage = getPercentValue(FinalDamage, DamageModifier);

                        if (pTargetCreature != NULL && !pTargetCreature->isSlayer()) {
                            // Deal the damage.
                            if (pTargetCreature->isPC()) {
                                GCModifyInformation gcMI;
                                ::setDamage(pTargetCreature, TileDamage, pSlayer, m_SkillType, &gcMI);
                                pTargetCreature->getPlayer()->sendPacket(&gcMI);

                            } else if (pTargetCreature->isMonster()) {
                                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);

                                ::setDamage(pMonster, TileDamage, pSlayer, m_SkillType);

                                pMonster->addEnemy(pSlayer);
                            }
                            // Add the blast's stun effect.
                            // Roll the chance of being stunned.
                            bool bAttackCheck = false;
                            int rn = Random(10, 100);
                            if (rn > 70 && iMode == 0)
                                bAttackCheck = true;
                            if (rn > 45 && iMode == 1)
                                bAttackCheck = true;
                            if (rn > 25 && iMode == 2)
                                bAttackCheck = true;
                            if (bAttackCheck) {
                                if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_SKILL_BOMB_CRASH_WALK_TO_ENEMY))
                                    continue;

                                EffectBombCrashWalkToEnemy* pEffect = new EffectBombCrashWalkToEnemy(pTargetCreature);
                                Assert(pEffect != NULL);
                                Turn_t delay = 0;
                                switch (iMode) {
                                case 0:
                                    delay = 500;
                                    break;
                                case 1:
                                    delay = 650;
                                    break;
                                case 2:
                                    delay = 800;
                                    break;
                                }
                                pEffect->setDeadline(delay);
                                pEffect->setTick(30);          // output.Tick);
                                pEffect->setPoint(delay / 10); // output.Damage);
                                pEffect->setUserObjectID(pSlayer->getObjectID());
                                pEffect->setNextTime(0);


                                pTargetCreature->addEffect(pEffect);
                                pTargetCreature->setFlag(Effect::EFFECT_CLASS_SKILL_BOMB_CRASH_WALK_TO_ENEMY);

                                // end by Coffee 2007-3-6

                                if (TileDamage > maxDamage)
                                    maxDamage = TileDamage;
                                // Notify the skill user.
                                Player* pPlayer = pSlayer->getPlayer();
                                pPlayer->sendPacket(&_GCSkillToObjectOK1);
                                _GCSkillToObjectOK1.setSkillType(m_SkillType);
                                _GCSkillToObjectOK1.setCEffectID(Effect::EFFECT_CLASS_COMA);
                                _GCSkillToObjectOK1.setTargetObjectID(pTargetCreature->getObjectID());
                                _GCSkillToObjectOK1.setDuration(delay);

                                if (pTargetCreature->isPC()) {
                                    // Notify the target.
                                    _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
                                    _GCSkillToObjectOK2.setSkillType(m_SkillType);
                                    _GCSkillToObjectOK2.setDuration(delay);

                                    pTargetCreature->getPlayer()->sendPacket(&_GCSkillToObjectOK2);
                                }
                                // Broadcast the message to everyone nearby.
                                _GCSkillToObjectOK4.setTargetObjectID(pTargetCreature->getObjectID());
                                _GCSkillToObjectOK4.setSkillType(m_SkillType);
                                _GCSkillToObjectOK4.setDuration(delay);
                                m_pZone->broadcastPacket(tileX, tileY, &_GCSkillToObjectOK4, pTargetCreature);

                                bHit = true;
                            }
                        }
                    }
                }
            }
        }

    if (bHit) {
        GCModifyInformation gcMI;
        increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, gcMI);

        pSlayer->getPlayer()->sendPacket(&gcMI);
    }

    // Count down the storm time.
    m_StormTime--;
    if (m_StormTime <= 0)
        setDeadline(0);
    else
        setNextTime(m_Tick);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalk::affect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalk::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalk::unaffect()

{
    __BEGIN_TRY

    Tile& EffectedTile = m_pZone->getTile(m_X, m_Y);
    EffectedTile.deleteEffect(m_ObjectID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBombCrashWalk::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "EffectBombCrashWalk(" << "ObjectID:" << (int)getObjectID() << ",Zone:" << (long)m_pZone << ",X:" << (int)m_X
        << ",Y:" << (int)m_Y << ",Damage:" << (int)m_Damage << ",UserOBjectID:" << (int)m_UserObjectID
        << ",StormTime:" << (int)m_StormTime << ",Tick:" << (int)m_Tick << ",SkillType:" << (int)m_SkillType << ")";

    return msg.toString();

    __END_CATCH
}
