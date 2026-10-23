//////////////////////////////////////////////////////////////////////////////
// Filename    : EventManager.cpp
// Written by  : Reiot
//////////////////////////////////////////////////////////////////////////////

#include "EventManager.h"

#include "Assert.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
EventManager::EventManager()

{
    Assert(m_Events.empty());
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
EventManager::~EventManager()

{
    __BEGIN_TRY

    while (!m_Events.empty()) {
        SAFE_DELETE(m_Events.front());
        m_Events.pop_front();
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// add effect and affect it to its target
// Register in the priority_queue and affect it.
//////////////////////////////////////////////////////////////////////////////
void EventManager::addEvent(Event* pEvent)

{
    __BEGIN_DEBUG

    Assert(pEvent != NULL);

    Event* pPrevEvent = getEvent(pEvent->getEventClass());

    if (pPrevEvent == NULL) {
        // push to priority queue
        m_Events.push_back(pEvent);
    } else {
        // cerr << "EventManager::addEvent() : Duplicated event class" << endl;
        throw Error("EventManager::addEvent() : Duplicated event class");
    }

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
Event* EventManager::getEvent(Event::EventClass EClass)

{
    list<Event*>::iterator itr = m_Events.begin();

    for (; itr != m_Events.end(); itr++) {
        Event* pEvent = *itr;
        if (pEvent->getEventClass() == EClass)
            return pEvent;
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EventManager::deleteEvent(Event::EventClass EClass)

{
    list<Event*>::iterator current = m_Events.begin();

    while (current != m_Events.end()) {
        Assert(*current != NULL);
        Event* pEvent = *current;

        if (pEvent->getEventClass() == EClass) {
            m_Events.erase(current);
            SAFE_DELETE(pEvent);
            break;
        }

        current++;
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EventManager::heartbeat()

{
    // Measure the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    list<Event*>::iterator before = m_Events.end();
    list<Event*>::iterator current = m_Events.begin();

    while (current != m_Events.end()) {
        Assert(*current != NULL);

        Event* pEvent = *current;

        //--------------------------------------------------------------------------------
        // *WARNING*
        //
        // For an EventResurrect event the GamePlayer object moves from the ZPM
        // to the IPM. The problem is that the ZPM and the IPM are different
        // threads, so EventResurrect may be activated again in the IPM.
        // (The GamePlayer object is moved to the IPM while EventResurrect has
        // not been fully deleted from it.) To avoid this, a one-shot event
        // should first be deleted from the event manager and only then be
        // moved to the IPM.
        // A permanent event must never be moved between player managers,
        // and there is no way around that.
        //--------------------------------------------------------------------------------
        if (currentTime > pEvent->getDeadline()) {
            // For a one-shot event
            if (pEvent->isTemporary()) {
                // Delete the event.
                m_Events.erase(current);

                if (before == m_Events.end()) { // first effect
                    current = m_Events.begin();
                } else { // !first effect
                    current = before;
                    current++;
                }
            }

            // Run the event.
            pEvent->activate();

            // For a one-shot event
            if (pEvent->isTemporary()) {
                // Delete the event object.
                SAFE_DELETE(pEvent);
            }
        } else {
            before = current++;
        }
    }
}
