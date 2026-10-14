#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct Reboot : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 4, 0, 0, 2, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::ShuffleAll;
        add_bottom(g, e);
        e = Effect{};
        e.kind = EffectKind::ShuffleGroup;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.ShuffleGroup.group = PileId::Draw;
        add_bottom(g, e);
        add_bottom(g, units::actions::Draw::construct(g.cards[card].magic));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
};
}
