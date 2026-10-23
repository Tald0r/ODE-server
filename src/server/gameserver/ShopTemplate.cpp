////////////////////////////////////////////////////////////////////////////////
// Filename    : ShopTemplate.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ShopTemplate.h"

#include "repository/GameInfoRepository.h"

////////////////////////////////////////////////////////////////////////////////
// class ShopTemplate member methods
////////////////////////////////////////////////////////////////////////////////

ShopTemplate::ShopTemplate()

{
    m_ID = 0;
    m_RackType = 0;
    m_ItemClass = 0;
    m_MinItemType = 0;
    m_MaxItemType = 0;
    m_MinOptionLevel = 0;
    m_MaxOptionLevel = 0;
}

ShopTemplate::~ShopTemplate()

    {__BEGIN_TRY __END_CATCH_NO_RETHROW}

string ShopTemplate::toString() const

{
    StringStream msg;
    msg << "ShopTemplate(" << "TemplateID : " << (int)m_ID << ",RackType : " << (int)m_RackType
        << ",ItemClass : " << (int)m_ItemClass << ",MinItemType : " << (int)m_MinItemType
        << ",MaxItemType : " << (int)m_MaxItemType << ",MinOptionLevel : " << (int)m_MinOptionLevel
        << ",MaxOptionLevel : " << (int)m_MaxOptionLevel << ")";
    return msg.toString();
}


////////////////////////////////////////////////////////////////////////////////
// class ShopTemplateManager member methods
////////////////////////////////////////////////////////////////////////////////

ShopTemplateManager::ShopTemplateManager()

{}

ShopTemplateManager::~ShopTemplateManager()

{
    __BEGIN_TRY

    unordered_map<ShopTemplateID_t, ShopTemplate*>::iterator itr = m_Entries.begin();
    for (; itr != m_Entries.end(); itr++) {
        ShopTemplate* pTemplate = itr->second;
        SAFE_DELETE(pTemplate);
    }

    m_Entries.clear();

    __END_CATCH_NO_RETHROW
}

void ShopTemplateManager::init()

{
    load();
}

void ShopTemplateManager::load()

{
    vector<ShopTemplateRow> rows = defaultGameInfoRepository().loadShopTemplates();

    for (size_t r = 0; r < rows.size(); r++) {
        ShopTemplate* pTemplate = new ShopTemplate();

        pTemplate->setID(rows[r].id);
        pTemplate->setShopType(rows[r].shopType);
        pTemplate->setItemClass(rows[r].itemClass);
        pTemplate->setMinItemType(rows[r].minItemType);
        pTemplate->setMaxItemType(rows[r].maxItemType);
        pTemplate->setMinOptionLevel(rows[r].minOptionLevel);
        pTemplate->setMaxOptionLevel(rows[r].maxOptionLevel);

        setTemplate(pTemplate->getID(), pTemplate);
    }
}

ShopTemplate* ShopTemplateManager::getTemplate(ShopTemplateID_t id) const {
    unordered_map<ShopTemplateID_t, ShopTemplate*>::const_iterator itr = m_Entries.find(id);

    if (itr == m_Entries.end()) {
        cerr << "ShopTemplateManager::getTemplate() : NoSuchElementException" << endl;
        throw NoSuchElementException("template not exist.");
    }

    return itr->second;
}

void ShopTemplateManager::setTemplate(ShopTemplateID_t id, ShopTemplate* pTemplate)

{
    unordered_map<ShopTemplateID_t, ShopTemplate*>::iterator itr = m_Entries.find(id);

    if (itr != m_Entries.end())
        throw Error("ShopTemplateManager::setTemplate() : Same ID already exist!");

    m_Entries[id] = pTemplate;
}

string ShopTemplateManager::toString() const

{
    StringStream msg;
    msg << "ShopTemplateManager(";
    unordered_map<ShopTemplateID_t, ShopTemplate*>::const_iterator itr = m_Entries.begin();
    for (; itr != m_Entries.end(); itr++)
        msg << "(ShopTemplate:" << (int)(itr->first) << "," << itr->second->toString() << ")";
    msg << ")";
    return msg.toString();
}
