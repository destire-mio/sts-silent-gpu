#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"

namespace spire::units::powers {
struct Storm : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int card, Effect &) {
        if (card_data(g.cards[card].id).type == CardType::Power && p.amount > 0)
            for (int i = 0; i < p.amount; ++i)
                add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Lightning)));
    }
};
}
