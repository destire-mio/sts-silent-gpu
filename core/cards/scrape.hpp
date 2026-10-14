#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct Scrape : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, 4, 3, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) {
            Effect v{};
            v.kind = EffectKind::VFX;
            v.source = v.target = no_target;
            add_bottom(g, v);
        }
        queue_card_damage(g, card, target);
        Effect follow{};
        follow.kind = EffectKind::ScrapeFollowUp;
        follow.source = follow.target = no_target;
        add_bottom(g, units::actions::Draw::construct(g, g.cards[card].magic, follow));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
