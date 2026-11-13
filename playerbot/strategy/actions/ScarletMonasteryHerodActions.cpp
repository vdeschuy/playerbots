#include "playerbot/playerbot.h"
#include "ScarletMonasteryHerodActions.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

using namespace ai;
namespace sm = ai::scarlet_monastery;

namespace
{
    constexpr float SEARCH_RANGE = 15.0f;

    Unit* FindHerod(Player* bot)
    {
        Unit* victim = bot->GetVictim();
        if (victim && victim->GetEntry() == sm::HEROD_ENTRY)
            return victim;

        std::list<Unit*> units;
        MaNGOS::AllCreaturesOfEntryInRangeCheck check(bot, sm::HEROD_ENTRY, SEARCH_RANGE);
        MaNGOS::UnitListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(units, check);
        Cell::VisitAllObjects(bot, searcher, SEARCH_RANGE);

        for (Unit* unit : units)
        {
            if (unit && unit->IsInCombat())
                return unit;
        }

        return nullptr;
    }
}

bool HerodMoveAwayAction::Execute(Event& event)
{
    // MoveAwayFromCreature only moves while Herod is inside its search range.
    // Once the bot is outside 12 yards the base action returns false, which
    // would let "reach melee" pull the bot straight back in for the rest of the
    // 10-second channel. Hold position instead while the trigger is running.
    Unit* herod = bot->GetVictim();
    if (!herod || herod->GetEntry() != sm::HEROD_ENTRY)
        herod = ai->GetUnit(AI_VALUE(ObjectGuid, "current target"));

    if (!herod || herod->GetEntry() != sm::HEROD_ENTRY)
        herod = FindHerod(bot);

    if (herod && herod->GetEntry() == sm::HEROD_ENTRY && bot->GetDistance(herod) > range)
    {
        // If the bot is fighting something else, leave its rotation alone; it
        // is already outside the Whirlwind and should not be held for ten
        // seconds because Herod happens to be nearby.
        Unit* victim = bot->GetVictim();
        if (!victim || victim->GetEntry() != sm::HEROD_ENTRY)
            return false;

        // The hold exists to stop "reach melee" from pulling a melee bot back
        // into the channel. Ranged bots have no such action and should keep
        // casting once they are outside the safe radius.
        if (!ai->IsMelee(bot, true))
        {
            // Stop before handing control back: a residual movement tick makes
            // the next cast-time spell fail, and the generic "shoot" fallback
            // would start wanding instead of resuming the rotation.
            if (bot->IsMoving())
            {
                ai->StopMoving();
                return true;
            }

            return false;
        }

        if (bot->IsMoving())
            ai->StopMoving();

        return true;
    }

    return MoveAwayFromCreature::Execute(event);
}

bool HerodMoveToTankSpotAction::Execute(Event& event)
{
    // Only the bot Herod is currently attacking drags him; that keeps the
    // fight centred on one predictable spot. The tank may be targeting an add,
    // so look for the Herod that is attacking it instead of trusting its
    // current target.
    Unit* herod = ai->GetUnit(herodGuid);
    if (!herod || herod->GetEntry() != sm::HEROD_ENTRY || herod->GetVictim() != bot)
    {
        herod = FindHerod(bot);
        if (!herod || herod->GetEntry() != sm::HEROD_ENTRY || herod->GetVictim() != bot)
        {
            herodGuid.Clear();
            return false;
        }

        herodGuid = herod->GetObjectGuid();
    }

    const WorldPosition spot(sm::MAP_ID, sm::HEROD_TANK_SPOT_X, sm::HEROD_TANK_SPOT_Y, sm::HEROD_TANK_SPOT_Z, 0.0f);
    const float dx = bot->GetPositionX() - sm::HEROD_TANK_SPOT_X;
    const float dy = bot->GetPositionY() - sm::HEROD_TANK_SPOT_Y;

    if (dx * dx + dy * dy <= sm::HEROD_TANK_SPOT_SLACK * sm::HEROD_TANK_SPOT_SLACK)
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return false;
    }

    if (MoveTo(spot, false, IsReaction(), false, true))
        return true;

    // If the spot cannot be reached, fight where we are rather than muting the
    // tank's attacks and taunts while it retries forever.
    if (bot->IsMoving())
        ai->StopMoving();

    return false;
}
