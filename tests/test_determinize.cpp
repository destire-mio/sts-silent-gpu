#include "obs/encode.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace spire;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static Game combat(bool eye = false) {
    Game g; reset(g, 123);
    CombatSetup setup; setup.encounter = EncounterId::JawWorm;
    const CardId cards[]{CardId::Strike_G, CardId::Defend_G, CardId::Neutralize,
                         CardId::Backflip, CardId::Footwork, CardId::PoisonedStab};
    for (int i = 0; i < 24; ++i) setup.deck.push_back(make_card(cards[i % 6], i % 2));
    if (eye) setup.relics.push_back({RelicId::FrozenEye});
    start_combat(g, setup);
    step(g, {ActionKind::EndTurn});
    require(g.room_in_combat && g.screen == Screen::Combat && g.turn == 2 && g.draw.size > 5,
            "fixture is not a mid-combat decision with a draw pile");
    return g;
}
static obs::Observation observe(const Game& g) {
    obs::Observation result; obs::History history{}; ActionSet actions;
    require(!obs::encode(g, history, result.view(), actions).fault, "observation fault");
    return result;
}
static void unchanged_except_hidden(const Game& before, Game after) {
    after.rng.ai = before.rng.ai; after.rng.shuffle = before.rng.shuffle;
    after.rng.card_random = before.rng.card_random; after.rng.misc = before.rng.misc;
    after.rng.monster_hp = before.rng.monster_hp; after.rng.potion = before.rng.potion;
    after.rng.card = before.rng.card; after.unseeded = before.unseeded;
    after.draw = before.draw;
    require(std::memcmp(&before, &after, sizeof(Game)) == 0, "determinize changed other game state");
}
static void hidden_state() {
    const Game root = combat();
    Game a = root, b = root, c = root;
    determinize(a, 42); determinize(b, 42); determinize(c, 43);
    const auto before = observe(root), after = observe(a);
    require(std::memcmp(&before, &after, sizeof(before)) == 0, "observation changed");
    unchanged_except_hidden(root, a);
    require(std::memcmp(&a, &b, sizeof(Game)) == 0, "same seed did not reproduce full state");
    require(std::memcmp(&a.rng, &c.rng, sizeof(RngStreams)) != 0, "different seeds reused RNG state");
    require(std::memcmp(&root.draw, &a.draw, sizeof(Pile)) != 0, "draw order did not change");
    require(std::memcmp(&a.draw, &c.draw, sizeof(Pile)) != 0, "different seeds reused draw order");
    Pile old_cards = root.draw, new_cards = a.draw;
    std::sort(old_cards.values, old_cards.values + old_cards.size);
    std::sort(new_cards.values, new_cards.values + new_cards.size);
    require(std::memcmp(&old_cards, &new_cards, sizeof(Pile)) == 0, "draw card multiset changed");
    const Random* old_rng[]{&root.rng.ai, &root.rng.shuffle, &root.rng.card_random,
        &root.rng.misc, &root.rng.monster_hp, &root.rng.potion, &root.rng.card};
    const Random* rng[]{&a.rng.ai, &a.rng.shuffle, &a.rng.card_random,
        &a.rng.misc, &a.rng.monster_hp, &a.rng.potion, &a.rng.card};
    for (int i = 0; i < 7; ++i) {
        require(rng[i]->counter == 0, "RNG counter was not reset");
        require(std::memcmp(&rng[i]->generator, &old_rng[i]->generator, sizeof(RandomXS128)) != 0,
                "combat RNG retained its original future");
        for (int j = 0; j < i; ++j)
            require(std::memcmp(&rng[i]->generator, &rng[j]->generator, sizeof(RandomXS128)) != 0,
                    "combat streams share a seed");
    }
    require(a.unseeded.counter == 0 &&
            std::memcmp(&a.unseeded.generator, &root.unseeded.generator, sizeof(RandomXS128)) != 0,
            "unseeded combat RNG was not reset");
    Game baseline = root; step(baseline, {ActionKind::EndTurn});
    bool different = false;
    for (uint64_t seed = 1; seed <= 8; ++seed) {
        Game future = root; determinize(future, seed); step(future, {ActionKind::EndTurn});
        require(future.turn == root.turn + 1 && future.screen == Screen::Combat, "continuation failed");
        for (int i = 0; i < future.hand.size; ++i)
            different |= future.cards[future.hand[i]].id != baseline.cards[baseline.hand[i]].id;
        different |= future.monsters[future.monster_order[0]].move != baseline.monsters[baseline.monster_order[0]].move;
    }
    require(different, "determinize did not change future draws or monster moves");
}
static void frozen_eye_and_noncombat() {
    Game g = combat(true); const Game before = g;
    determinize(g, 42);
    require(std::memcmp(&before.draw, &g.draw, sizeof(Pile)) == 0, "Frozen Eye draw order changed");
    unchanged_except_hidden(before, g);
    const auto a = observe(before), b = observe(g);
    require(std::memcmp(&a, &b, sizeof(a)) == 0, "Frozen Eye observation changed");
    reset(g, 123); const Game outside = g;
    determinize(g, 42);
    require(std::memcmp(&outside, &g, sizeof(Game)) == 0, "noncombat state changed");
}
int main() { hidden_state(); frozen_eye_and_noncombat(); }
