#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Whirlwind : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, -1, 5, -1, -1, 3, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::Whirlwind;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = card;
        e.payload.state.Whirlwind.energyOnUse = g.cards[card].energy_on_use;
        e.payload.state.Whirlwind.freeToPlayOnce = g.cards[card].free_to_play_once;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
