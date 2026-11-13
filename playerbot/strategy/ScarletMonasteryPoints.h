#pragma once

#include <cstdint>

namespace ai
{
    namespace scarlet_monastery
    {
        constexpr uint32_t MAP_ID = 189;

        constexpr uint32_t HEROD_ENTRY = 3975;
        constexpr uint32_t DOAN_ENTRY = 6487;

        constexpr uint32_t SPELL_WHIRLWIND = 8989;
        constexpr uint32_t SPELL_DETONATION = 9435;
        constexpr uint32_t SPELL_ARCANE_BUBBLE = 9438;

        // Whirlwind is an 8-yard aura; the extra distance keeps bots clear of
        // combat reach while they run out.
        constexpr float HEROD_WHIRLWIND_RANGE = 12.0f;

        // Centre of the Armory arena floor, measured from the collision mesh.
        // Holding Herod here keeps the fight out of the entrance and gives the
        // ranged group room to spread out on the ledges.
        constexpr float HEROD_TANK_SPOT_X = 1962.0f;
        constexpr float HEROD_TANK_SPOT_Y = -431.6f;
        constexpr float HEROD_TANK_SPOT_Z = 6.18f;
        constexpr float HEROD_TANK_SPOT_SLACK = 3.0f;

        // Verified in-game on 2026-11-08.
        constexpr float DOORWAY_X = 169.29f;
        constexpr float DOORWAY_Y = -429.07f;
        constexpr float DOORWAY_Z = 18.53f;

        // Past this X the bot is on the antechamber side of the library wall,
        // where the LOS hold is allowed to stop it.
        constexpr float DOAN_ANTECHAMBER_X = 172.0f;

        constexpr float DOAN_SAFE_X = 177.14f;
        constexpr float DOAN_SAFE_Y = -434.59f;
        constexpr float DOAN_SAFE_Z = 18.72f;

        // Detonation is a 30-yard radius that also needs line of sight from
        // Doan. The tank holds him just inside the library, in line with the
        // doorway, so the group can step out into the antechamber and hide
        // behind the wall instead of outrunning the radius. Distant ranged bots
        // need every second, so they leave on the bubble; the bot holding
        // Doan's aggro waits for the cast, which keeps the boss parked here
        // because casting freezes him. After the cast the tank returns.
        //
        // The shelter is LOS-blocked from Doan both at the tank spot and with
        // him displaced by several yards (verified against the collision mesh).
        constexpr float DOAN_TANK_SPOT_X = 165.5f;
        constexpr float DOAN_TANK_SPOT_Y = -429.0f;
        constexpr float DOAN_TANK_SPOT_Z = 18.52f;

        // Waypoint just outside the door, east of the wall corner, so bots
        // round the jamb instead of scraping along the antechamber wall.
        constexpr float DOAN_HALL_X = 176.5f;
        constexpr float DOAN_HALL_Y = -431.0f;
        constexpr float DOAN_HALL_Z = 18.53f;

        // Reposition while farther than this from the tank spot.
        constexpr float DOAN_TANK_SPOT_SLACK = 3.0f;

        // Tolerance around the doorway when deciding whether a bot is still
        // inside the library or already on the antechamber side.
        constexpr float DOORWAY_MARGIN = 1.0f;
    }
}
