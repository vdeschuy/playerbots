#pragma once

class Unit;

namespace ai
{
    namespace scarlet_monastery
    {
        // True while Doan is in combat and casting Detonation. The five-second
        // cast is the window the group uses to get clear.
        bool IsCastingDetonation(Unit* doan);
    }
}
