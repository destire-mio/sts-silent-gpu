#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
    Game game{};
    game.run.act = 2; game.player.creature.hp = 12;
    require(!event_canSpawn(game, EventId::KnowingSkull), "Knowing Skull excludes 12 HP");
    game.player.creature.hp = 13;
    require(event_canSpawn(game, EventId::KnowingSkull), "Knowing Skull admits 13 HP");
    game.run.gold = 49;
    require(!event_canSpawn(game, EventId::TheJoust), "The Joust excludes 49 gold");
    game.run.gold = 50;
    require(event_canSpawn(game, EventId::TheJoust), "The Joust admits 50 gold");
    const int acts[]{1, 3, 4};
    for (int act : acts) {
        game.run.act = act;
        require(!event_canSpawn(game, EventId::KnowingSkull) && !event_canSpawn(game, EventId::TheJoust), "city-only one-time events");
    }
    game.run.floor = 6;
    require(!event_canSpawn(game, EventId::DeadAdventurer), "Dead Adventurer excludes floor 6");
    game.run.floor = 7;
    require(event_canSpawn(game, EventId::DeadAdventurer), "Dead Adventurer admits floor 7");
    game.event.id = EventId::KnowingSkull; event_initialize_state(game.event);
    game.player.creature.hp = game.player.creature.max_hp = 1000;
    units::events::KnowingSkull::enter(game);
    units::events::KnowingSkull::choose(game, 0);
    for (int i = 0; i < 16; ++i) units::events::KnowingSkull::choose(game, 1);
    require(game.event.state.KnowingSkull.count == 16 && game.event.state.KnowingSkull.goldEarned == 1440 && game.player.creature.hp == 784, "the complete reward-history budget remains usable");
    const pid_t child = fork();
    require(child >= 0, "fork");
    if (child == 0) {
        const rlimit limit{0, 0}; setrlimit(RLIMIT_CORE, &limit);
        units::events::KnowingSkull::choose(game, 1);
        _exit(0);
    }
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP), "reward-history overflow aborts in release builds");
}
