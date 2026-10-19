#include "core/engine.hpp"
#include "core/power_lifetime.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void long_combat() {
    for (int seed : {101, 102, 103}) {
        Game g{};
        reset(g, seed);
        CombatSetup setup{};
        setup.encounter = EncounterId::SnakePlant;
        setup.hp = setup.max_hp = 100000;
        for (CardId id : {CardId::Flex, CardId::DodgeAndRoll, CardId::Outmaneuver, CardId::Blur})
            setup.deck.push_back(make_card(id));
        start_combat(g, setup);
        for (int turn = 1; turn <= 180; ++turn) {
            require(g.turn == turn && g.screen == Screen::Combat, "long combat reaches each player turn");
            while (!g.hand.empty()) {
                ActionSet actions{};
                legal_actions(g, actions);
                require(actions[0].kind == ActionKind::Play, "churn card is legal");
                step(g, actions[0]);
            }
            step(g, {ActionKind::EndTurn});

        }
        int count = 0;
        for (uint64_t word : g.power_objects.used) count += __builtin_popcountll(word);
        require(count < 16, "removed powers do not accumulate across turns");
        require(g.turn == 181 && g.player.creature.hp > 0, "180 turns of legal power churn survive");
    }
}

static void queued_power_identity() {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    apply_power(g, player_target, player_target, PowerId::Strength, 7);
    const int slot = g.player.creature.powers.index(g.power_objects, 0);
    const Power original = g.power_objects[slot];
    add_bottom(g, units::actions::ReducePower::construct(player_target, player_target, original, 2));
    remove_power(g, player_target, PowerId::Strength, original.instance);
    collect_powers(g);
    require(g.power_objects.contains(slot), "queued reduction retains the detached object");
    apply_power(g, player_target, player_target, PowerId::Strength, 11);
    run_until_decision(g);
    require(power_instance(g, original.instance).amount == 5 &&
            power_amount(g, g.player.creature, PowerId::Strength) == 11,
            "reduction changes the detached instance without changing its replacement");
    collect_powers(g);
    require(!g.power_objects.contains(slot), "completed reduction releases the detached power root");
    apply_power(g, player_target, player_target, PowerId::Dexterity, 3);
    require(g.power_objects.contains(slot) && g.power_objects[slot].instance != original.instance,
            "reused slot receives a distinct identity");
    Game copy = g;
    remove_power(copy, player_target, PowerId::Dexterity);
    collect_powers(copy);
    require(g.power_objects.contains(slot) && !copy.power_objects.contains(slot), "Game copies own independent power pools");
}

static void indirect_power_roots() {
    Game g{};
    Monster m{};
    Power power{PowerId::Strength, 7}; power.instance = 1;
    const int slot = g.power_objects.add(power);
    m.creature.powers.push_back(g.power_objects, slot);
    g.monsters.push_back(m);
    Effect e{}; e.kind = EffectKind::Block; e.source = 0;
    const int action = g.action_objects.add(e);
    collect_powers(g);
    require(g.power_objects.contains(slot), "an action source retains its creature's powers without a room-list reference");
    g.action_objects.release(action);
    g.manager.card_queue.push_back({-1, 0});
    collect_powers(g);
    require(g.power_objects.contains(slot), "a card queue target retains its creature's powers");
    g.manager.card_queue.clear();
    collect_powers(g);
    require(!g.power_objects.contains(slot), "unreferenced creature powers are reclaimed");
}

int main() {
    long_combat();
    queued_power_identity();
    indirect_power_roots();
    std::printf("power lifetime: 3 x 180 turns and detached queued identity passed; sizeof(Game)=%zu\n", sizeof(Game));
}
