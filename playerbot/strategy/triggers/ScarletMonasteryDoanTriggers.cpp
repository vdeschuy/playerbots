#include "playerbot/playerbot.h"
#include "ScarletMonasteryDoanTriggers.h"
#include "playerbot/strategy/ScarletMonasteryDoan.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

using namespace ai;
namespace sm = ai::scarlet_monastery;

namespace
{
    constexpr float SEARCH_RANGE = 60.0f;

    // The Doan wing of the monastery. Other wings' fights never pay for the
    // Doan search.
    constexpr float WING_MIN_X = 120.0f;
    constexpr float WING_MAX_X = 215.0f;
    constexpr float WING_MIN_Y = -465.0f;
    constexpr float WING_MAX_Y = -400.0f;

    bool InDoanWing(Player* bot)
    {
        const float x = bot->GetPositionX();
        const float y = bot->GetPositionY();
        return x > WING_MIN_X && x < WING_MAX_X && y > WING_MIN_Y && y < WING_MAX_Y;
    }

    Unit* FindCombatDoan(Player* bot)
    {
        std::list<Unit*> units;
        MaNGOS::AllCreaturesOfEntryInRangeCheck check(bot, sm::DOAN_ENTRY, SEARCH_RANGE);
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

namespace ai
{
    namespace scarlet_monastery
    {
        bool IsCastingDetonation(Unit* doan)
        {
            if (!doan || !doan->IsInCombat())
                return false;

            Creature* creature = static_cast<Creature*>(doan);
            Spell const* spell = creature->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL);
            return spell && spell->m_spellInfo &&
                spell->m_spellInfo->Id == SPELL_DETONATION &&
                spell->getState() != SPELL_STATE_FINISHED;
        }
    }
}

bool DoanCastingDetonationTrigger::IsActive()
{
    if (bot->GetMapId() != sm::MAP_ID || !bot->IsAlive() || !bot->IsInCombat())
        return false;

    if (!InDoanWing(bot))
        return false;

    Unit* doan = bot->GetVictim();
    if (!doan || doan->GetEntry() != sm::DOAN_ENTRY)
    {
        // Reuse the cached Doan. Healers usually target the tank, so without
        // this they would run the 60-yard search on every AI tick.
        doan = ai->GetUnit(doanGuid);
        if (!doan || doan->GetEntry() != sm::DOAN_ENTRY || !doan->IsInCombat())
        {
            // Throttle misses: while the group clears trash in this wing there
            // is no Doan to find, and the search is not cheap.
            if (time(0) == lastSearch)
                return false;

            lastSearch = time(0);
            doanGuid.Clear();
            doan = FindCombatDoan(bot);

            if (doan)
                doanGuid = doan->GetObjectGuid();
        }
    }

    if (!doan || bot->GetDistance(doan, true, DIST_CALC_COMBAT_REACH) > SEARCH_RANGE)
        return false;

    // The bubble is the early warning, the cast is the hard deadline. Distant
    // ranged bots need the extra second; the bot holding Doan's aggro waits for
    // the cast in the action so the boss stays parked.
    return sm::IsCastingDetonation(doan) || doan->HasAura(sm::SPELL_ARCANE_BUBBLE);
}

bool DoanTankSpotTrigger::IsActive()
{
    if (bot->GetMapId() != sm::MAP_ID || !bot->IsAlive() || !bot->IsInCombat())
        return false;

    Unit* doan = bot->GetVictim();
    if (!doan || doan->GetEntry() != sm::DOAN_ENTRY)
        return false;

    // Only the bot Doan is currently attacking drags him into position.
    if (doan->GetVictim() != bot)
        return false;

    // While the bubble or the Detonation cast is up, the escape action owns the
    // movement. The lingering bubble also keeps the tank from re-engaging while
    // pacified Doan is still free to chase.
    if (doan->HasAura(sm::SPELL_ARCANE_BUBBLE) || sm::IsCastingDetonation(doan))
        return false;

    const float dx = bot->GetPositionX() - sm::DOAN_TANK_SPOT_X;
    const float dy = bot->GetPositionY() - sm::DOAN_TANK_SPOT_Y;
    return dx * dx + dy * dy > sm::DOAN_TANK_SPOT_SLACK * sm::DOAN_TANK_SPOT_SLACK;
}
