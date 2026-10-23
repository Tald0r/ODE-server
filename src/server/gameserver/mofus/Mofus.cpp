/////////////////////////////////////////////////////////////////////////////
// Filename : Mofus.cpp
// Desc		:
/////////////////////////////////////////////////////////////////////////////

#include "Mofus.h"

#include "DatabaseError.h"
#include "Exception.h"
#include "repository/MofusPointRepository.h"

// Every function here deliberately IGNORES a SQL failure and carries on
// with a zero balance (SQL errors are deliberately ignored): the mofus link is an
// external service, and the game must not fall over when its bookkeeping
// does.
//
// The type the swallow has to name is the DatabaseError each repository
// call throws from END_DB after converting the driver's
// SQLQueryException; catching SQLQueryException here would silently turn
// an ignored error into a thrown one.
//
// What the swallow prevents is worse than an ignored error becoming a
// thrown one, and it differs per caller. PlayerCreature::load calls
// loadPowerPoint on EVERY character login, so an escape there reaches
// GamePlayer::processCommand's catch (...) and disconnects the player
// instead of logging them in with zero points. The Restore and
// EventMorph paths call it on a zone thread, where nothing catches a
// DatabaseError at all -- std::terminate, i.e. the process. And
// MPlayerManager::processResult calls it on the mofus thread, whose
// run loop catches Throwable& only: the scoped critical section
// releases the PCFinder on the way out, but the escape then leaves
// run() and takes the process with it.

int loadPowerPoint(const string& name) {
    int powerpoint = 0;

    try {
        defaultMofusPointRepository().loadPowerPoint(name, powerpoint);
    } catch (const DatabaseError&) {
        // SQL errors are ignored.
    }

    return powerpoint;
}

int savePowerPoint(const string& name, int amount) {
    int powerpoint = 0;

    // One try around all three statements, as before: a failure in any of
    // them skips the rest and leaves powerpoint at 0.
    try {
        MofusPointRepository& points = defaultMofusPointRepository();

        // No row yet means a first save, which the insert creates.
        if (!points.increasePowerPoint(name, amount)) {
            points.insertPowerPoint(name, amount);
        }

        points.loadPowerPoint(name, powerpoint);
    } catch (const DatabaseError&) {
        // SQL errors are ignored.
    }

    return powerpoint;
}

void logPowerPoint(const string& name, int recvPoint, int savePoint) {
    try {
        defaultMofusPointRepository().logPowerPoint(name, recvPoint, savePoint);
    } catch (const DatabaseError&) {
        // SQL errors are ignored.
    }
}
