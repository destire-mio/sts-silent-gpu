#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::cards {
struct Intimidate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::AllEnemies, 0, -1, -1, 1, 0, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect sound{};
        sound.kind = EffectKind::SFX;
        add_bottom(g, sound);
        add_bottom(g, actions::VFX::make(player_target));
        for (int i = 0; i < g.monster_order.size; ++i)
            queue_power(g, player_target, g.monster_order[i], PowerId::Weak, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
