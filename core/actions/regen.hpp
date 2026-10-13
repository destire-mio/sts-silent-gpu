#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Regen : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (g.screen == Screen::CombatReward || g.screen == Screen::None) return;
        Creature& target = get_creature(g, e.target);
        if (target.hp > 0) {
            heal_creature(g, e.target, e.amount);
        }
        if (e.target == player_target) {
            const int regen = find_power(g, target, "Regeneration");
            if (regen >= 0 && --target.powers.at(g.power_objects, regen).amount == 0) target.powers.erase(g.power_objects, regen);
        }
    }
};
}
