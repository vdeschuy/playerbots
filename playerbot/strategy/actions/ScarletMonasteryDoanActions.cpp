#include "playerbot/playerbot.h"
#include "ScarletMonasteryDoanActions.h"
#include "playerbot/strategy/ScarletMonasteryDoan.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

using namespace ai;
namespace sm = ai::scarlet_monastery;

namespace
{
    constexpr float DOAN_SEARCH_RANGE = 60.0f;
    constexpr float DETONATION_RADIUS = 30.0f;
    constexpr float ARRIVE_DISTANCE = 1.0f;
    constexpr float WAYPOINT_REACH = 3.0f;
    constexpr int DEBUG_MARKER_INTERVAL = 5;

    Unit* FindDoan(Player* bot)
    {
        Unit* victim = bot->GetVictim();
        if (victim && victim->GetEntry() == sm::DOAN_ENTRY && victim->IsInCombat())
            return victim;

        std::list<Unit*> units;
        MaNGOS::AllCreaturesOfEntryInRangeCheck check(bot, sm::DOAN_ENTRY, DOAN_SEARCH_RANGE);
        MaNGOS::UnitListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(units, check);
        Cell::VisitAllObjects(bot, searcher, DOAN_SEARCH_RANGE);

        for (Unit* unit : units)
        {
            if (unit && unit->IsInCombat())
                return unit;
        }

        return nullptr;
    }

    void ShowMarker(PlayerbotAI* ai, Player* bot, time_t& last, const WorldPosition& pos)
    {
        if (!ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
            return;

        const time_t now = time(0);
        if (now - last < DEBUG_MARKER_INTERVAL)
            return;

        last = now;
        bot->SummonCreature(15631, pos.getX(), pos.getY(), pos.getZ(), 0.0f, TEMPSPAWN_TIMED_DESPAWN, 5000.0f);
    }
}

bool DoanMoveToSafeSpotAction::Execute(Event& event)
{
    Unit* doanUnit = ai->GetUnit(doanGuid);
    if (!doanUnit || doanUnit->GetEntry() != sm::DOAN_ENTRY || !doanUnit->IsInCombat())
    {
        doanGuid.Clear();
        routeStep = 0;
        doanUnit = FindDoan(bot);

        if (doanUnit)
            doanGuid = doanUnit->GetObjectGuid();
    }

    if (!doanUnit)
        return false;

    Creature* doan = static_cast<Creature*>(doanUnit);
    const WorldPosition doanPosition(doan);
    const WorldPosition botPosition(bot);
    const float effectiveRadius = DETONATION_RADIUS + bot->GetCombatReach() + doan->GetCombatReach();

    const WorldPosition doorway(sm::MAP_ID, sm::DOORWAY_X, sm::DOORWAY_Y, sm::DOORWAY_Z, 0.0f);
    const WorldPosition hall(sm::MAP_ID, sm::DOAN_HALL_X, sm::DOAN_HALL_Y, sm::DOAN_HALL_Z, 0.0f);
    const WorldPosition shelter(sm::MAP_ID, sm::DOAN_SAFE_X, sm::DOAN_SAFE_Y, sm::DOAN_SAFE_Z, 0.0f);

    // Parked at the shelter: hold. The shelter and its one-yard arrival ring
    // are LOS-blocked from Doan, including while he is displaced a few yards
    // off the tank spot, so no extra check is needed here.
    if (botPosition.distance(shelter) <= ARRIVE_DISTANCE)
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return true;
    }

    // Already outside the blast radius: hold instead of moving again.
    if (doanPosition.distance(botPosition) > effectiveRadius + 1.0f)
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return true;
    }

    // Behind the antechamber wall: stay there. Without this, combat movement
    // pulls the bot back into the room and into the blast. Only consider this
    // once the bot is actually past the wall; at the doorway it is still in
    // the open and must keep moving.
    if (botPosition.getX() >= sm::DOAN_ANTECHAMBER_X &&
        !bot->IsWithinLOS(doanPosition.getX(), doanPosition.getY(),
            doanPosition.getZ() + bot->GetCollisionHeight(), true))
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return true;
    }

    // The bot holding Doan's aggro waits for the cast. Leaving on the bubble
    // would make Doan follow and move the fight; everyone else can leave early.
    if (doan->GetVictim() == bot && !sm::IsCastingDetonation(doan))
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return true;
    }

    // Route out: inside -> doorway -> hall waypoint -> shelter. Latch the
    // phase so steering near a threshold cannot flip it back and rebuild the
    // path.
    if (botPosition.getX() < sm::DOORWAY_X - sm::DOORWAY_MARGIN)
        routeStep = 0;
    else if (routeStep == 0 && botPosition.distance(doorway) <= WAYPOINT_REACH)
        routeStep = 1;

    if (routeStep == 1 && botPosition.distance(hall) <= WAYPOINT_REACH)
        routeStep = 2;

    WorldPosition target = shelter;
    if (routeStep == 0)
        target = doorway;
    else if (routeStep == 1)
        target = hall;

    if (MoveTo(target, false, IsReaction(), false, true))
    {
        ShowMarker(ai, bot, lastMarker, target);
        return true;
    }

    // A failed path must not hand movement back to combat actions, which would
    // walk the bot toward Doan. Hold and retry next tick.
    if (bot->IsMoving())
        ai->StopMoving();

    return true;
}

bool DoanMoveToTankSpotAction::Execute(Event& event)
{
    // Only the bot Doan is currently attacking drags him into position; that
    // keeps the blast centre predictable for the rest of the group.
    Unit* doan = bot->GetVictim();
    if (!doan || doan->GetEntry() != sm::DOAN_ENTRY || doan->GetVictim() != bot)
        return false;

    const WorldPosition spot(sm::MAP_ID, sm::DOAN_TANK_SPOT_X, sm::DOAN_TANK_SPOT_Y, sm::DOAN_TANK_SPOT_Z, 0.0f);
    const WorldPosition botPosition(bot);

    if (botPosition.distance(spot) <= ARRIVE_DISTANCE)
    {
        if (bot->IsMoving())
            ai->StopMoving();

        return false;
    }

    // Coming back from the antechamber, round the wall corner before heading
    // in through the door.
    const WorldPosition hall(sm::MAP_ID, sm::DOAN_HALL_X, sm::DOAN_HALL_Y, sm::DOAN_HALL_Z, 0.0f);
    if (botPosition.getX() > sm::DOORWAY_X + sm::DOORWAY_MARGIN &&
        botPosition.getY() < sm::DOAN_HALL_Y &&
        botPosition.distance(hall) > WAYPOINT_REACH &&
        MoveTo(hall, false, IsReaction(), false, true))
        return true;

    if (MoveTo(spot, false, IsReaction(), false, true))
    {
        ShowMarker(ai, bot, lastMarker, spot);
        return true;
    }

    return false;
}
