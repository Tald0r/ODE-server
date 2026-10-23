#include "LuaTradeEventVampireItem.h"

LuaTradeEventVampireItem::LuaTradeEventVampireItem(LuaState* pState) : LuaSelectItem(pState) {
    m_Level = 0;
}

void LuaTradeEventVampireItem::prepare()

{
    lua_pushnumber(m_pState->getState(), m_Level);
    lua_setglobal(m_pState->getState(), "Level");
}
