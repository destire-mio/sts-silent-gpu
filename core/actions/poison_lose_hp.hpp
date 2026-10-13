#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct PoisonLoseHp : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.done = true;
        if (g.screen == Screen::Victory || g.screen == Screen::None) return;
        Creature& target = get_creature(g, e.target);
        if (target.hp > 0) {
            DamageInfo info{};
            info.source = e.source;
            info.target = e.target;
            info.amount = info.info_base = e.amount;
            info.damage_type = DamageType::HpLoss;
            creature_damage(g, info);
        }
        const int poison = find_power(g, target, "Poison");
        if (poison >= 0 && --target.powers.at(g.power_objects, poison).amount == 0) target.powers.erase(poison);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
