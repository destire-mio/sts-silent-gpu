#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct Conclude : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 12, -1, -1, 4, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect sound{};
        sound.kind = EffectKind::SFX;
        sound.source = sound.target = no_target;
        add_bottom(g, sound);
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = player_target;
        add_bottom(g, visual);
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        Effect end{};
        end.kind = EffectKind::PressEndTurnButton;
        end.source = end.target = no_target;
        add_bottom(g, end);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 4;
        }
    }
};
}
