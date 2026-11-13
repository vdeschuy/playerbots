#pragma once
#include "playerbot/strategy/Trigger.h"

namespace ai
{
    class HerodCastingWhirlwindTrigger : public Trigger
    {
    public:
        HerodCastingWhirlwindTrigger(PlayerbotAI* ai)
            : Trigger(ai, "herod casting whirlwind", 1) {}

        bool IsActive() override;
    };

    class HerodTankSpotTrigger : public Trigger
    {
    public:
        HerodTankSpotTrigger(PlayerbotAI* ai)
            : Trigger(ai, "herod tank spot", 1) {}

        bool IsActive() override;

    private:
        ObjectGuid herodGuid;
        time_t lastSearch = 0;
    };
}
