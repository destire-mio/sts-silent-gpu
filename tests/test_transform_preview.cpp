#include "core/engine.hpp"

using namespace spire;

int main() {
    Game game{};
    reset(game, 2000000764);
    game.run.master_deck.clear();
    game.run.master_deck.push_back(make_card(CardId::Regret));
    game.run.master_deck.push_back(make_card(CardId::Strike_G));
    game.grid_cards.push_back(0);
    game.grid_cards.push_back(1);
    Random expected = game.rng.card;
    for (int attempt = 0; attempt < 4; ++attempt) {
        open_master_selection(game, 1, false);
        game.grid_for_transform = true;
        step(game, {ActionKind::Choose, attempt == 0 ? 1 : 0, -1});
        SPIRE_ASSERT(game.grid_confirm_screen);
        SPIRE_ASSERT(game.rng.card.counter == expected.counter);
        step(game, {ActionKind::Skip, -1, -1});
        // GridCardSelectScreen.update: float32 0.1f needs seven 1/60 ticks to go below zero.
        if (attempt == 3) expected.random(9);
        SPIRE_ASSERT(!game.grid_confirm_screen);
        SPIRE_ASSERT(game.rng.card.counter == expected.counter);
        SPIRE_ASSERT(game.rng.card.generator.s0 == expected.generator.s0);
        SPIRE_ASSERT(game.rng.card.generator.s1 == expected.generator.s1);
    }
    reset(game, 2000000764);
    SPIRE_ASSERT(game.grid_transform_timer == 0.0f);
}
