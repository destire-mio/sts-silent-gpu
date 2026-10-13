#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BottledFlame : RelicHooks {
    SPIRE_STATE(BottledFlame, bool cardSelected; uint16_t card;)
    SPIRE_SNAPSHOT("BottledFlame.card", master_card_uuid, relic.state.BottledFlame.card)
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.BottledFlame.cardSelected = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        master_selection_cards(g, CardType::Attack);
        if (!g.grid_cards.empty()) {
            g.run.relics[relic].state.BottledFlame.cardSelected = false;
            open_master_selection(g, 1, false);
        }
    }
    static SPIRE_FN void update(Game& g, int relic) {
        auto& state = g.run.relics[relic].state.BottledFlame;
        if (!state.cardSelected && !g.selected_cards.empty()) {
            state.cardSelected = true;
            Card& card = g.run.master_deck[g.selected_cards[0]];
            state.card = card.uuid;
            card.bottle_flame = true;
            g.selected_cards.clear();
        }
    }
    static SPIRE_FN bool referencesMasterCard(const Game& g, int relic, uint32_t uuid) { return g.run.relics[relic].state.BottledFlame.card == uuid; }
    static SPIRE_FN Card getCard(const Game& g, int relic) {
        return make_card(master_card_by_uuid(g, g.run.relics[relic].state.BottledFlame.card).id);
    }
    static SPIRE_FN void onUnequip(Game& g, int relic) {
        const uint32_t uuid = g.run.relics[relic].state.BottledFlame.card;
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (uuid != 0 && g.run.master_deck[i].uuid == uuid) g.run.master_deck[i].bottle_flame = false;
    }
    static SPIRE_FN void atBattleStart(Game& g, int relic) { queue_relic_above(g, relic, player_target, true); }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& c = g.run.master_deck[i];
            if (card_def(c.id).type == CardType::Attack && card_rarity(c.id) != CardRarity::Basic) return true;
        }
        return false;
    }
};
}
