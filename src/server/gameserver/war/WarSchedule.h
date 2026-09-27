///////////////////////////////////////////////////////////////////
// War schedule. For DB I/O support
///////////////////////////////////////////////////////////////////

#ifndef __WAR_SCHEDULE_H__
#define __WAR_SCHEDULE_H__

#include "Exception.h"
#include "Schedule.h"
#include "Types.h"
#include "War.h"
#include "Work.h"

struct WarScheduleInfo;
class WarInfo;

class WarSchedule : public Schedule {
public:
    WarSchedule(Work* pWork, const VSDateTime& Time, ScheduleType type = SCHEDULE_TYPE_ONCE);
    virtual ~WarSchedule();

public:
    void create();
    void tinysave(const string& query);
    void save();
    const War* getWar() const {
        return dynamic_cast<const War*>(getWork());
    }
    War* getWar() {
        return dynamic_cast<War*>(getWork());
    }
    WarID_t getWarID() const {
        return getWar()->getWarID();
    }

    // Executes the war and records its new state in the war's row.
    void run() override;

public:
    void makeWarScheduleInfo(WarScheduleInfo* pWSI) const;
    void makeWarInfo(WarInfo* pWarInfo) const;

private:
};

#endif // __WAR_SCHEDULE_H__
