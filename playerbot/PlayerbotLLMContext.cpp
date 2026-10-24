/*
 * Copyright (C) 2005-2018 MaNGOS project <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/PlayerbotLLMContext.h"

#include "Grids/CellImpl.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Groups/Group.h"
#include "Maps/Map.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{
    // How long an event stays worth mentioning, and how many are kept.
    time_t const EVENT_TTL = 120;
    size_t const MAX_EVENTS = 12;
    size_t const MAX_NEARBY = 5;
    size_t const MAX_EVENT_LINES = 3;

    // Ranges and thresholds used when a value is turned into words.
    float const CLOSE_RANGE = 10.0f;      // closer than this: just "close"
    float const AWAY_RANGE = 15.0f;       // further than this: worth naming
    time_t const JUST_NOW = 90;           // seconds that still count as "just now"

    std::string ClassName(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return "warrior";
            case CLASS_PALADIN:      return "paladin";
            case CLASS_HUNTER:       return "hunter";
            case CLASS_ROGUE:        return "rogue";
            case CLASS_PRIEST:       return "priest";
            case CLASS_SHAMAN:       return "shaman";
            case CLASS_MAGE:         return "mage";
            case CLASS_WARLOCK:      return "warlock";
            case CLASS_DRUID:        return "druid";
#ifdef MANGOSBOT_TWO
            case CLASS_DEATH_KNIGHT: return "death knight";
#endif
            default:                 return "adventurer";
        }
    }

    std::string Percent(float pct)
    {
        std::ostringstream out;
        out << static_cast<int>(pct + 0.5f) << "%";
        return out.str();
    }

    // "close" or "18 yd" - the model has no use for a decimal.
    std::string Distance(float yards)
    {
        if (yards < CLOSE_RANGE)
            return "close";
        std::ostringstream out;
        out << static_cast<int>(yards) << " yd";
        return out.str();
    }

    std::string Duration(time_t seconds)
    {
        if (seconds < 60)
            return "under a minute";
        std::ostringstream out;
        if (seconds < 3600)
            out << (seconds / 60) << " min";
        else
            out << (seconds / 3600) << " h";
        return out.str();
    }

    std::string Ago(time_t when, time_t now)
    {
        time_t seconds = now - when;
        if (seconds < JUST_NOW)
            return "just now";
        if (seconds < 3600)
        {
            std::ostringstream out;
            out << (seconds / 60) << " min ago";
            return out.str();
        }
        std::ostringstream out;
        out << (seconds / 3600) << " h ago";
        return out.str();
    }

}

PlayerbotLLMContext& PlayerbotLLMContext::Instance()
{
    static PlayerbotLLMContext instance;
    return instance;
}

void PlayerbotLLMContext::Update(Player* master)
{
    if (!master)
        return;

    time_t now = time(nullptr);
    if (m_lastUpdate == now)
        return;                             // at most once a second

    m_lastUpdate = now;
    uint32 key = master->GetObjectGuid().GetCounter();
    std::map<uint32, Snapshot>::iterator found = m_snapshots.find(key);
    bool hadPrevious = found != m_snapshots.end();

    Snapshot snapshot;
    snapshot.at = now;
    snapshot.mapId = master->GetMapId();
    Collect(master, snapshot);
    CollectNearby(master, snapshot);

    if (hadPrevious)
    {
        Snapshot const& previous = found->second;
        snapshot.afkSince = previous.afkSince;
        CollectEvents(master->GetMap(), previous, snapshot);
    }
    if (snapshot.afk && !snapshot.afkSince)
        snapshot.afkSince = now;
    if (!snapshot.afk)
        snapshot.afkSince = 0;

    // Oldest first, so the renderer and the cap agree on which end is new.
    std::stable_sort(snapshot.events.begin(), snapshot.events.end(),
                     [](Event const& left, Event const& right) { return left.at < right.at; });

    // Forget what nobody would mention any more, oldest first.
    size_t expired = 0;
    while (expired < snapshot.events.size() && now - snapshot.events[expired].at > EVENT_TTL)
        ++expired;
    if (expired)
        snapshot.events.erase(snapshot.events.begin(), snapshot.events.begin() + expired);
    if (snapshot.events.size() > MAX_EVENTS)
        snapshot.events.erase(snapshot.events.begin(),
                              snapshot.events.end() - MAX_EVENTS);

    m_snapshots[key] = snapshot;

    if (sPlayerbotAIConfig.llmLiveContext)
        WriteFile(master);
}

void PlayerbotLLMContext::Collect(Player* master, Snapshot& out)
{
    out.inCombat = master->IsInCombat();
    out.afk = master->isAFK();
    out.taxi = master->IsTaxiFlying();

    out.target.clear();
    out.targetLevel = 0;
    out.targetHealthPct = 0.0f;
    if (Unit* target = master->GetTarget())
    {
        out.target = target->GetName();
        out.targetLevel = target->GetLevel();
        if (target->IsAlive())
            out.targetHealthPct = target->GetHealthPercent();
    }

    // The master first, then the rest of the group.
    std::vector<Player*> players;
    players.push_back(master);
    if (Group* group = master->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->getSource();
            if (member && member != master)
                players.push_back(member);
        }
    }

    for (size_t i = 0; i < players.size(); ++i)
    {
        Player* player = players[i];
        if (!player || !player->IsInWorld())
            continue;

        Member member;
        member.guid = player->GetObjectGuid();
        member.name = player->GetName();
        member.level = player->GetLevel();
        member.cls = player->getClass();
        member.healthPct = player->GetHealthPercent();
        member.bot = player->GetPlayerbotAI() != nullptr;
        member.dead = !player->IsAlive();
        // A member in another map is not "40 yards away", it is elsewhere.
        member.distance = (player == master || player->GetMapId() != master->GetMapId())
                              ? 0.0f
                              : master->GetDistance(player);
        out.members.push_back(member);
    }
}

void PlayerbotLLMContext::CollectNearby(Player* master, Snapshot& out)
{
    out.nearby.clear();

    std::list<Creature*> creatures;
    MaNGOS::AnyUnitInObjectRangeCheck check(master, sPlayerbotAIConfig.llmLiveContextRange);
    MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(creatures, check);
    Cell::VisitGridObjects(master, searcher, sPlayerbotAIConfig.llmLiveContextRange);

    FactionTemplateEntry const* myFaction = master->GetFactionTemplateEntry();
    if (!myFaction)
        return;

    for (std::list<Creature*>::iterator it = creatures.begin(); it != creatures.end(); ++it)
    {
        Creature* creature = *it;
        if (!creature || !creature->IsAlive() || creature->GetLevel() == 0)
            continue;
        FactionTemplateEntry const* theirFaction = creature->GetFactionTemplateEntry();
        if (!theirFaction || !myFaction->IsHostileTo(*theirFaction))
            continue;
        CreatureInfo const* info = creature->GetCreatureInfo();
        if (!info)
            continue;

        NearbyUnit unit;
        unit.guid = creature->GetObjectGuid();
        unit.name = creature->GetName();
        unit.level = creature->GetLevel();
        unit.distance = master->GetDistance(creature);
        unit.elite = info->Rank != CREATURE_ELITE_NORMAL;
        out.nearby.push_back(unit);
    }

    // Closest first, then keep the few a person would actually mention.
    std::sort(out.nearby.begin(), out.nearby.end(),
              [](NearbyUnit const& left, NearbyUnit const& right)
              {
                  return left.distance < right.distance;
              });
    if (out.nearby.size() > MAX_NEARBY)
        out.nearby.resize(MAX_NEARBY);
}

void PlayerbotLLMContext::CollectEvents(Map* map, Snapshot const& before,
                                        Snapshot& after) const
{
    time_t now = after.at;

    // Deaths, resurrections and level-ups.
    for (size_t i = 0; i < after.members.size(); ++i)
    {
        Member const& member = after.members[i];
        for (size_t j = 0; j < before.members.size(); ++j)
        {
            Member const& was = before.members[j];
            if (was.guid != member.guid)
                continue;

            if (member.dead && !was.dead)
                after.events.push_back(Event{ now, "death", member.name + " died." });
            else if (was.dead && !member.dead)
                after.events.push_back(Event{ now, "res", member.name + " is back up." });

            if (member.level > was.level)
                after.events.push_back(Event{ now, "level_up", member.name +
                                              " reached level " +
                                              std::to_string(member.level) + "." });
            break;
        }
    }

    // The player going AFK (or coming back) is the single most useful event for
    // a line that is not answering anything.
    if (after.afk != before.afk)
    {
        std::string text = "The player went AFK.";
        if (!after.afk)
            text = before.afkSince > 0
                       ? "The player is back after " + Duration(now - before.afkSince) + "."
                       : "The player is back.";
        after.events.push_back(Event{ now, after.afk ? "afk_start" : "afk_end", text });
    }

    // An elite that was in front of the group and is gone *and dead* is a kill.
    // Leaving the top-5 window or the search radius is not: the creature still
    // exists and is still alive, so nothing is said.
    for (size_t i = 0; i < before.nearby.size(); ++i)
    {
        NearbyUnit const& was = before.nearby[i];
        if (!was.elite)
            continue;
        bool stillListed = false;
        for (size_t j = 0; j < after.nearby.size(); ++j)
            if (after.nearby[j].guid == was.guid)
                stillListed = true;
        if (stillListed)
            continue;

        // It left the list: either it died, or it is merely out of range now.
        // Only a creature that is gone or dead counts as killed.
        Creature* creature = map ? map->GetCreature(was.guid) : nullptr;
        if (creature && creature->IsAlive())
            continue;
        after.events.push_back(Event{ now, "boss_kill", was.name + " is down." });
    }

    // Carry the older events forward; they are re-sorted and capped by Update().
    after.events.insert(after.events.end(), before.events.begin(), before.events.end());
}

PlayerbotLLMContext::Snapshot const* PlayerbotLLMContext::Find(Player* bot) const
{
    if (!bot)
        return nullptr;
    ObjectGuid guid = bot->GetObjectGuid();
    for (std::map<uint32, Snapshot>::const_iterator it = m_snapshots.begin();
         it != m_snapshots.end(); ++it)
    {
        Snapshot const& snapshot = it->second;
        for (size_t i = 0; i < snapshot.members.size(); ++i)
            if (snapshot.members[i].guid == guid)
                return &snapshot;
    }
    return nullptr;
}

std::string PlayerbotLLMContext::Describe(Player* bot) const
{
    Snapshot const* found = Find(bot);
    if (!found || found->members.empty())
        return "";

    ObjectGuid botGuid = bot->GetObjectGuid();
    time_t now = time(nullptr);
    std::ostringstream out;
    AppendSelf(out, *found, botGuid);
    AppendCombat(out, *found);
    AppendParty(out, *found, botGuid);
    AppendNearby(out, *found);
    AppendEvents(out, *found, now);
    AppendStates(out, *found);
    return out.str();
}

void PlayerbotLLMContext::AppendSelf(std::ostringstream& out, Snapshot const& snapshot,
                                     ObjectGuid bot) const
{
    for (size_t i = 0; i < snapshot.members.size(); ++i)
    {
        Member const& member = snapshot.members[i];
        if (member.guid != bot)
            continue;
        if (member.dead)
            out << "You are dead.";
        else
            out << "You are at " << Percent(member.healthPct) << " health.";
        return;
    }
}

void PlayerbotLLMContext::AppendCombat(std::ostringstream& out, Snapshot const& snapshot) const
{
    if (snapshot.inCombat && !snapshot.target.empty())
        out << " You are fighting " << snapshot.target << " (level "
            << snapshot.targetLevel << ", " << Percent(snapshot.targetHealthPct)
            << " health).";
    else if (snapshot.inCombat)
        out << " You are in combat.";
    else
        out << " You are not in combat.";
}

void PlayerbotLLMContext::AppendParty(std::ostringstream& out, Snapshot const& snapshot,
                                      ObjectGuid bot) const
{
    if (snapshot.members.size() < 2)
        return;

    out << " With you:";
    bool first = true;
    for (size_t i = 0; i < snapshot.members.size(); ++i)
    {
        Member const& member = snapshot.members[i];
        if (member.guid == bot)
            continue;
        if (!first)
            out << ",";
        first = false;
        out << " " << member.name << (member.bot ? "" : " (the player)");
        out << " (level " << member.level << " " << ClassName(member.cls);
        if (member.dead)
            out << ", dead";
        else
            out << ", " << Percent(member.healthPct) << " health";
        if (member.distance > AWAY_RANGE)
            out << ", " << Distance(member.distance) << " away";
        out << ")";
    }
    out << ".";
}

void PlayerbotLLMContext::AppendNearby(std::ostringstream& out, Snapshot const& snapshot) const
{
    if (snapshot.nearby.empty())
        return;

    out << " Nearby:";
    for (size_t i = 0; i < snapshot.nearby.size(); ++i)
    {
        NearbyUnit const& unit = snapshot.nearby[i];
        if (i)
            out << ",";
        out << " " << unit.name << " (level " << unit.level
            << (unit.elite ? ", elite" : "") << ", " << Distance(unit.distance) << ")";
    }
    out << ".";
}

void PlayerbotLLMContext::AppendEvents(std::ostringstream& out, Snapshot const& snapshot,
                                       time_t now) const
{
    size_t shown = 0;
    for (size_t i = snapshot.events.size(); i > 0 && shown < MAX_EVENT_LINES; --i)
    {
        Event const& event = snapshot.events[i - 1];
        if (now - event.at > EVENT_TTL)
            continue;
        out << (shown == 0 ? " Just now: " : " ") << Ago(event.at, now) << ": "
            << event.text;
        ++shown;
    }
}

void PlayerbotLLMContext::AppendStates(std::ostringstream& out, Snapshot const& snapshot) const
{
    if (snapshot.afk)
        out << " The player is AFK right now.";
    if (snapshot.taxi)
        out << " The player is on a flight path.";
}

std::string PlayerbotLLMContext::DescribeGroup(Player* master) const
{
    // The format is documented in the header (DescribeGroup).
    Snapshot const* found = Find(master);
    if (!found || found->members.empty())
        return "";

    std::ostringstream out;
    out << "at=" << found->at << "\n";
    out << "map=" << found->mapId << "\n";
    out << "combat=" << (found->inCombat ? 1 : 0) << "\n";
    if (found->inCombat && !found->target.empty())
        out << "target=" << found->target << "|" << found->targetLevel << "|"
            << static_cast<int>(found->targetHealthPct) << "\n";
    for (size_t i = 0; i < found->members.size(); ++i)
    {
        Member const& member = found->members[i];
        out << "member=" << member.name
            << "|" << (member.bot ? "bot" : "player")
            << "|" << member.level
            << "|" << ClassName(member.cls)
            << "|" << static_cast<int>(member.healthPct)
            << "|" << static_cast<int>(member.distance)
            << "|" << (member.dead ? 1 : 0) << "\n";
    }
    for (size_t i = 0; i < found->nearby.size(); ++i)
    {
        NearbyUnit const& unit = found->nearby[i];
        out << "nearby=" << unit.name << "|" << unit.level << "|"
            << (unit.elite ? "elite" : "normal") << "|"
            << static_cast<int>(unit.distance) << "\n";
    }
    for (size_t i = 0; i < found->events.size(); ++i)
        out << "event=" << found->events[i].at << "|" << found->events[i].kind << "|"
            << found->events[i].text << "\n";
    out << "afk=" << (found->afk ? 1 : 0) << "\n";
    out << "taxi=" << (found->taxi ? 1 : 0) << "\n";
    return out.str();
}

void PlayerbotLLMContext::WriteFile(Player* master) const
{
    if (sPlayerbotAIConfig.llmLiveContextFile.empty())
        return;
    std::string text = DescribeGroup(master);
    if (text.empty())
        return;
    WriteText(sPlayerbotAIConfig.llmLiveContextFile, text);
}

void PlayerbotLLMContext::WriteText(std::string const& path, std::string const& text) const
{
    // Write beside the target, then rename over it: a reader never sees half a
    // snapshot, and std::filesystem::rename replaces an existing file on both
    // POSIX and Windows (C's rename() does not, on MSVC).
    std::string temporary = path + ".tmp";
    {
        std::ofstream out(temporary.c_str(), std::ios::out | std::ios::trunc);
        if (!out)
        {
            sLog.outError("PlayerbotLLMContext: cannot write %s", temporary.c_str());
            return;
        }
        out << text;
        out.flush();
        if (!out)
        {
            sLog.outError("PlayerbotLLMContext: write failed for %s", temporary.c_str());
            out.close();
            std::remove(temporary.c_str());
            return;
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        sLog.outError("PlayerbotLLMContext: cannot replace %s: %s", path.c_str(),
                      error.message().c_str());
        std::remove(temporary.c_str());
    }
}
