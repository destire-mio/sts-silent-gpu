#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct DuVuDoll : RelicHooks {
    static SPIRE_FN void setCounter(Game& g, int relic, int counter) { g.run.relics[relic].counter = counter; }
    static SPIRE_FN void onMasterDeckChange(Game& g, int relic) {
        int count = 0;
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_data(g.run.master_deck[i].id).type == CardType::Curse) ++count;
        setCounter(g, relic, count);
    }
    static SPIRE_FN void onEquip(Game& g, int relic) { onMasterDeckChange(g, relic); }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        if (g.run.relics[relic].counter > 0) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.power = PowerId::Strength;
            e.amount = g.run.relics[relic].counter;
            add_top(g, e);
            queue_relic_above(g, relic, player_target, true);
        }
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
};
}
