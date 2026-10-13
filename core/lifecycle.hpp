#pragma once
#include "cards.hpp"

namespace spire {
SPIRE_FN inline void gain_gold(Game& g, int amount) {
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).blocks_gold) return;
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
    relic_initialState(g, index);
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
    const CardRarity rarity = card_rarity(card.id);
    if (rarity != CardRarity::Basic && rarity != CardRarity::Special && rarity != CardRarity::Curse)
        ++g.obtained_cards[static_cast<int>(card.id)];
    for (int i = 0; i < g.run.relics.size; ++i) relic_onObtainCard(g, i, card);
    g.run.master_deck.push_back(card);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void increase_max_hp(Game& g, int amount) {
    g.player.creature.max_hp += amount;
    heal_creature(g, player_target, amount);
}
SPIRE_FN inline void queue_card_obtain(Game& g, Card card) {
    g.pending_obtains.push_back(create_card(g, card));
}
SPIRE_FN inline void settle_card_obtains(Game& g) {
    for (int i = 0; i < g.pending_obtains.size; ++i) obtain_card(g, g.cards[g.pending_obtains[i]]);
    g.pending_obtains.clear();
}
SPIRE_FN inline void open_master_selection(Game& g, int amount, bool for_purge) {
    g.screen = Screen::Grid;
    g.grid_source = PileId::MasterDeck;
    g.selection_amount = amount;
    g.selection_any_number = false;
    g.grid_for_purge = for_purge;
    g.grid_can_cancel = false;
    g.selected_cards.clear();
}
SPIRE_FN inline void queue_relic_above(Game& g, int relic, int owner, bool top) {
    Effect e{};
    e.kind = EffectKind::RelicAboveCreature;
    action_initialize_state(e);
    e.source = e.target = owner;
    e.extra = relic;
    if (top) add_top(g, e);
    else add_bottom(g, e);
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
        for (int i = 0; i < powers.size; ++i) power_onVictory(g, powers.at(g.power_objects, i), player_target);
    }
}
SPIRE_FN inline void gain_energy(Game& g, int amount) {
    g.player.energy = min(999, g.player.energy + amount);
}
}
