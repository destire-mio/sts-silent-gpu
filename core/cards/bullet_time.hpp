#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BulletTime : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 3, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::NoDraw, 1);
        Effect e{};
        e.kind = EffectKind::ApplyBulletTime;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            const int diff = card.cost_for_turn - card.cost;
            card.cost = 2;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + diff;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
