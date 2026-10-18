#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FlameBarrier : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 2, -1, 12, 4, 0, 4, 2, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        queue_block(g, player_target, g.cards[card].block);
        queue_power(g, player_target, player_target, PowerId::FlameBarrier, g.cards[card].magic, false, 0);
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
