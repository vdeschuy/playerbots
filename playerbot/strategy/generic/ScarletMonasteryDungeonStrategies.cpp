#include "playerbot/playerbot.h"
#include "ScarletMonasteryDungeonStrategies.h"

using namespace ai;

void ScarletMonasteryDungeonStrategy::InitCombatTriggers(std::list<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "herod casting whirlwind",
        NextAction::array(0, new NextAction("move away from herod", 100.0f), NULL)));

    // Above ordinary movement (30) so the drag wins, but below interrupts (40)
    // and heals: the tank must not stop taunting or interrupting while it
    // repositions.
    triggers.push_back(new TriggerNode(
        "herod tank spot",
        NextAction::array(0, new NextAction("move to herod tank spot", 35.0f), NULL)));

    // Same priority for the Doan hold: above movement, below interrupts/heals.
    triggers.push_back(new TriggerNode(
        "doan tank spot",
        NextAction::array(0, new NextAction("move to doan tank spot", 35.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "doan casting detonation",
        NextAction::array(0, new NextAction("move to doan safe spot", 100.0f), NULL)));
}
