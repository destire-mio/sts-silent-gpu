#pragma once
#include "core/engine.hpp"
#include <vector>

namespace spire::host {
struct Batch {
    std::vector<Game> games;

    explicit Batch(std::size_t size) : games(size) {}

    void reset(const std::vector<std::int64_t>& seeds) {
        SPIRE_ASSERT(seeds.size() == games.size());
        for (std::size_t i = 0; i < games.size(); ++i) spire::reset(games[i], seeds[i]);
    }

    std::vector<uint32_t> faults() const {
        std::vector<uint32_t> result;
        for (const auto& game : games) result.push_back(game.fault);
        return result;
    }

    void reset_at(std::size_t index, std::int64_t seed) { spire::reset(games[index], seed); }

    void step(const std::vector<Action>& actions) {
        SPIRE_ASSERT(actions.size() == games.size());
        for (std::size_t i = 0; i < games.size(); ++i) spire::step(games[i], actions[i]);
    }
};
}
