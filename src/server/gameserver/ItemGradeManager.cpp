#include "ItemGradeManager.h"

#include "Assert.h"
#include "repository/GameInfoRepository.h"

void ItemGradeManager::load() {
    vector<ItemGradeRatioRow> rows = defaultGameInfoRepository().loadItemGradeRatios();

    Assert(rows.size() == 10);
    m_GradeGambleRatios.reserve(10);
    m_GradeBeadRatios.reserve(10);
    m_GradeRatios.reserve(10);

    for (size_t r = 0; r < rows.size(); r++) {
        int Grade = rows[r].grade;
        int Ratio = rows[r].ratio;
        int GambleRatio = rows[r].gambleRatio;
        int BeadRatio = rows[r].beadRatio;

        m_GradeRatios[Grade - 1] = Ratio;
        m_GradeGambleRatios[Grade - 1] = GambleRatio;
        m_GradeBeadRatios[Grade - 1] = BeadRatio;
        cout << Grade << " grade : " << Ratio << ", " << GambleRatio << ", " << BeadRatio << endl;
    }
}

Grade_t ItemGradeManager::getRandomGrade() const {
    int value = rand() % 100000;
    int i = 0;

    for (; i < 10; ++i) {
        value -= m_GradeRatios[i];
        if (value < 0)
            return i + 1;
    }

    return i;
}

Grade_t ItemGradeManager::getRandomGambleGrade() const {
    int value = rand() % 100000;
    int i = 0;

    for (; i < 10; ++i) {
        value -= m_GradeGambleRatios[i];
        if (value < 0)
            return i + 1;
    }

    return i;
}

Grade_t ItemGradeManager::getRandomBeadGrade() const {
    int value = rand() % 100000;
    int i = 0;

    for (; i < 10; ++i) {
        value -= m_GradeBeadRatios[i];
        if (value < 0)
            return i + 1;
    }

    return i;
}

ItemGradeManager& ItemGradeManager::Instance() {
    static ItemGradeManager theInstance;
    return theInstance;
}
