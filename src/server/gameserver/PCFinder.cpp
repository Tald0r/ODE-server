//////////////////////////////////////////////////////////////////////////////
// Filename    : PCFinder.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PCFinder.h"

#include "Player.h"
#include "PlayerCreature.h"
// #include "GamePlayer.h"

//////////////////////////////////////////////////////////////////////////////
// class PCFinder member methods
//////////////////////////////////////////////////////////////////////////////

PCFinder::PCFinder()

{
    __BEGIN_TRY

    m_Mutex.setName("PCFinder");

    __END_CATCH
}

PCFinder::~PCFinder()

{
    __BEGIN_TRY

    m_PCs.clear();

    __END_CATCH_NO_RETHROW
}

// add creature to unordered_map
// execute just once at PC's login
void PCFinder::addCreature(Creature* pCreature)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pCreature != NULL);

    const string& Name = pCreature->getName();
    const string& ID = pCreature->getPlayer()->getID();

    unordered_map<string, Creature*>::iterator itr = m_PCs.find(Name);
    unordered_map<string, Creature*>::iterator itr2 = m_IDs.find(ID); // for BillingServer. by sigi. 2002.11.18

    if (itr != m_PCs.end() || itr2 != m_IDs.end()) {
        // m_Mutex.unlock();
        throw DuplicatedException();
    }

    m_PCs[Name] = pCreature;
    m_IDs[ID] = pCreature; // for BillingServer. by sigi. 2002.11.18

    if (pCreature->isPC()) {
        //		m_GuildMap.insert( pair<GuildID_t, Creature*>( pPC->getGuildID(), pCreature ) );
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

// Delete creature from unordered_map
// execute just once at PC's logout
void PCFinder::deleteCreature(const string& name)
// NoSuchElementException , Error)
{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, Creature*>::iterator itr = m_PCs.find(name);

    if (itr == m_PCs.end()) {
        // cerr << "PCFinder::deleteCreature() : NoSuchElementException" << endl;
        // throw NoSuchElementException();
        //  Do not throw NoSuchElementException here.

        return;
    }


    // The account entry goes with the name, if it still names this creature.
    Creature* pCreature = itr->second;
    Player* pPlayer = pCreature->getPlayer();
    Assert(pPlayer != NULL);

    const string& ID = pPlayer->getID();

    unordered_map<string, Creature*>::iterator itr2 = m_IDs.find(ID);

    if (itr2 != m_IDs.end() && itr2->second == pCreature) {
        m_IDs.erase(itr2);
    }

    m_PCs.erase(itr);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

// get creature with PC-name
Creature* PCFinder::getCreature_LOCKED(const string& name) const
// NoSuchElementException , Error)
{
    __BEGIN_TRY

    unordered_map<string, Creature*>::const_iterator itr;

    //__ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_PCs.find(name);

    if (itr == m_PCs.end()) {
        // throw NoSuchElementException();
        //  Do not throw NoSuchElementException here.
        return NULL;
    }

    //__LEAVE_CRITICAL_SECTION(m_Mutex)

    return itr->second;


    __END_CATCH
}

// get creature with PlayerID
Creature* PCFinder::getCreatureByID_LOCKED(const string& ID) const
// NoSuchElementException , Error)
{
    __BEGIN_TRY

    unordered_map<string, Creature*>::const_iterator itr;

    //__ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_IDs.find(ID);

    if (itr == m_IDs.end()) {
        // throw NoSuchElementException();
        //  Do not throw NoSuchElementException here.
        return NULL;
    }

    //__LEAVE_CRITICAL_SECTION(m_Mutex)

    return itr->second;


    __END_CATCH
}


// get creature with PC-name
Creature* PCFinder::getCreature(const string& name) const
// NoSuchElementException , Error)
{
    __BEGIN_TRY

    unordered_map<string, Creature*>::const_iterator itr;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_PCs.find(name);

    if (itr == m_PCs.end()) {
        // cerr << "PCFinder::getCreature() : NoSuchElementException" << endl;
        // cerr << "PCFinder::getCreature() : NoSuchCreature" << endl;

        // throw NoSuchElementException();
        //  Do not throw NoSuchElementException here.
        return NULL;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return itr->second;


    __END_CATCH
}

// get creature with PlayerID
Creature* PCFinder::getCreatureByID(const string& ID) const
// NoSuchElementException , Error)
{
    __BEGIN_TRY

    unordered_map<string, Creature*>::const_iterator itr;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_IDs.find(ID);

    if (itr == m_IDs.end()) {
        // cerr << "PCFinder::getCreature() : NoSuchElementException" << endl;
        // cerr << "PCFinder::getCreature() : NoSuchCreature" << endl;

        // throw NoSuchElementException();
        //  Do not throw NoSuchElementException here.
        return NULL;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return itr->second;


    __END_CATCH
}

// get creature's IP address
IP_t PCFinder::getIP(const string& name) const {
    __BEGIN_TRY

    IP_t IP = 0;

    unordered_map<string, Creature*>::const_iterator itr;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_PCs.find(name);

    if (itr == m_PCs.end()) {
        cerr << "PCFinder::getCreature() : NoSuchElementException" << endl;
        // m_Mutex.unlock();
        throw NoSuchElementException();
    }

    Creature* pCreature = itr->second;
    Assert(pCreature->isPC());

    Player* pPlayer = pCreature->getPlayer();
    Assert(pPlayer != NULL);

    Socket* pSocket = pPlayer->getSocket();
    Assert(pSocket != NULL);

    IP = pSocket->getHostIP();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return IP;

    __END_CATCH
}

list<string> PCFinder::getGuildPlayerNames_LOCKED(GuildID_t gID) const {
    list<string> names;

    for (unordered_map<string, Creature*>::const_iterator itr = m_PCs.begin(); itr != m_PCs.end(); ++itr) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(itr->second);
        if (pPC != NULL && pPC->getGuildID() == gID)
            names.push_back(itr->first);
    }

    return names;
}

void PCFinder::addNPC(NPC* pNPC) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pNPC != NULL);

    const string& Name = pNPC->getName();

    unordered_map<string, NPC*>::iterator itr = m_NPCs.find(Name);

    if (itr != m_NPCs.end()) {
        return;
        // throw DuplicatedException();
    }

    m_NPCs[Name] = pNPC;

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


void PCFinder::deleteNPC(const string& name)

{
    // Probably never used; it only exists to pair with the add function.
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<string, NPC*>::iterator itr = m_NPCs.find(name);

    if (itr == m_NPCs.end()) {
        return;
    }

    m_NPCs.erase(itr);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


NPC* PCFinder::getNPC(const string& name) const

{
    __BEGIN_TRY

    unordered_map<string, NPC*>::const_iterator itr;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    itr = m_NPCs.find(name);

    if (itr == m_NPCs.end()) {
        return NULL;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return itr->second;


    __END_CATCH
}


NPC* PCFinder::getNPC_LOCKED(const string& name) const

{
    __BEGIN_TRY

    unordered_map<string, NPC*>::const_iterator itr;

    itr = m_NPCs.find(name);

    if (itr == m_NPCs.end()) {
        return NULL;
    }

    return itr->second;

    __END_CATCH
}
