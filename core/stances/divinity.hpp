#pragma once
#include "../api.hpp"
#include "../actions/gain_energy.hpp"
#include "../actions/change_stance.hpp"
SPIRE_RULE("rule:stances.DivinityStance")

namespace spire::units::stances {
struct Divinity {
    static SPIRE_FN float damageGive(float value, DamageType type) { return type == DamageType::Normal ? value * 3.0f : value; }
    static SPIRE_FN void onEnter(Game& g) { add_bottom(g, units::actions::GainEnergy::make(3)); }
    static SPIRE_FN void atStartOfTurn(Game& g) { add_bottom(g, units::actions::ChangeStance::make(Stance::Neutral)); }
};
}
