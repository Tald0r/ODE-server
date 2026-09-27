//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectManager.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectManager.h"

#include "Assert.h"
#include "Creature.h"
#include "EffectEnemyErase.h"
#include "EffectInfo.h"
#include "GCAddEffect.h"
#include "Profile.h"
#include "Zone.h"
#include "ZoneUtil.h"

// #define __PROFILE_EM__

#ifdef __PROFILE_EM__
#define __BEGIN_PROFILE_EM(name) beginProfileEx(name);
#define __END_PROFILE_EM(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_EM(name) ((void)0);
#define __END_PROFILE_EM(name) ((void)0);
#endif


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
class isSameEffectClass {
public:
    isSameEffectClass(Effect::EffectClass EClass) : m_EClass(EClass) {}
    bool operator()(Effect* pEffect) {
        return pEffect->getEffectClass() == m_EClass;
    }

private:
    Effect::EffectClass m_EClass;
};


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectManager::EffectManager()

{
    __BEGIN_TRY

    Assert(m_Effects.empty());

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectManager::~EffectManager()

{
    __BEGIN_TRY

    // If deleteEffects() was not called properly,
    // there is no need to call unaffect() on each effect.
    // Deleting the objects is enough.
    while (!m_Effects.empty()) {
        // Delete the effect object.
        SAFE_DELETE(m_Effects.front());
        m_Effects.pop_front();
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectManager::save(const string& ownerID)

{
    __BEGIN_TRY

    list<Effect*>::const_iterator itr = m_Effects.begin();
    for (; itr != m_Effects.end(); itr++)
        (*itr)->save(ownerID);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool EffectManager::isEffect(Effect::EffectClass EClass)

{
    __BEGIN_TRY

    list<Effect*>::const_iterator itr = find_if(m_Effects.begin(), m_Effects.end(), isSameEffectClass(EClass));
    if (itr != m_Effects.end())
        return true;
    return false;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectManager::deleteEffect(ObjectID_t effectID)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    list<Effect*>::iterator current = m_Effects.begin();

    if (effectID == 0) {
        for (current = m_Effects.begin(); current != m_Effects.end(); current++) {
            Effect* pEffect = *current;

            pEffect->unaffect();
            SAFE_DELETE(pEffect);
        }

        m_Effects.clear();
    } else {
        for (current = m_Effects.begin(); current != m_Effects.end(); current++) {
            Effect* pEffect = *current;

            if (pEffect->getObjectID() == effectID) {
                m_Effects.erase(current);
                pEffect->unaffect();
                SAFE_DELETE(pEffect);
                return;
            }
        }

        // cerr << "EffectManager::deleteEffect could not find the effect."<<endl;
        filelog("EffectManagerBug.txt", "there is no effect with effect id %d", effectID);
        Assert(false);
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectManager::deleteEffect(Effect::EffectClass EClass)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    list<Effect*>::iterator itr = find_if(m_Effects.begin(), m_Effects.end(), isSameEffectClass(EClass));
    if (itr != m_Effects.end()) {
        Effect* pEffect = *itr;
        m_Effects.erase(itr);
        // pEffect->unaffect(); // Why does this not work?
        SAFE_DELETE(pEffect);
    } else {
        // cerr << "EffectManager::deleteEffect could not find the effect."<<endl;
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectManager::deleteEffect(Creature* pCreature, Effect::EffectClass EClass)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    list<Effect*>::iterator itr = find_if(m_Effects.begin(), m_Effects.end(), isSameEffectClass(EClass));
    if (itr != m_Effects.end()) {
        Effect* pEffect = *itr;

        m_Effects.erase(itr);

        if (pCreature) {
            if (pCreature->isFlag(EClass))
                pCreature->removeFlag(EClass);

            pEffect->destroy(pCreature->getName());
        }

        // pEffect->unaffect(); // This does not work.
        SAFE_DELETE(pEffect);
    } else {
        // cerr << "EffectManager::deleteEffect could not find the effect."<<endl;
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Effect* EffectManager::findEffect(Effect::EffectClass EClass) const

{
    __BEGIN_TRY

    list<Effect*>::const_iterator itr = find_if(m_Effects.begin(), m_Effects.end(), isSameEffectClass(EClass));
    if (itr == m_Effects.end()) {
        return NULL;
    }
    return *itr;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Effect* EffectManager::findEffect(ObjectID_t ObjectID) const

{
    __BEGIN_TRY

    list<Effect*>::const_iterator itr = m_Effects.begin();
    for (; itr != m_Effects.end(); itr++) {
        Effect* pEffect = *itr;
        if (pEffect->getObjectID() == ObjectID)
            return pEffect;
    }

    return NULL;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Effect* EffectManager::findEffect(Effect::EffectClass EClass, string EnemyName) const

{
    __BEGIN_TRY

    list<Effect*>::const_iterator itr = m_Effects.begin();

    for (; itr != m_Effects.end(); itr++) {
        if ((*itr)->getEffectClass() == EClass && EClass == Effect::EFFECT_CLASS_ENEMY_ERASE) {
            if (((EffectEnemyErase*)(*itr))->getEnemyName() == EnemyName)
                return *itr;
        }
    }

    return NULL;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectInfo* EffectManager::getEffectInfo()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    EffectInfo* pEffectInfo;

    __BEGIN_PROFILE_EM("EM_GETEFFECTINFO");

    __BEGIN_PROFILE_EM("EM_NEW_EFFECTINFO");
    pEffectInfo = new EffectInfo();
    __END_PROFILE_EM("EM_NEW_EFFECTINFO");

    BYTE ListNum = 0;

    Timeval currentTime;

    __BEGIN_PROFILE_EM("EM_GETTIME");
    getCurrentTime(currentTime);
    __END_PROFILE_EM("EM_GETTIME");

    // Build the effect information.
    for (list<Effect*>::const_iterator itr = m_Effects.begin(); itr != m_Effects.end(); itr++) {
        Effect* pEffect = *itr;
        EffectID_t EffectID = pEffect->getSendEffectClass();

        // Each Effect's constructor decides whether it is an effect sent to the client.
        // by sigi. 2002.11.14
        bool bSend = pEffect->isBroadcastingEffect();

        if (bSend) {
            Timeval DeadLine;
            Turn_t Duration;

            __BEGIN_PROFILE_EM("EM_COMPUTE_TIME")
            // Subtract the current time from the deadline and send the remaining time.
            DeadLine = (*itr)->getDeadline();
            Duration = DeadLine.tv_sec - currentTime.tv_sec;
            __END_PROFILE_EM("EM_COMPUTE_TIME")

            if (EffectID == Effect::EFFECT_CLASS_BLOOD_DRAIN || EffectID == Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR) {
                // Duration is sent as a WORD,
                // and two bytes are too short for a blood drain time,
                // so it would overflow.
                // Other effects are therefore sent multiplied by 10,
                // but the blood drain effect is not.
                pEffectInfo->addListElement(EffectID, Duration);
            } else {
                pEffectInfo->addListElement(EffectID, Duration * 10);
            }

            ListNum++;
        }
    }

    pEffectInfo->setListNum(ListNum);

    // int tempNum = ListNum;

    // cout << "EffectManager.cpp listNum" << tempNum << endl;

    __END_PROFILE_EM("EM_GETEFFECTINFO");

    return pEffectInfo;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//
// sendEffectInfo
//
//////////////////////////////////////////////////////////////////////////////
// Broadcasts the Effect information.
// by sigi. 2002.11.14
//////////////////////////////////////////////////////////////////////////////
void EffectManager::sendEffectInfo(Creature* pCreature, Zone* pZone, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    __BEGIN_PROFILE_EM("SEND_EFFECT_INFO");

    Assert(pCreature != NULL);
    Assert(isValidZoneCoord(pZone, x, y));

    Timeval currentTime;

    getCurrentTime(currentTime);

    // Scatter it to the surroundings.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());

    for (list<Effect*>::const_iterator itr = m_Effects.begin(); itr != m_Effects.end(); itr++) {
        Effect* pEffect = *itr;

        if (pEffect->isBroadcastingEffect()) {
            EffectID_t EffectID = pEffect->getSendEffectClass();
            gcAddEffect.setEffectID(EffectID);

            // Subtract the current time from the deadline and send the remaining time.
            Timeval DeadLine = (*itr)->getDeadline();
            Turn_t Duration = DeadLine.tv_sec - currentTime.tv_sec;

            if (EffectID == Effect::EFFECT_CLASS_BLOOD_DRAIN || EffectID == Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR) {
                // Duration is sent as a WORD,
                // and two bytes are too short for a blood drain time,
                // so it would overflow.
                // Other effects are therefore sent multiplied by 10,
                // but the blood drain effect is not.
                gcAddEffect.setDuration(Duration);
            } else {
                gcAddEffect.setDuration(Duration * 10);
            }

            pZone->broadcastPacket(x, y, &gcAddEffect);
        }
    }

    __END_PROFILE_EM("SEND_EFFECT_INFO");

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectManager::addEffect(Effect* pEffect)

{
    __BEGIN_TRY

    Assert(pEffect != NULL);
    m_Effects.push_back(pEffect);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Same as the heartbeat above,
// only the way the time is obtained differs:
// this one avoids calling getCurrentTime().
//////////////////////////////////////////////////////////////////////////////
int EffectManager::heartbeat(const Timeval& currentTime)

{
    __BEGIN_TRY
    __END_DEBUG

    int rvalue = 0;

    __BEGIN_PROFILE_EM("EM_HEARTBEAT");

    list<Effect*>::iterator before = m_Effects.end();
    list<Effect*>::iterator current = m_Effects.begin();

    while (current != m_Effects.end()) {
        Assert(*current != NULL);
        Effect* pEffect = *current;

        // by sigi. for debugging. 2002.12.23
        m_LastEffectClass = (int)pEffect->getEffectClass();

        // Delete the effect once it has expired.
        if (currentTime > pEffect->getDeadline()) {
            if (before == m_Effects.end()) {
                // first effect
                m_Effects.erase(current);
                current = m_Effects.begin();
            } else {
                // !first effect
                m_Effects.erase(current);
                current = before;
                current++;
            }

            // *CAUTION
            // EffectBloodDrain changes the Slayer's data inside unaffect().
            pEffect->unaffect();
            SAFE_DELETE(pEffect);

            rvalue++;
        } else {
            // Among the effects that have not expired,
            // affect those whose next affect time has come.
            if (currentTime > pEffect->getNextTime()) {
                pEffect->affect();
            }

            before = current++;
        }
    }

    __END_PROFILE_EM("EM_HEARTBEAT");

    return rvalue;

    __BEGIN_DEBUG
    __END_CATCH
}

void EffectManager::setTimeOutAllEffect()

{
    __BEGIN_TRY

    list<Effect*>::iterator itr = m_Effects.begin();
    list<Effect*>::iterator endItr = m_Effects.end();

    for (; itr != endItr; ++itr) {
        Assert(*itr != NULL);
        Effect* pEffect = *itr;

        pEffect->setDeadline(0);
    }

    __END_CATCH
}
