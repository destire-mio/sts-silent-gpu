#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::cards {
struct HeavyBlade : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 14, -1, 3, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) add_bottom(g, actions::VFX::make());
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        const int index = find_power(g, g.player.creature, PowerId::Strength);
        if (index >= 0) g.player.creature.powers.at(g.power_objects, index).amount *= get_card(g, card).magic;
        apply_card_powers(g, card);
        if (index >= 0) g.player.creature.powers.at(g.power_objects, index).amount /= get_card(g, card).magic;
    }
    static SPIRE_FN void calculateCardDamage(Game& g, int card, int target) {
        const int index = find_power(g, g.player.creature, PowerId::Strength);
        if (index >= 0) g.player.creature.powers.at(g.power_objects, index).amount *= get_card(g, card).magic;
        calculate_card_damage(g, card, target);
        if (index >= 0) g.player.creature.powers.at(g.power_objects, index).amount /= get_card(g, card).magic;
    }
};
}
