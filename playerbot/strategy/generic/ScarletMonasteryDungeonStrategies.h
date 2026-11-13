#pragma once
#include "playerbot/strategy/Strategy.h"

namespace ai
{
    class ScarletMonasteryDungeonStrategy : public Strategy
    {
    public:
        ScarletMonasteryDungeonStrategy(PlayerbotAI* ai) : Strategy(ai) {}
        std::string getName() override { return "scarlet monastery"; }

    private:
        void InitCombatTriggers(std::list<TriggerNode*>& triggers) override;
    };
}
