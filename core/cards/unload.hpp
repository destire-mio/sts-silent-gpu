#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Unload : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 14, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect discard{};
        discard.kind = EffectKind::UnloadAction;
        add_bottom(g, discard);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Unload); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
