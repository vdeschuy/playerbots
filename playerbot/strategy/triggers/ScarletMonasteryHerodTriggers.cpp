#include "playerbot/playerbot.h"
#include "ScarletMonasteryHerodTriggers.h"
#include "playerbot/strategy/ScarletMonasteryPoints.h"

#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

using namespace ai;
namespace sm = ai::scarlet_monastery;

namespace
{
    constexpr float SEARCH_RANGE = 15.0f;

    // The Armory wing, including the corridor that leads into Herod's room.
    // Other wings' fights never pay for the Herod searches below.
    constexpr float ARMORY_MIN_X = 1500.0f;
    constexpr float ARMORY_MAX_X = 2050.0f;
    constexpr float ARMORY_MIN_Y = -500.0f;
    constexpr float ARMORY_MAX_Y = -250.0f;

    bool InArmory(Player* bot)
    {
        const float x = bot->GetPositionX();
        const float y = bot->GetPositionY();
        return x > ARMORY_MIN_X && x < ARMORY_MAX_X && y > ARMORY_MIN_Y && y < ARMORY_MAX_Y;
    }

    bool IsChannelingWhirlwind(Unit* unit)
    {
        Creature* creature = (Creature*)unit;
        if (!creature || !creature->IsInCombat())
            return false;

        // Whirlwind 8989 is self-channeled. The generic slot is checked as a
        // fallback in case the server stores it there during a transition.
        Spell const* spell = creature->GetCurrentSpell(CurrentSpellTypes::CURRENT_CHANNELED_SPELL);
        if (!spell)
            spell = creature->GetCurrentSpell(CurrentSpellTypes::CURRENT_GENERIC_SPELL);

        return spell && spell->m_spellInfo &&
            spell->m_spellInfo->Id == sm::SPELL_WHIRLWIND &&
            spell->getState() != SPELL_STATE_FINISHED;
    }

    Unit* FindAggroHerod(Player* bot)
    {
        Unit* victim = bot->GetVictim();
        if (victim && victim->GetEntry() == sm::HEROD_ENTRY && victim->GetVictim() == bot)
            return victim;

        std::list<Unit*> units;
        MaNGOS::AllCreaturesOfEntryInRangeCheck check(bot, sm::HEROD_ENTRY, SEARCH_RANGE);
        MaNGOS::UnitListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(units, check);
        Cell::VisitAllObjects(bot, searcher, SEARCH_RANGE);

        for (Unit* unit : units)
        {
            if (unit && unit->GetVictim() == bot)
                return unit;
        }

        return nullptr;
    }
}

bool HerodCastingWhirlwindTrigger::IsActive()
{
    // Cheapest checks first: the map guard keeps every bot outside the Armory
    // away from the creature search below.
    if (bot->GetMapId() != sm::MAP_ID || !bot->IsAlive() || !bot->IsInCombat())
        return false;

    if (!InArmory(bot))
        return false;

    Unit* herod = bot->GetVictim();
    if (herod && herod->GetEntry() == sm::HEROD_ENTRY)
    {
        if (!IsChannelingWhirlwind(herod))
            return false;

        // A tank holding Herod's aggro stays put and eats the Whirlwind.
        // Other tanks must still run when a pet, human or another bot has
        // aggro.
        return !(herod->GetVictim() == bot && ai->IsTank(bot));
    }

    // If the bot is on another target, look only 15 yards around it: the
    // action moves only when Herod is inside 12, and the hold keeps the bot
    // outside 12 once it is there. A 60-yard search is unnecessary here.
    std::list<Unit*> units;
    MaNGOS::AllCreaturesOfEntryInRangeCheck check(bot, sm::HEROD_ENTRY, SEARCH_RANGE);
    MaNGOS::UnitListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(units, check);
    Cell::VisitAllObjects(bot, searcher, SEARCH_RANGE);

    for (Unit* unit : units)
    {
        if (!unit || !IsChannelingWhirlwind(unit))
            continue;

        return !(unit->GetVictim() == bot && ai->IsTank(bot));
    }

    return false;
}

bool HerodTankSpotTrigger::IsActive()
{
    if (bot->GetMapId() != sm::MAP_ID || !bot->IsAlive() || !bot->IsInCombat())
        return false;

    if (!InArmory(bot))
        return false;

    Unit* herod = bot->GetVictim();
    if (herod && herod->GetEntry() == sm::HEROD_ENTRY)
    {
        // Targeting Herod but he is on someone else: this bot is not the one
        // that should drag him.
        if (herod->GetVictim() != bot)
            return false;

        herodGuid = herod->GetObjectGuid();
    }
    else
    {
        // The tank may have switched to an add. Reuse the cached Herod, and
        // throttle searches for a new one.
        herod = ai->GetUnit(herodGuid);
        if (!herod || herod->GetEntry() != sm::HEROD_ENTRY || herod->GetVictim() != bot)
        {
            if (time(0) == lastSearch)
                return false;

            lastSearch = time(0);
            herodGuid.Clear();
            herod = FindAggroHerod(bot);

            if (herod)
                herodGuid = herod->GetObjectGuid();
        }
    }

    if (!herod || herod->GetEntry() != sm::HEROD_ENTRY || herod->GetVictim() != bot)
        return false;

    // During the channel the tank stays where it is and eats the Whirlwind;
    // moving now would only pull the fight away from the centre.
    if (IsChannelingWhirlwind(herod))
        return false;

    const float dx = bot->GetPositionX() - sm::HEROD_TANK_SPOT_X;
    const float dy = bot->GetPositionY() - sm::HEROD_TANK_SPOT_Y;
    return dx * dx + dy * dy > sm::HEROD_TANK_SPOT_SLACK * sm::HEROD_TANK_SPOT_SLACK;
}
