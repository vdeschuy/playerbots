#pragma once
#include "MovementActions.h"

namespace ai
{
    class DoanMoveToSafeSpotAction : public MovementAction
    {
    public:
        DoanMoveToSafeSpotAction(PlayerbotAI* ai)
            : MovementAction(ai, "move to doan safe spot") {}

        bool Execute(Event& event) override;
        bool isUseful() override { return true; }
        bool IsEscapeAction() const override { return true; }

    private:
        ObjectGuid doanGuid;
        int routeStep = 0; // 0 = doorway, 1 = hall waypoint, 2 = shelter
        time_t lastMarker = 0;
    };

    // Bring Doan back to the doorway spot while the bot holds his aggro, so the
    // group always knows where the next Detonation will be.
    class DoanMoveToTankSpotAction : public MovementAction
    {
    public:
        DoanMoveToTankSpotAction(PlayerbotAI* ai)
            : MovementAction(ai, "move to doan tank spot") {}

        bool Execute(Event& event) override;
        bool isUseful() override { return true; }

    private:
        time_t lastMarker = 0;
    };
}
