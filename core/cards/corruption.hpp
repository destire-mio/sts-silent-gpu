#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Corruption : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 3, -1, -1, 3, 0, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::SFX;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = 0;
            e.extra = 0;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        if (!has_power(g, g.player.creature, PowerId::Corruption))
            queue_power(g, player_target, player_target, PowerId::Corruption, -1, false, 0);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 2);
        }
    }
};
}
