#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct TinyHouse : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        Pile upgradeable{};
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_canUpgrade(g, -i - 2)) upgradeable.push_back(i);
        shuffle(upgradeable.values, upgradeable.size, g.rng.misc);
        if (!upgradeable.empty()) card_upgrade(g.run.master_deck[upgradeable[0]]);
        increase_max_hp(g, 5);
        add_gold_reward(g, 50);
        add_potion_reward(g, potion_from_pool(g.rng.misc));
        open_room_card_rewards(g);
    }
};
}
