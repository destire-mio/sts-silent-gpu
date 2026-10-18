#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"
namespace spire::units::cards {
struct ThunderClap : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 4, -1, -1, 3, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::SFX;
        e.source = e.target = no_target;
        add_bottom(g, e);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            const Creature& monster = g.monsters[owner].creature;
            if (monster.dying || monster.half_dead || monster.escaped) continue;
            e.kind = EffectKind::VFX;
            add_bottom(g, e);
        }
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        for (int i = 0; i < g.monster_order.size; ++i)
            queue_power(g, player_target, g.monster_order[i], PowerId::Vulnerable, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
