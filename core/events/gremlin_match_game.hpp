#pragma once
#include "../hooks.hpp"
#include "../card_pool.hpp"

namespace spire::units::events {
struct GremlinMatchGame : EventHooks {
    SPIRE_STATE(GremlinMatchGame, uint8_t screen; uint8_t count; uint8_t cards[12]; uint8_t matched[5]; uint8_t cardsMatched; uint8_t attemptCount; int16_t chosen; bool cardFlipped; bool gameDone; bool cleanUpCalled;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.GremlinMatchGame", true}; }
    SPIRE_SNAPSHOT("GremlinMatchGame.screen", scalar, event.state.GremlinMatchGame.screen == 0 ? "INTRO" : event.state.GremlinMatchGame.screen == 1 ? "RULE_EXPLANATION" : event.state.GremlinMatchGame.screen == 2 ? "PLAY" : "COMPLETE")
    SPIRE_SNAPSHOT("GremlinMatchGame.chosenCard", card, event.state.GremlinMatchGame.chosen)
    SPIRE_SNAPSHOT("GremlinMatchGame.matchedCards", scalar, [&]() -> pybind11::object { pybind11::list result; const auto& s = event.state.GremlinMatchGame; for (int i = 0; i < s.cardsMatched; ++i) result.append(card_names[static_cast<int>(graph.game.cards[s.matched[i]].id)]); return result; }())
    SPIRE_SNAPSHOT("GremlinMatchGame.cards", scalar, pybind11::object(graph.object({46, 0, 0}, "cards.CardGroup", [&](const std::string& key) -> pybind11::object { if (key == "CardGroup.type") return graph.scalar("UNSPECIFIED"); if (key == "CardGroup.group") { pybind11::list result; const auto& s = event.state.GremlinMatchGame; for (int i = 0; i < s.count; ++i) result.append(graph.card(s.cards[i])); return result; } return {}; })))
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.GremlinMatchGame;
        s.attemptCount = 5; s.chosen = -1;
        const CardPoolEntry* pools[] = {rare_card_pool, uncommon_card_pool, common_card_pool};
        const int sizes[] = {sizeof(rare_card_pool) / sizeof(*rare_card_pool), sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool), sizeof(common_card_pool) / sizeof(*common_card_pool)};
        for (int i = 0; i < 3; ++i) s.cards[s.count++] = create_card(g, make_card(card_id(pools[i][g.rng.card.random(sizes[i] - 1)].id)));
        for (int i = 0; i < 2; ++i) s.cards[s.count++] = create_card(g, make_card(random_curse(g)));
        s.cards[s.count++] = create_card(g, make_card(CardId::Neutralize));
        for (int i = 0; i < 6; ++i) {
            preview_card_obtain(g, g.cards[s.cards[i]]);
            s.cards[s.count++] = create_card(g, card_makeStatEquivalentCopy(g, s.cards[i]));
        }
        shuffle(s.cards, s.count, g.rng.misc);
        event_options(g, 1);
    }
    static SPIRE_FN void choices(const Game& g, EventChoices& choices) {
        const auto& s = g.event.state.GremlinMatchGame;
        if (s.screen != 2) { event_dialog_choices(g, choices); return; }
        for (int i = 0; i < s.count; ++i) if (g.cards[s.cards[i]].is_flipped) choices.push_back({EventChoiceKind::Card, s.cards[i]});
    }
    static SPIRE_FN void choose(Game& g, int choice) {
        auto& s = g.event.state.GremlinMatchGame;
        if (s.screen == 0) { s.screen = 1; return; }
        if (s.screen == 1) {
            s.screen = 2; event_options(g, 0);
            for (int i = 0; i < s.count; ++i) g.cards[s.cards[i]].is_flipped = true;
            return;
        }
        if (s.screen == 3) { open_map(g); return; }
        g.cards[choice].is_flipped = false;
        if (!s.cardFlipped) { s.cardFlipped = true; s.chosen = choice; return; }
        s.cardFlipped = false;
        if (g.cards[s.chosen].id == g.cards[choice].id) {
            s.matched[s.cardsMatched++] = s.chosen;
            queue_card_obtain(g, make_card(g.cards[s.chosen].id));
            for (int i = s.count - 1; i >= 0; --i) if (s.cards[i] == s.chosen || s.cards[i] == choice) {
                for (int j = i; j + 1 < s.count; ++j) s.cards[j] = s.cards[j + 1];
                --s.count;
            }
        } else { g.cards[s.chosen].is_flipped = true; g.cards[choice].is_flipped = true; }
        s.chosen = -1;
        if (--s.attemptCount == 0) { s.gameDone = s.cleanUpCalled = true; s.screen = 3; event_options(g, 1); }
    }
};
}
