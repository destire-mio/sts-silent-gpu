#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Cauldron : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        g.acquisition_potions.clear();
        for (int i = 0; i < 5; ++i) g.acquisition_potions.push_back(random_potion_from_pool(g, g.rng.potion));
        add_room_card_reward(g);
        g.room_rewards.clear();
        g.screen = Screen::PotionRewards;
    }
};
}
