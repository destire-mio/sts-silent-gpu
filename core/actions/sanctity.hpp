#pragma once
#include "../hooks.hpp"
#include "draw.hpp"

namespace spire::units::actions {
struct Sanctity : ActionHooks {
    SPIRE_SNAPSHOT("SanctityAction.amtToDraw", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.manager.played_cards.size >= 2 &&
            card_data(g.cards[g.manager.played_cards.values[g.manager.played_cards.size - 2]].id).type ==
                CardType::Skill) {
            add_top(g, units::actions::Draw::construct(e.extra));
            Effect v{};
            v.source = v.target = no_target;
            v.kind = EffectKind::VFX;
            add_top(g, v);
            v.kind = EffectKind::SFX;
            add_top(g, v);
            v.kind = EffectKind::VFX;
            add_top(g, v);
        }
    }
};
}
