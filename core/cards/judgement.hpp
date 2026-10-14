#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Judgement : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, 30, 0, 0, 10, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) {
            Effect visual{};
            visual.kind = EffectKind::VFX;
            visual.source = visual.target = no_target;
            add_bottom(g, visual);
            add_bottom(g, visual);
        }
        Effect e{};
        e.kind = EffectKind::Judgement;
        e.source = no_target;
        e.target = target;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 10;
            card.magic = card.base_magic;
        }
    }
};
}
