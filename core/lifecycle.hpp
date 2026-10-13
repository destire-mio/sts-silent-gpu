#pragma once
#include "cards.hpp"

namespace spire {
SPIRE_FN inline void gain_gold(Game& g, int amount) {
    if (amount <= 0) return;
    g.run.gold += amount;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onGainGold(g, i);
}
SPIRE_FN inline void lose_gold(Game& g, int amount) {
    if (amount <= 0) return;
    g.run.gold = max(0, g.run.gold - amount);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onLoseGold(g, i);
}
SPIRE_FN inline void spend_gold(Game& g, int amount) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onSpendGold(g, i);
    lose_gold(g, amount);
}
SPIRE_FN inline void equip_relic(Game& g, RelicId id) {
    const int index = g.run.relics.size;
    g.run.relics.push_back({id, relic_data(id).counter});
    relic_initialize_state(g.run.relics[index]);
    relic_onEquip(g, index);
}
SPIRE_FN inline bool unequip_relic(Game& g, RelicId id) {
    int remove_index = -1;
    for (int i = 0; i < g.run.relics.size; ++i) {
        if (g.run.relics[i].id == id) {
            relic_onUnequip(g, i);
            remove_index = i;
        }
    }
    if (remove_index < 0) return false;
    g.run.relics.erase(remove_index);
    return true;
}
SPIRE_FN inline void preview_card_obtain(Game& g, Card& card) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPreviewObtainCard(g, i, card);
}
SPIRE_FN inline void obtain_card(Game& g, Card card) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onObtainCard(g, i, card);
    g.run.master_deck.push_back(card);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void remove_master_card(Game& g, int index) {
    Card removed = g.run.master_deck.erase(index);
    card_onRemoveFromMasterDeck(g, removed);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void on_victory(Game& g) {
    if (!g.player.creature.dying) {
        for (int i = 0; i < g.run.relics.size; ++i) relic_onVictory(g, i);
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onVictory(g, powers[i], player_target);
    }
}
SPIRE_FN inline void gain_energy(Game& g, int amount) {
    g.player.energy = min(999, g.player.energy + amount);
}
}
