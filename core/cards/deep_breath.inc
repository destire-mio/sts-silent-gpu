#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct DeepBreath : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 1, 0, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (!g.discard.empty()) {
            Effect shuffle{};
            shuffle.kind = EffectKind::Shuffle;
            add_bottom(g, shuffle);
            Effect group{};
            group.kind = EffectKind::ShuffleGroup;
            action_initialize_state(group);
            group.payload.state.ShuffleGroup.group = PileId::Draw;
            add_bottom(g, group);
        }
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = g.cards[card].magic;
        add_bottom(g, draw);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
