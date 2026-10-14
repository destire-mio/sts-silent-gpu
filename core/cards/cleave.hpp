#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct Cleave : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 8, -1, -1, 3, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect sfx{};
        sfx.kind = EffectKind::SFX;
        sfx.source = sfx.target = no_target;
        add_bottom(g, sfx);
        Effect vfx{};
        vfx.kind = EffectKind::VFX;
        vfx.source = vfx.target = player_target;
        add_bottom(g, vfx);
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
