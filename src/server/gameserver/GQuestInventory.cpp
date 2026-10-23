#include "GQuestInventory.h"

#include "repository/QuestItemRepository.h"

void GQuestInventory::load(const string& ownerName) {
    vector<int> itemTypes = defaultQuestItemRepository().loadItemTypes(ownerName);
    for (size_t i = 0; i < itemTypes.size(); i++) {
        addOne(itemTypes[i]);
    }
}

void GQuestInventory::removeOne(const string& ownerName, ItemType_t item) {
    defaultQuestItemRepository().removeOne(ownerName, item);
}

void GQuestInventory::addOne(ItemType_t item) {
    m_Inventory.addItem(item);
}

void GQuestInventory::saveOne(const string& ownerName, ItemType_t item) {
    addOne(item);

    defaultQuestItemRepository().insert(ownerName, item);
}
