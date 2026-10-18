#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::cards {
struct SweepingBeam : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 6, -1, 1, 3, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect v{};
        v.kind = EffectKind::SFX;
        v.source = v.target = no_target;
        add_bottom(g, v);
        v.kind = EffectKind::VFX;
        v.source = player_target;
        add_bottom(g, v);
        add_bottom(g, units::actions::DamageAllEnemies::from_card(g, card));
        add_bottom(g, units::actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
