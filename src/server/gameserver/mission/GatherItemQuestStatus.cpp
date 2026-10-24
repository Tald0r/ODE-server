
#include "GatherItemQuestStatus.h"

#include <map>

#include "Inventory.h"
#include "PlayerCreature.h"
#include "QuestInfoManager.h"

GCQuestStatus* GatherItemQuestStatus::makeStatusPacket() const {
    GCQuestStatus* pPacket = new GCQuestStatus();

    pPacket->setQuestID((WORD)getQuestID());
    pPacket->setCurrentNum((WORD)m_CurrentNum);
    pPacket->setRemainTime((DWORD)max(VSDateTime::currentDateTime().secsTo(m_Deadline), 0));

    return pPacket;
}

bool GatherItemQuestStatus::isTarget(Item* pItem) const {
    Assert(pItem != NULL);

    return (pItem->getItemClass() == m_TargetItemClass &&
            (m_TargetItemType == 999 || pItem->getItemType() == m_TargetItemType));
}

bool GatherItemQuestStatus::isSuccess() const {
    PlayerCreature* pOwnerPC = getOwnerPC();
    Inventory* pInventory = pOwnerPC->getInventory();

    // Search the inventory
    map<ObjectID_t, Item*> ItemList;
    int height = pInventory->getHeight();
    int width = pInventory->getWidth();

    m_CurrentNum = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            Item* pItem = pInventory->getItem(i, j);
            if (pItem != NULL) {
                // Find the current item in the list of checked items.
                map<ObjectID_t, Item*>::iterator itr = ItemList.find(pItem->getObjectID());

                if (itr == ItemList.end()) {
                    i += pItem->getVolumeWidth() - 1;
                    if (isTarget(pItem)) {
                        ++m_CurrentNum;
                        if (m_CurrentNum >= m_TargetNum)
                            break;
                    }
                }
                ItemList[pItem->getObjectID()] = pItem;
            }
        }
        if (m_CurrentNum >= m_TargetNum)
            break;
    }

    if (m_TargetNum <= m_CurrentNum) {
        if (!QuestStatus::isSuccess() && !isRewarded())
            setSuccess();
        return true;
    }

    return false;
}
