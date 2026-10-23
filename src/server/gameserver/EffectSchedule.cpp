//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSchedule.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSchedule.h"

#include "Assert.h"
#include "Effect.h"
#include "EffectManager.h"
#include "GCAddVampirePortal.h"
#include "ObjectRegistry.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "skill/EffectVampirePortal.h"


//////////////////////////////////////////////////////////////////////////////
// class EffectScheduleWork member methods
//////////////////////////////////////////////////////////////////////////////

EffectScheduleWork::EffectScheduleWork()

{
    m_Code = WORKCODE_MAX;
}

EffectScheduleWork::~EffectScheduleWork()

{
    __BEGIN_TRY

    // SAFE_DELETE(m_pData);

    // Depending on the code, void* m_pData must be cast
    // and deleted appropriately.
    if (m_Code == WORKCODE_ADD_VAMPIRE_PORTAL) {
        Assert(m_pData == NULL);
    } else if (m_Code == WORKCODE_DELETE) {
        Assert(m_pData == NULL);
    } else {
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// class EffectSchedule member methods
//////////////////////////////////////////////////////////////////////////////

EffectSchedule::EffectSchedule()

{
    m_pEffect = NULL;
}

EffectSchedule::~EffectSchedule()

{
    __BEGIN_TRY

    while (!m_WorkList.empty()) {
        EffectScheduleWork* pWork = getFrontWork();
        SAFE_DELETE(pWork);
    }

    __END_CATCH_NO_RETHROW
}

void EffectSchedule::addWork(int WorkCode, void* pData)

{
    Assert(WorkCode < WORKCODE_MAX);

    EffectScheduleWork* pWork = new EffectScheduleWork();
    pWork->setCode(WorkCode);
    pWork->setData(pData);
    m_WorkList.push_back(pWork);
}

EffectScheduleWork* EffectSchedule::getFrontWork(void)

{
    if (!m_WorkList.empty()) {
        EffectScheduleWork* pWork = m_WorkList.front();
        m_WorkList.pop_front();
        return pWork;
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
// class EffectScheduleManager member methods
//////////////////////////////////////////////////////////////////////////////

EffectScheduleManager::EffectScheduleManager()

{}

EffectScheduleManager::~EffectScheduleManager()

{
    __BEGIN_TRY

    while (!m_EffectScheduleList.empty()) {
        EffectSchedule* pSchedule = m_EffectScheduleList.front();
        m_EffectScheduleList.pop_front();
        SAFE_DELETE(pSchedule);
    }

    __END_CATCH_NO_RETHROW
}

void EffectScheduleManager::addEffectSchedule(EffectSchedule* pEffectSchedule)

{
    Assert(pEffectSchedule != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_EffectScheduleList.push_back(pEffectSchedule);

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void EffectScheduleManager::heartbeat(void)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    // Until every schedule has been handled...
    while (!m_EffectScheduleList.empty()) {
        EffectSchedule* pEffectSchedule = m_EffectScheduleList.front();
        Assert(pEffectSchedule != NULL);

        Effect* pEffect = pEffectSchedule->getEffect();

        // If the effect the work targets still exists, start the work.
        if (pEffect != NULL) {
            // Until every job has been handled...
            while (true) {
                EffectScheduleWork* pWork = pEffectSchedule->getFrontWork();

                // Break out once every job is done.
                if (pWork == NULL)
                    break;

                int WorkCode = pWork->getCode();
                // void* pData    = pWork->getData();

                // Handle the work according to each work code.
                if (WorkCode == WORKCODE_ADD_VAMPIRE_PORTAL) {
                    Assert(pEffect->getEffectClass() == Effect::EFFECT_CLASS_VAMPIRE_PORTAL);

                    EffectVampirePortal* pEffectVampirePortal = dynamic_cast<EffectVampirePortal*>(pEffect);
                    Assert(pEffectVampirePortal != NULL);

                    // The coordinates inside the effect are the ones first aimed at.
                    // They may change, for instance if that tile already holds the same effect.
                    Zone* pZone = pEffectVampirePortal->getZone();
                    ZoneCoord_t cx = pEffectVampirePortal->getX();
                    ZoneCoord_t cy = pEffectVampirePortal->getY();
                    ZONE_COORD ZoneCoord = pEffectVampirePortal->getZoneCoord();

                    TPOINT pt = findSuitablePositionForEffect(pZone, cx, cy, Effect::EFFECT_CLASS_VAMPIRE_PORTAL);

                    if (pt.x != -1) {
                        // The coordinates may have been changed, so update them.
                        pEffectVampirePortal->setX(pt.x);
                        pEffectVampirePortal->setY(pt.y);

                        // An OID must be registered before find and similar work can run.
                        ObjectRegistry& OR = pZone->getObjectRegistry();
                        OR.registerObject(pEffectVampirePortal);

                        // Register it with the tile and the vampire portal manager.
                        Tile& rTile = pZone->getTile(pt.x, pt.y);
                        rTile.addEffect(pEffectVampirePortal);

                        EffectManager* pVampirePortalManager = pZone->getVampirePortalManager();
                        pVampirePortalManager->addEffect(pEffectVampirePortal);

                        // The effect is attached to the tile, so broadcast it.
                        GCAddVampirePortal gcAddVampirePortal;
                        gcAddVampirePortal.setObjectID(pEffectVampirePortal->getObjectID());
                        gcAddVampirePortal.setOwnerID(pEffectVampirePortal->getOwnerID());
                        gcAddVampirePortal.setX(pt.x);
                        gcAddVampirePortal.setY(pt.y);
                        gcAddVampirePortal.setTargetZoneID(ZoneCoord.id);
                        gcAddVampirePortal.setTargetX(ZoneCoord.x);
                        gcAddVampirePortal.setTargetY(ZoneCoord.y);
                        gcAddVampirePortal.setDuration(pEffectVampirePortal->getRemainDuration());
                        gcAddVampirePortal.setCreateFlag(1);

                        pZone->broadcastPacket(pt.x, pt.y, &gcAddVampirePortal);
                    } else {
                        SAFE_DELETE(pEffectVampirePortal);
                    }
                } else if (WorkCode == WORKCODE_DELETE) {
                    pEffect->setDeadline(0);
                } else {
                    Assert(false);
                }
            }
        }

        m_EffectScheduleList.pop_front();
        SAFE_DELETE(pEffectSchedule);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}
