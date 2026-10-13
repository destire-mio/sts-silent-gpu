#pragma once
#include "rewards.hpp"

namespace spire {
SPIRE_FN inline void enter_campfire(Game& g) {
    g.campfire = CampfireState{};
    auto& options = g.campfire.options;
    options.push_back({CampfireOption::Rest, true});
    bool upgradeable = false;
    for (int i = 0; i < g.run.master_deck.size; ++i) upgradeable |= card_canUpgrade(g, -i - 2);
    options.push_back({CampfireOption::Smith, upgradeable});
    for (int i = 0; i < g.run.relics.size; ++i) relic_addCampfireOption(g, i, options);
    for (int i = 0; i < options.size; ++i)
        for (int r = 0; r < g.run.relics.size; ++r)
            if (!relic_canUseCampfireOption(g, r, static_cast<int>(options[i].kind))) { options[i].usable = false; break; }
    if (!g.run.ruby_key) options.push_back({CampfireOption::Recall, true});
    g.room_phase = RoomPhase::Complete;
    for (int i = 0; i < options.size; ++i) if (options[i].usable) g.room_phase = RoomPhase::Incomplete;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onEnterRestRoom(g, i);
    g.screen = Screen::Room;
}

SPIRE_FN inline void choose_campfire(Game& g, int option) {
    const CampfireOption kind = g.campfire.options[option].kind;
    g.campfire.selected = kind;
    g.campfire.something_selected = true;
    switch (kind) {
    case CampfireOption::Rest: {
        int amount = static_cast<int>(g.player.creature.max_hp * 0.3f);
        for (int i = 0; i < g.run.relics.size; ++i) amount += relic_data(g.run.relics[i].id).rest_heal_bonus;
        heal_creature(g, player_target, amount);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onRest(g, i);
        g.room_phase = RoomPhase::Complete;
        for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).rest_card_reward) {
            const int reward = create_card_reward(g);
            if (reward >= 0) {
                g.reward_cards.clear();
                for (int c = 0; c < g.reward_objects[reward].cards.size; ++c) g.reward_cards.push_back(g.reward_objects[reward].cards[c]);
                g.acquisition_reward = -1;
                g.reward_skippable = true;
                g.screen = Screen::CardReward;
            }
        }
        break;
    }
    case CampfireOption::Smith:
        g.grid_cards.clear();
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
        open_master_selection(g, 1, false);
        g.grid_for_upgrade = true;
        g.grid_can_cancel = true;
        // CampfireSmithEffect.update invokes onSmith when it opens the selection screen.
        for (int i = 0; i < g.run.relics.size; ++i) relic_onSmith(g, i);
        break;
    case CampfireOption::Toke:
        master_selection_cards(g, -1);
        for (int i = g.grid_cards.size - 1; i >= 0; --i) {
            const Card& card = g.run.master_deck[g.grid_cards[i]];
            if (card.bottle_flame || card.bottle_lightning || card.bottle_tornado) g.grid_cards.erase(i);
        }
        open_master_selection(g, 1, true);
        g.grid_can_cancel = true;
        break;
    case CampfireOption::Lift:
        for (int i = 0; i < g.run.relics.size; ++i)
            if (g.run.relics[i].id == RelicId::Girya) ++g.run.relics[i].counter;
        g.room_phase = RoomPhase::Complete;
        break;
    case CampfireOption::Dig:
        g.room_rewards.clear();
        add_relic_reward(g, random_relic_tier(g));
        g.room_phase = RoomPhase::Complete;
        open_room_card_rewards(g);
        g.reward_proceed_visible = true;
        g.combat_reward_skippable = false;
        break;
    case CampfireOption::Recall:
        g.room_rewards.clear();
        g.run.ruby_key = true;
        g.room_phase = RoomPhase::Complete;
        break;
    case CampfireOption::Count: SPIRE_ASSERT(false);
    }
}

SPIRE_FN inline void finish_campfire_selection(Game& g) {
    if (g.selected_cards.empty()) return;
    if (g.campfire.selected == CampfireOption::Smith) {
        for (int i = 0; i < g.selected_cards.size; ++i) card_upgrade(g.run.master_deck[g.selected_cards[i]]);
    } else {
        SPIRE_ASSERT(g.campfire.selected == CampfireOption::Toke);
        remove_master_card(g, g.selected_cards[0]);
    }
    g.selected_cards.clear();
    g.room_phase = RoomPhase::Complete;
    g.screen = Screen::Room;
}
}
