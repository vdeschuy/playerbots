#pragma once
#include "DungeonActions.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

namespace ai
{
    class HerodMoveAwayAction : public MoveAwayFromCreature
    {
    public:
        HerodMoveAwayAction(PlayerbotAI* ai)
            : MoveAwayFromCreature(ai, "move away from herod", scarlet_monastery::HEROD_ENTRY,
                scarlet_monastery::HEROD_WHIRLWIND_RANGE) {}

        bool Execute(Event& event) override;
        bool isUseful() override { return true; }
        bool IsEscapeAction() const override { return true; }
    };

    // Drag Herod to the middle of the Armory so the fight happens in the open
    // arena instead of at the entrance.
    class HerodMoveToTankSpotAction : public MovementAction
    {
    public:
        HerodMoveToTankSpotAction(PlayerbotAI* ai)
            : MovementAction(ai, "move to herod tank spot") {}

        bool Execute(Event& event) override;

    private:
        ObjectGuid herodGuid;
    };
}
