#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::potions {
struct SneckoOil : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (!g.battle_over && g.screen != Screen::None) {
            add_bottom(g, units::actions::Draw::construct(potion_potency(g, slot), true, -1, player_target));
            Effect e{};
            e.kind = EffectKind::RandomizeHandCost;
            add_bottom(g, e);
        }
    }
};
}
