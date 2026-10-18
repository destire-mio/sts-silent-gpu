#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct DieDieDie : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 13, -1, -1, 4, 0, 0, true, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::VFX::make());
        Effect shake{}; shake.kind = EffectKind::ShakeScreen; add_bottom(g, shake);
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
