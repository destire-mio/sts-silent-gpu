#include "host/batch.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
    Game game{};
    reset(game, 101);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::ForceField));
    start_combat(game, setup);
    const int power = create_card(game, make_card(CardId::Footwork));
    game.manager.played_cards.record(power);
    const Card generated = make_card(game, CardId::ForceField);
    require(generated.cost == 3 && generated.cost_for_turn == 3,
            "a newly constructed card reads this combat's played powers");
    const int source = create_card(game, make_card(CardId::ForceField));
    game.cards[source].cost = 2;
    game.cards[source].cost_for_turn = 0;
    const Card fresh = card_makeCopy(game, source);
    const Card equivalent = card_makeStatEquivalentCopy(game, source);
    require(fresh.cost == 3 && fresh.cost_for_turn == 3 &&
            equivalent.cost == 2 && equivalent.cost_for_turn == 0,
            "fresh and stat-equivalent copies preserve their Java cost semantics");
    for (int i = 0; i < 5; ++i) game.manager.played_cards.record(power);
    require(make_card(game, CardId::ForceField).cost == 0, "constructor cost reduction stops at zero");
    Game other{};
    require(make_card(other, CardId::ForceField).cost == 4,
            "new cards read only their owning Game's history");
    Effect eyes{};
    eyes.kind = EffectKind::ForTheEyes;
    eyes.extra = no_target;
    units::actions::ForTheEyes::execute(game, eyes);
    require(game.manager.actions.empty(), "a missing intent target queues no Weak application");
    Game thieves{};
    reset(thieves, 101);
    CombatSetup theft{};
    theft.encounter = EncounterId::TwoThieves;
    for (int i = 0; i < 8; ++i) theft.deck.push_back(make_card(CardId::FTL));
    start_combat(thieves, theft);
    step(thieves, {ActionKind::EndTurn});
    const int player_gold = thieves.run.gold;
    const int monster_gold = thieves.monsters[1].creature.gold;
    require(monster_gold > 0, "the second thief holds stolen gold");
    step(thieves, {ActionKind::Play, 0, 1});
    require(thieves.run.gold == player_gold && thieves.monsters[1].creature.gold == monster_gold,
            "FTL damage does not reclaim stolen gold from a nonzero target index");
    Card temporary = make_card(CardId::DoubleEnergy);
    temporary.cost_for_turn = 3;
    card_upgrade(temporary);
    require(temporary.cost == 0 && temporary.cost_for_turn == 2,
            "upgrading Double Energy preserves the temporary cost difference");
    Card free = make_card(CardId::Fusion);
    free.cost_for_turn = 0;
    card_upgrade(free);
    require(free.cost == 1 && free.cost_for_turn == 0,
            "upgrading Fusion preserves a free turn cost");
    static_assert(sizeof(Game) <= 35552 + 128);
}
