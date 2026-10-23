//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectEnemyErase.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectEnemyErase.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveInjuriousCreature.h"
#include "Monster.h"
#include "Ousters.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/EffectSaveRepository.h"

EffectEnemyErase::EffectEnemyErase(Creature* pCreature)

{
    setTarget(pCreature);

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

EffectEnemyErase::~EffectEnemyErase()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void EffectEnemyErase::affect(Creature* pCreature)

{}

void EffectEnemyErase::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectEnemyErase::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    if (pCreature->isSlayer()) {
        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pCreature);
        pTargetSlayer->deleteEnemy(m_EnemyName);

        GCRemoveInjuriousCreature gcRemoveInjuriousCreature;
        gcRemoveInjuriousCreature.setName(m_EnemyName);

        pTargetSlayer->getPlayer()->sendPacket(&gcRemoveInjuriousCreature);

    } else if (pCreature->isVampire()) {
        Vampire* pTargetVampire = dynamic_cast<Vampire*>(pCreature);
        pTargetVampire->deleteEnemy(m_EnemyName);

        GCRemoveInjuriousCreature gcRemoveInjuriousCreature;
        gcRemoveInjuriousCreature.setName(m_EnemyName);

        pTargetVampire->getPlayer()->sendPacket(&gcRemoveInjuriousCreature);

    } else if (pCreature->isOusters()) {
        Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
        pTargetOusters->deleteEnemy(m_EnemyName);

        GCRemoveInjuriousCreature gcRemoveInjuriousCreature;
        gcRemoveInjuriousCreature.setName(m_EnemyName);

        pTargetOusters->getPlayer()->sendPacket(&gcRemoveInjuriousCreature);

    } else {
    }
    destroy(pCreature->getName());

    __END_DEBUG
}

void EffectEnemyErase::create(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().insertEnemyErase(ownerID, currentYearTime, m_Deadline.tv_sec, m_EnemyName);
}

void EffectEnemyErase::destroy(const string& ownerID)

{
    defaultEffectSaveRepository().deleteEnemyErase(ownerID, m_EnemyName);
}

void EffectEnemyErase::save(const string& ownerID)

{
    Turn_t currentYearTime;

    getCurrentYearTime(currentYearTime);

    defaultEffectSaveRepository().updateEnemyErase(ownerID, currentYearTime, m_Deadline.tv_sec, m_EnemyName);
}

string EffectEnemyErase::toString() const {
    StringStream msg;
    msg << "EffectEnemyErase(" << "EnemyName:" << getEnemyName() << ")";
    return msg.toString();
}

void EffectEnemyEraseLoader::load(Creature* pCreature)

{
    // Assert(pCreature != NULL);
    if (pCreature == NULL) {
        return;
    }

    vector<EnemyEraseRow> rows = defaultEffectSaveRepository().loadEnemyErases(pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        int DayTime = rows[r].dayTime;

        Timeval currentTime;
        getCurrentTime(currentTime);

        EffectEnemyErase* pEffectEnemyErase = new EffectEnemyErase(pCreature);

        EffectManager* pEffectManager = pCreature->getEffectManager();

        if (currentTime.tv_sec < DayTime) {
            pEffectEnemyErase->setDeadline((DayTime - currentTime.tv_sec) * 10);
            pEffectEnemyErase->setEnemyName(rows[r].enemyName);
        } else {
            pEffectEnemyErase->setDeadline(100);
            pEffectEnemyErase->setEnemyName(rows[r].enemyName);
        }

        pEffectManager->addEffect(pEffectEnemyErase);

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            pSlayer->addEnemy(pEffectEnemyErase->getEnemyName());
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            pVampire->addEnemy(pEffectEnemyErase->getEnemyName());
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            pOusters->addEnemy(pEffectEnemyErase->getEnemyName());
        }
    }
}
