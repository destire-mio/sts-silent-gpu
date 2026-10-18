#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_per_attack_played.hpp"

namespace spire::units::cards {
struct Finisher : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 6, -1, -1, 2, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        DamageInfo info{};
        info.source = player_target;
        info.amount = info.info_base = g.cards[card].damage;
        add_bottom(g, actions::DamagePerAttackPlayed::construct(target, info));
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        CardHooks::applyPowers(g, card);
    }
    // Finisher.onMoveToDiscard only changes excluded description text.
    static SPIRE_FN void onMoveToDiscard(Game&, int) {}
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
