#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Surrounded : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{}; data.rear_damage_multiplier = 1.5f; data.rear_attack_power = PowerId::BackAttack; return data; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
};
}
