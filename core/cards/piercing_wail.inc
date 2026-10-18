#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::cards {
struct PiercingWail : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::AllEnemies, 1, -1, -1, 6, 0, 0, 2, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect sound{}; sound.kind = EffectKind::SFX; add_bottom(g, sound);
        add_bottom(g, actions::VFX::make(player_target));
        for (int i = 0; i < g.monster_order.size; ++i)
            queue_power(g, player_target, g.monster_order[i], PowerId::Strength, -g.cards[card].magic);
        // PiercingWail.use checks Artifact before either queued power is applied.
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            if (!has_power(g, g.monsters[owner].creature, PowerId::Artifact))
                queue_power(g, player_target, owner, PowerId::GainStrength, g.cards[card].magic);
        }
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
