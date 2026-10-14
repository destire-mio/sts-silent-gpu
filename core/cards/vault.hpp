#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Vault : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::All, 3, -1, -1, -1, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = no_target;
            e.target = no_target;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::SkipEnemiesTurn;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = 0;
            e.extra = 0;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::PressEndTurnButton;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = 0;
            e.extra = 0;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = card.cost_for_turn = 2;
            card.upgraded_cost = true;
        }
    }
};
}
