#include "core/engine.hpp"
#include "core/action_lifetime.hpp"
#include "core/power_lifetime.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;
using namespace spire::units::actions;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void discarded_follow_ups() {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    for (int i = 0; i < 300; ++i) {
        const int amounts[]{1, 7, 3};
        const Effect damage = DamageAllEnemies::make(g, player_target, amounts, 3);
        add_bottom(g, Draw::construct(g, 1, Draw::construct(g, 1, damage)));
        clear_actions(g, g.manager.actions);
        collect_actions(g);
        for (uint64_t word : g.action_objects.used) require(word == 0, "discarded Draw chains release held children");
        for (uint64_t word : g.damage_exceptions.used) require(word == 0, "discarded follow-ups release owned damage rows");
    }
}

static void shared_follow_up() {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    const int child = hold_action(g, GainEnergy::make(2));
    const int first = hold_action(g, Draw::construct(0, true, child));
    const int second = hold_action(g, Draw::construct(0, true, child));
    g.manager.actions.push_back(first);
    g.manager.pre_turn_actions.push_back(second);
    collect_actions(g);
    require(g.action_objects.contains(child), "both queue roots retain their shared follow-up");
    clear_actions(g, g.manager.actions);
    collect_actions(g);
    require(!g.action_objects.contains(first) && g.action_objects.contains(child), "dropping one parent preserves the other parent and child");
    g.manager.current_action = g.manager.pre_turn_actions.pop_front();
    g.manager.has_current_action = true;
    collect_actions(g);
    require(g.action_objects.contains(second) && g.action_objects.contains(child), "current action retains its follow-up");
    g.manager.has_current_action = false;
    collect_actions(g);
    for (uint64_t word : g.action_objects.used) require(word == 0, "dropping the last parent reclaims the shared child");
}

int main() {
    discarded_follow_ups();
    shared_follow_up();
    std::puts("action lifetime: 300 discarded chains, shared roots and damage release passed");
}
