#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"
namespace spire::units::cards {
struct Warcry : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 1, 0, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::VFX;
        e.source = e.target = player_target;
        add_bottom(g, e);
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
        e = Effect{};
        e.kind = EffectKind::PutOnDeck;
        action_initialize_state(e);
        e.amount = 1;
        add_bottom(g, e);
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
