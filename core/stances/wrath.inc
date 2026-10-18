#pragma once
#include "core/api.hpp"
#include "core/actions/gain_energy.hpp"
#include "core/actions/change_stance.hpp"
SPIRE_RULE("rule:stances.WrathStance")

namespace spire::units::stances {
struct Wrath {
    static SPIRE_FN float damageGive(float value, DamageType type) { return type == DamageType::Normal ? value * 2.0f : value; }
    static SPIRE_FN float damageReceive(float value, DamageType type) { return type == DamageType::Normal ? value * 2.0f : value; }
};
}
