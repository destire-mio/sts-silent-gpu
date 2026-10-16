#include "host/training_batch.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using namespace spire;
using host::TrainingBatch;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static void clone_and_isolation() {
    TrainingBatch batch(2, 2, 1000, {1, 2, 3, 4, 5, 6});
    batch.clone(0, 1);
    int32_t choices[]{0, 0};
    batch.step(choices);
    require(std::memcmp(&batch.games[0], &batch.games[1], sizeof(Game)) == 0, "same clone actions diverged");
    batch.clone(0, 1);
    const Game before = batch.games[1];
    choices[1] = -1;
    batch.step(choices);
    require(std::memcmp(&before, &batch.games[1], sizeof(Game)) == 0, "faulted clone changed game");
    require(batch.faults[0] == 0 && batch.faults[1] == static_cast<uint32_t>(obs::Fault::InvalidCandidate), "fault isolation failed");
    require(!batch.done[1] && !batch.truncated[1] && batch.rewards[1] == 0, "fault counted as terminal game outcome");
    for (int i = 0; i < obs::entity_mask_bytes; ++i) require(batch.entity_mask[obs::entity_mask_bytes + i] == 0, "fault observation remained valid");
    batch.clone(0, 1);
    require(!batch.faults[1], "clone did not replace fault status");
    batch.games[0].fault = 12345;
    choices[0] = choices[1] = 0;
    const Game latched = batch.games[0];
    batch.step(choices);
    require(batch.faults[0] == 12345 && batch.faults[1] == 0, "latched engine fault contaminated another slot");
    require(std::memcmp(&latched, &batch.games[0], sizeof(Game)) == 0, "latched game stepped");
    batch.step(choices);
    require(batch.reset_mask[0] && !batch.faults[0] && batch.rewards[0] == 0, "fault reset failed");
}
static void truncation() {
    TrainingBatch b(1, 1, 1, {1, 2});
    int32_t choice = 0;
    b.step(&choice);
    require(b.truncated[0] && !b.done[0] && !b.faults[0] && b.rewards[0] == 0, "cap became a loss");
    require(b.candidate_counts[0] > 0, "cap lost bootstrap observation");
    const auto before = b.episodes[0].decisions;
    choice = -1; b.step(&choice);
    require(before == 1 && b.reset_mask[0] && !b.advanced[0] && b.decision_counts[0] == 0 && b.seed_indices[0] == 1, "autoreset stepped terminal or consumed wrong seed");
    choice = 0; b.step(&choice); b.step(&choice);
    require(b.faults[0] == static_cast<uint32_t>(obs::Fault::SeedsExhausted) && !b.done[0] && !b.truncated[0], "seed exhaustion became game outcome");
    b.extend_seeds({3}); b.step(&choice);
    require(b.reset_mask[0] && b.seed_indices[0] == 2 && !b.faults[0], "seed sequence did not resume");
}
static void observation_fault() {
    TrainingBatch b(2, 2, 1000, {1, 2, 3, 4});
    CombatSetup setup;
    for (int i = 0; i < master_deck_capacity; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    start_combat(b.games[0], setup);
    auto& g = b.games[0];
    for (int i = 0; i < 65; ++i) g.discard.push_back(create_card(g, make_card(CardId::Strike_G)));
    g.grid_cards.clear();
    for (int i = 0; i < 100; ++i) g.grid_cards.push_back(g.draw[i]);
    open_grid_selection(g, PileId::Grid, 1, false);
    b.publish(0);
    require(b.faults[0] == static_cast<uint32_t>(obs::Fault::Entities), "entity overflow not reported");
    require(!b.done[0] && !b.truncated[0] && !b.candidate_counts[0] && b.rewards[0] == 0, "overflow became a loss or partial observation");
    for (int i = 0; i < obs::entity_mask_bytes; ++i) require(b.entity_mask[i] == 0, "overflow retained partial mask");
    require(!b.faults[1] && b.candidate_counts[1] > 0, "overflow contaminated another game");
    int32_t choices[]{-1, 0}; b.step(choices);
    require(b.reset_mask[0] && !b.faults[0] && b.advanced[1], "overflow recovery blocked another game");
    b.games[0] = Game{}; b.episodes[0].finished = false; b.publish(0);
    b.step(choices);
    require(b.faults[0] == static_cast<uint32_t>(obs::Fault::NoDecision) && !b.done[0], "NONE screen was treated as a game loss");
}
static void original_terminals() {
    std::ifstream input(SPIRE_DEEP_ACTIONS);
    require(input.good(), "missing accepted oracle action inputs");
    int64_t seed; int count, wins = 0, deaths = 0;
    while (input >> seed >> count) {
        TrainingBatch b(1, 1, count, {seed, 424242});
        float total = 0;
        bool heart = false;
        for (int i = 0; i < count; ++i) {
            int kind, index, target; input >> kind >> index >> target;
            require(input.good(), "incomplete input");
            int32_t selected = -1;
            for (int j = 0; j < b.actions[0].size; ++j) {
                const auto& a = b.actions[0][j];
                if (static_cast<int>(a.kind) == kind && a.index == index && a.target == target) selected = j;
            }
            require(selected >= 0, "oracle action missing from candidate list");
            heart |= b.games[0].run.act == 4 && b.games[0].encounter == EncounterId::TheHeart;
            b.step(&selected);
            require(!b.faults[0] && !b.truncated[0], "terminal precedence/cap failed");
            total += b.rewards[0];
            require(b.done[0] == (i == count - 1), "episode ended at wrong boundary");
        }
        if (b.games[0].screen == Screen::Victory) {
            ++wins;
            require(heart && b.games[0].run.ruby_key && b.games[0].run.emerald_key && b.games[0].run.sapphire_key, "victory lacked Heart and three keys");
            require(total == 1, "victory not paid exactly once");
        } else { ++deaths; require(total == 0, "death produced reward"); }
        int32_t ignored = -1;
        b.step(&ignored);
        require(b.reset_mask[0] && !b.advanced[0] && !b.done[0] && b.rewards[0] == 0, "post-terminal step paid or advanced");
    }
    require(wins > 0 && deaths > 0, "terminal coverage missing");
    std::printf("original terminal routes: wins=%d deaths=%d\n", wins, deaths);
}
int main() { clone_and_isolation(); truncation(); observation_fault(); original_terminals(); }
