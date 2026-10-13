#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Anger : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect&) {
        if (card_def(g.cards[card].id).type == CardType::Skill) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Strength;
            e.amount = p.amount;
            add_top(g, e);
        }
    }
};
}
