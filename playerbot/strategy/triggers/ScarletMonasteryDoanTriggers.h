#pragma once
#include "playerbot/strategy/Trigger.h"

namespace ai
{
    class DoanCastingDetonationTrigger : public Trigger
    {
    public:
        DoanCastingDetonationTrigger(PlayerbotAI* ai)
            : Trigger(ai, "doan casting detonation", 1) {}

        bool IsActive() override;

    private:
        ObjectGuid doanGuid;
        time_t lastSearch = 0;
    };

    // Active while the bot holding Doan's aggro is away from the tank spot.
    // Lets the tank bring the boss back to the doorway after everyone has
    // stepped out for Detonation.
    class DoanTankSpotTrigger : public Trigger
    {
    public:
        DoanTankSpotTrigger(PlayerbotAI* ai)
            : Trigger(ai, "doan tank spot", 1) {}

        bool IsActive() override;
    };
}
