#include "core/engine.hpp"
#include "core/orb_lifetime.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace spire;
static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void no_slots() {
    for (int seed : {101, 102, 103}) {
        Game g{};
        reset(g, seed);
        CombatSetup setup{};
        setup.hp = setup.max_hp = 100000;
        setup.deck.push_back(make_card(CardId::Zap));
        start_combat(g, setup);
        for (int t = 0; t < 180; ++t) {
            step(g, {ActionKind::Play, 0, -1});
            step(g, {ActionKind::EndTurn});
        }
        require(g.turn == 181 && g.orbs.channeled.empty() && g.player.max_orbs == 0,
                "zero-slot channel attempts leave no history over 180 turns");
    }
}
static void roots() {
    Game g{};
    const int first = make_orb(g, OrbKind::Frost);
    const int second = make_orb(g, OrbKind::Dark);
    const int third = make_orb(g, OrbKind::Plasma);
    const int garbage = make_orb(g, OrbKind::Lightning);
    g.orbs.slots.push_back(first);
    g.orbs.channeled.push_back(second);
    const int action = g.action_objects.add(units::actions::Channel::make(third));
    collect_orbs(g);
    require(g.orbs.objects.contains(first) && g.orbs.objects.contains(second) &&
            g.orbs.objects.contains(third) && !g.orbs.objects.contains(garbage),
            "slots, history and queued Channel retain their distinct objects");
    g.action_objects.release(action);
    collect_orbs(g);
    require(!g.orbs.objects.contains(third), "discarding Channel releases its unchanneled orb");
    const int reused = make_orb(g, OrbKind::Lightning);
    require(reused == third && g.orbs.kind(reused) == OrbKind::Lightning &&
            g.orbs.kind(second) == OrbKind::Dark, "slot reuse preserves the historical orb and updates the reused type");
}
int main() {
    no_slots(); roots();
    std::printf("orb lifetime: 3 x 180 turns and slot/history/queue roots passed; sizeof(Game)=%zu\n", sizeof(Game));
}
