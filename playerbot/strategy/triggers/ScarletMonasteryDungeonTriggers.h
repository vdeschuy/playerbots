#pragma once
#include "DungeonTriggers.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

namespace ai
{
    class ScarletMonasteryEnterDungeonTrigger : public EnterDungeonTrigger
    {
    public:
        ScarletMonasteryEnterDungeonTrigger(PlayerbotAI* ai)
            : EnterDungeonTrigger(ai, "enter scarlet monastery", "scarlet monastery", scarlet_monastery::MAP_ID) {}
    };

    class ScarletMonasteryLeaveDungeonTrigger : public LeaveDungeonTrigger
    {
    public:
        ScarletMonasteryLeaveDungeonTrigger(PlayerbotAI* ai)
            : LeaveDungeonTrigger(ai, "leave scarlet monastery", "scarlet monastery", scarlet_monastery::MAP_ID) {}
    };
}
