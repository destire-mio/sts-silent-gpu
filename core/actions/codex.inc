#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Codex : ActionHooks {
    SPIRE_STATE(Codex, bool retrieveCard; bool selecting;)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (monsters_basically_dead(g)) return;
        if (!e.payload.state.Codex.selecting) {
            g.reward_cards.clear();
            while (g.reward_cards.size != 3) {
                const CardId name = random_card_in_combat(g, -1, false);
                bool duplicate = false;
                for (int i = 0; i < g.reward_cards.size; ++i)
                    if (g.cards[g.reward_cards[i]].id == name) duplicate = true;
                if (duplicate) continue;
                g.reward_cards.push_back(create_card(g, make_card(name)));
            }
            g.discovery_card = -1;
            g.reward_skippable = true;
            g.screen = Screen::CardReward;
            e.payload.state.Codex.selecting = true;
            return;
        }
        if (!e.payload.state.Codex.retrieveCard) {
            if (g.discovery_card >= 0) {
                Card card = card_makeStatEquivalentCopy(g, g.discovery_card);
                const CardType type = card_data(card.id).type;
                if (type != CardType::Curse && type != CardType::Status && has_power(g, g.player.creature, PowerId::MasterReality))
                    card_upgrade(card);
                const int index = create_card(g, card);
                if (g.draw.empty()) g.draw.push_back(index);
                else g.draw.insert(g.rng.card_random.random(g.draw.size - 1), index);
                // ShowCardAndAddToDrawPileEffect.update starts a Soul, including during hand selection.
                mark_card_move(g, index);
                g.discovery_card = -1;
            }
            e.payload.state.Codex.retrieveCard = true;
        }
    }
};
}
