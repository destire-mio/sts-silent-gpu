#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Cauldron : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        for (int i = 0; i < 5; ++i) add_potion_reward(g, random_potion_from_pool(g, g.rng.potion));
        open_room_card_rewards(g);
        for (int i = 0; i < g.screen_rewards.size; ++i)
            if (g.reward_objects[g.screen_rewards[i]].kind == RewardKind::Card) {
                g.screen_rewards.erase(i);
                break;
            }
    }
};
}
