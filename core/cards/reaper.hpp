#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Reaper : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::AllEnemies, 2, 4, -1, -1, 1, 0, 0, true, false, false, true};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::VFX;
        e.source = e.target = no_target;
        add_bottom(g, e);
        e.kind = EffectKind::VampireDamageAllEnemies;
        e.source = player_target;
        e.card = card;
        e.amount = g.multi_damage.get(card).at(g.damage_exceptions, 0);
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
