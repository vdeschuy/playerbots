#pragma once
#include "DungeonActions.h"
#include "ChangeStrategyAction.h"

namespace ai
{
    class ScarletMonasteryEnableStrategyAction : public ChangeAllStrategyAction
    {
    public:
        ScarletMonasteryEnableStrategyAction(PlayerbotAI* ai)
            : ChangeAllStrategyAction(ai, "enable scarlet monastery strategy", "+scarlet monastery") {}
    };

    class ScarletMonasteryDisableStrategyAction : public ChangeAllStrategyAction
    {
    public:
        ScarletMonasteryDisableStrategyAction(PlayerbotAI* ai)
            : ChangeAllStrategyAction(ai, "disable scarlet monastery strategy", "-scarlet monastery") {}
    };
}
