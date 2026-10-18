#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FlyingSleeves : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 4, -1, -1, 2, 0, 0, false, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 0; i < 2; ++i) {
            if (target != no_target) {
                Effect sound{};
                sound.kind = EffectKind::SFX;
                sound.source = sound.target = no_target;
                add_bottom(g, sound);
                add_bottom(g, sound);
                Effect visual{};
                visual.kind = EffectKind::VFX;
                visual.source = visual.target = no_target;
                add_bottom(g, visual);
            }
            queue_card_damage(g, card, target);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
        }
    }
};
}
