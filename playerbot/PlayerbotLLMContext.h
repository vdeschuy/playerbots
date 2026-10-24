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

#pragma once

#include "Common.h"
#include "Entities/ObjectGuid.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

class Map;
class Player;

/**
 * A short, human-readable description of what is happening around a group, for
 * the LLM chat feature: who is in the party, how hurt they are, what is being
 * fought, what is close by, and what just happened (a death, a ding, the player
 * going AFK, a boss going down).
 *
 * Design notes:
 *  - optional and off by default (AiPlayerbot.LLMLiveContext);
 *  - every value is read from the live world, never from the database, so it is
 *    as current as the chat request itself;
 *  - the block is rebuilt at most once a second from an existing playerbot
 *    update tick: no threads, no timers, no packet handling;
 *  - the same text can be written to AiPlayerbot.LLMLiveContextFile for external
 *    tools that want the group's state without waiting for a chat request.
 *
 * It never changes game state and never sends anything to a player.
 */
class PlayerbotLLMContext
{
public:
    static PlayerbotLLMContext& Instance();

    // Sample the group this master is leading. Cheap; throttled internally.
    // World thread only: it walks the group and the grid around the master.
    void Update(Player* master);

    // The block for one bot's prompt: second person ("you are ...").
    std::string Describe(Player* bot) const;

    // The same situation as the snapshot written to LLMLiveContextFile, one
    // record per line:
    //
    //   at=<epoch>          map=<map id>
    //   combat=0|1          target=<name>|<level>|<health pct>
    //   member=<name>|bot|player|<level>|<class>|<health pct>|<yards>|<dead>
    //   nearby=<name>|<level>|elite|normal|<yards>
    //   event=<epoch>|<kind>|<text>
    //   afk=0|1             taxi=0|1
    //
    // Every field is a single token (no separator, no newline), so a reader can
    // split on the separators without escaping rules.
    std::string DescribeGroup(Player* master) const;

    // Write DescribeGroup() to LLMLiveContextFile (no-op when it is empty).
    void WriteFile(Player* master) const;

private:
    struct Member
    {
        ObjectGuid guid;
        std::string name;
        uint32 level = 0;
        uint8 cls = 0;
        float healthPct = 0.0f;
        float distance = 0.0f;
        bool bot = false;
        bool dead = false;
    };

    struct NearbyUnit
    {
        ObjectGuid guid;
        std::string name;
        uint32 level = 0;
        float distance = 0.0f;
        bool elite = false;
    };

    struct Event
    {
        time_t at = 0;
        std::string kind;
        std::string text;
    };

    struct Snapshot
    {
        time_t at = 0;
        time_t afkSince = 0;
        uint32 mapId = 0;
        bool inCombat = false;
        bool afk = false;
        bool taxi = false;
        std::string target;
        uint32 targetLevel = 0;
        float targetHealthPct = 0.0f;
        std::vector<Member> members;
        std::vector<NearbyUnit> nearby;
        std::vector<Event> events;
    };

    PlayerbotLLMContext() = default;

    PlayerbotLLMContext(PlayerbotLLMContext const&) = delete;
    PlayerbotLLMContext& operator=(PlayerbotLLMContext const&) = delete;

    void AppendSelf(std::ostringstream& out, Snapshot const& snapshot,
                    ObjectGuid bot) const;
    void AppendCombat(std::ostringstream& out, Snapshot const& snapshot) const;
    void AppendParty(std::ostringstream& out, Snapshot const& snapshot,
                     ObjectGuid bot) const;
    void AppendNearby(std::ostringstream& out, Snapshot const& snapshot) const;
    void AppendEvents(std::ostringstream& out, Snapshot const& snapshot, time_t now) const;
    void AppendStates(std::ostringstream& out, Snapshot const& snapshot) const;

    void Collect(Player* master, Snapshot& out);
    void CollectNearby(Player* master, Snapshot& out);
    void CollectEvents(Map* map, Snapshot const& before, Snapshot& after) const;
    void WriteText(std::string const& path, std::string const& text) const;

    Snapshot const* Find(Player* bot) const;

    // master guid (low) -> what the world looked like a moment ago
    std::map<uint32, Snapshot> m_snapshots;
    time_t m_lastUpdate = 0;
};

#define sPlayerbotLLMContext PlayerbotLLMContext::Instance()
