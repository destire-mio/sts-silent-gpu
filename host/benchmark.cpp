#include "core/engine.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char** argv) {
    const std::uint64_t requested = argc > 1 ? std::stoull(argv[1]) : 100000;
    SPIRE_ASSERT(requested > 0);
    spire::Game game;
    std::mt19937_64 policy(1);
    spire::CombatSetup setup;
    for (int i = 0; i < 5; ++i) setup.deck.push_back(spire::make_card(spire::CardId::Strike_G));
    for (int i = 0; i < 5; ++i) setup.deck.push_back(spire::make_card(spire::CardId::Defend_G));
    setup.deck.push_back(spire::make_card(spire::CardId::Survivor));
    setup.deck.push_back(spire::make_card(spire::CardId::Neutralize));
    setup.deck.push_back(spire::make_card(spire::CardId::AscendersBane));
    setup.relics.push_back({spire::RelicId::SnakeRing, -1});
    setup.hp = 59;
    setup.max_hp = 66;
    std::uint64_t completed = 0;
    std::uint64_t games = 0;
    const auto begin = std::chrono::steady_clock::now();
    while (completed < requested) {
        spire::reset(game, games++);
        setup.encounter = static_cast<spire::EncounterId>((games - 1) % static_cast<int>(spire::EncounterId::Count));
        spire::start_combat(game, setup);
        while (completed < requested && game.screen != spire::Screen::CombatReward && game.screen != spire::Screen::Death) {
            spire::ActionSet actions;
            spire::legal_actions(game, actions);
            SPIRE_ASSERT(actions.size > 0);
            spire::step(game, actions[static_cast<int>(policy() % static_cast<std::uint64_t>(actions.size))]);
            ++completed;
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << "benchmark=starter_combat_with_dungeon_reset steps=" << completed << " games=" << games << " threads=1 game_bytes=" << sizeof(spire::Game) << " seconds="
              << std::fixed << std::setprecision(6) << seconds
              << " steps_per_second=" << completed / seconds << '\n';
}
