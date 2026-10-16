#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Necronomicon : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    SPIRE_STATE(Necronomicon, bool activated;)
    static SPIRE_FN void initializeState(Relic& r) { r.state.Necronomicon.activated = true; }
    static SPIRE_FN void onEquip(Game& g, int) { obtain_card(g, make_card(CardId::Necronomicurse)); }
    static SPIRE_FN void onUnequip(Game& g, int) {
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            if (g.run.master_deck[i].id != CardId::Necronomicurse) continue;
            g.run.master_deck.erase(i);
            break;
        }
    }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect& use) {
        const Card& c = g.cards[card];
        if (card_data(c.id).type != CardType::Attack ||
            !((c.cost_for_turn >= 2 && !c.free_to_play_once) || (c.cost == -1 && c.energy_on_use >= 2)) ||
            !g.run.relics[relic].state.Necronomicon.activated) return;
        g.run.relics[relic].state.Necronomicon.activated = false;
        queue_relic_above(g, relic, player_target, true);
        const int copy = g.cards.add(card_makeSameInstanceOf(g, card));
        card_applyPowers(g, copy);
        g.cards[copy].purge_on_use = true;
        add_card_queue_item(g, {copy, use.target, c.energy_on_use, true, true}, true);
    }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].state.Necronomicon.activated = true; }
    static SPIRE_FN bool checkTrigger(const Game& g, int relic) { return g.run.relics[relic].state.Necronomicon.activated; }
};
}
