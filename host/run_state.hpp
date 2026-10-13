#pragma once
#include "core/engine.hpp"
#include "core/card_pool.hpp"
#include "names.hpp"
#include <string>
#include <vector>

namespace spire::host {
struct FreshRunState {
    std::vector<std::string> relicsToRemoveOnStart;
};

inline void initialize_fresh_run(FreshRunState& run) {
    run = FreshRunState{};
    // CharacterManager and CharacterSelectScreen each construct the four characters.
    for (int creation = 0; creation < 2; ++creation) {
        for (const char* relic : {"Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater"}) {
            run.relicsToRemoveOnStart.push_back(relic);
        }
    }
    run.relicsToRemoveOnStart.push_back("Ring of the Snake");
}

}
