#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BottledLightning : RelicHooks {
    SPIRE_STATE(BottledLightning, bool cardSelected; uint32_t card;)
    SPIRE_SNAPSHOT("BottledLightning.card", master_card_uuid, relic.state.BottledLightning.card)
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.BottledLightning.cardSelected = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        master_selection_cards(g, CardType::Skill);
        if (!g.grid_cards.empty()) {
            g.run.relics[relic].state.BottledLightning.cardSelected = false;
            open_master_selection(g, 1, false);
        }
    }
    static SPIRE_FN void update(Game& g, int relic) {
        auto& state = g.run.relics[relic].state.BottledLightning;
        if (!state.cardSelected && !g.selected_cards.empty()) {
            state.cardSelected = true;
            Card& card = g.run.master_deck[g.selected_cards[0]];
            state.card = card.uuid;
            card.bottle_lightning = true;
            g.selected_cards.clear();
        }
    }
    static SPIRE_FN bool referencesMasterCard(const Game& g, int relic, uint32_t uuid) { return g.run.relics[relic].state.BottledLightning.card == uuid; }
    static SPIRE_FN Card getCard(const Game& g, int relic) {
        return make_card(master_card_by_uuid(g, g.run.relics[relic].state.BottledLightning.card).id);
    }
    static SPIRE_FN void onUnequip(Game& g, int relic) {
        const uint32_t uuid = g.run.relics[relic].state.BottledLightning.card;
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (uuid != 0 && g.run.master_deck[i].uuid == uuid) g.run.master_deck[i].bottle_lightning = false;
    }
    static SPIRE_FN void atBattleStart(Game& g, int relic) { queue_relic_above(g, relic, player_target, true); }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& c = g.run.master_deck[i];
            if (card_def(c.id).type == CardType::Skill && card_rarity(c.id) != CardRarity::Basic) return true;
        }
        return false;
    }
};
}
