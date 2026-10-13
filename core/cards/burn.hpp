#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Burn : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Status, CardTarget::None, -2, -1, -1, 2, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (g.cards[card].dont_trigger_on_use_card) {
            Effect damage{};
            damage.kind = EffectKind::Damage;
            damage.source = damage.target = player_target;
            damage.amount = damage.info_base = g.cards[card].magic;
            damage.damage_type = DamageType::Thorns;
            add_bottom(g, damage);
        }
    }
    static SPIRE_FN void triggerOnEndOfTurnForPlayingCard(Game& g, int card) {
        g.cards[card].dont_trigger_on_use_card = true;
        CardQueueItem item{};
        item.card = card;
        item.energy_on_use = g.player.energy;
        item.is_end_turn_auto_play = true;
        g.manager.card_queue.push_back(item);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Burn); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
