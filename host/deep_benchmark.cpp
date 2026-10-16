#include "core/engine.hpp"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Replay {
    int64_t seed;
    std::vector<spire::Action> actions;
};

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::stoi(argv[1]) : 100;
    SPIRE_ASSERT(rounds > 0);
    std::ifstream input(argc > 2 ? argv[2] : SPIRE_DEEP_ACTIONS);
    SPIRE_ASSERT(input.is_open());
    std::vector<Replay> replays;
    int64_t seed;
    int count;
    while (input >> seed >> count) {
        Replay replay{seed, {}};
        for (int i = 0; i < count; ++i) {
            int kind, index, target;
            input >> kind >> index >> target;
            SPIRE_ASSERT(input.good());
            replay.actions.push_back({static_cast<spire::ActionKind>(kind), index, target});
        }
        replays.push_back(std::move(replay));
    }
    SPIRE_ASSERT(input.eof() && !replays.empty());
    spire::Game game;
    for (const auto& replay : replays) {
        spire::reset(game, replay.seed);
        for (const auto& action : replay.actions) {
            spire::ActionSet legal;
            spire::legal_actions(game, legal);
            bool found = false;
            for (int i = 0; i < legal.size; ++i)
                found |= legal[i].kind == action.kind && legal[i].index == action.index && legal[i].target == action.target;
            SPIRE_ASSERT(found);
            spire::step(game, action);
        }
        SPIRE_ASSERT(game.screen == spire::Screen::Death || game.screen == spire::Screen::Victory);
    }
    uint64_t decisions = 0;
    uint64_t checksum = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (int round = 0; round < rounds; ++round) {
        for (const auto& replay : replays) {
            spire::reset(game, replay.seed);
            for (const auto& action : replay.actions) spire::step(game, action);
            decisions += replay.actions.size();
            checksum += game.run.floor + game.player.creature.hp + static_cast<int>(game.screen);
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << "benchmark=deep_replay steps=" << decisions << " games=" << replays.size() * rounds
              << " threads=1 game_bytes=" << sizeof(spire::Game) << " checksum=" << checksum
              << " seconds=" << std::fixed << std::setprecision(6) << seconds
              << " steps_per_second=" << decisions / seconds << '\n';
}
