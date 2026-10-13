#pragma once
#include "../api.hpp"
#include "../actions/gain_energy.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::stances {
struct Wrath {
    static SPIRE_FN float damageGive(float value, DamageType type) { return type == DamageType::Normal ? value * 2.0f : value; }
    static SPIRE_FN float damageReceive(float value, DamageType type) { return type == DamageType::Normal ? value * 2.0f : value; }
};
}
