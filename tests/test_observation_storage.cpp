#include "host/training_batch.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using namespace spire;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static void ranges() {
    obs::Observation output;
    obs::Writer writer{output.view()};
    require(writer.word(-32768) == -32768 && writer.word(32767) == 32767 && !writer.result.fault, "int16 endpoints changed");
    require(writer.byte(0) == 0 && writer.byte(255) == 255 && !writer.result.fault, "uint8 endpoints changed");
    for (int value : {-32769, 32768}) {
        writer.result.fault = 0; writer.word(value);
        require(writer.result.fault == static_cast<uint32_t>(obs::Fault::NumericRange), "int16 overflow was wrapped or clamped");
    }
    for (int value : {-1, 256}) {
        writer.result.fault = 0; writer.byte(value);
        require(writer.result.fault == static_cast<uint32_t>(obs::Fault::NumericRange), "uint8 overflow was wrapped or clamped");
    }
    for (int value : {-1, 2}) {
        writer.result.fault = 0; writer.bit(value, 3);
        require(writer.result.fault == static_cast<uint32_t>(obs::Fault::NumericRange), "flag overflow was accepted");
    }
    host::TrainingBatch batch(2, 2, 1000, {1, 2});
    batch.games[0].run.gold = 32767;
    batch.publish(0);
    require(!batch.faults[0] && batch.values[2 * obs::entity_capacity + 1] == 32767, "visible endpoint lost");
    batch.games[0].run.gold = 32768;
    batch.publish(0);
    require(batch.faults[0] == static_cast<uint32_t>(obs::Fault::NumericRange), "visible numeric overflow lacked fault");
    require(batch.entity_counts[0] == 0 && batch.candidate_counts[0] == 0 && !batch.done[0] && !batch.truncated[0] && batch.rewards[0] == 0, "numeric fault leaked partial output or became a loss");
    require(!batch.faults[1] && batch.entity_counts[1] > 0, "numeric fault contaminated another slot");
    obs::Observation blank;
    auto view = batch.view(0);
    require(!std::memcmp(view.tags, blank.tags, sizeof(blank.tags)) && !std::memcmp(view.values, blank.values, sizeof(blank.values)), "fault left previous/written data");
    uint8_t data[128]; std::memset(data, 0xa5, sizeof(data));
    uint64_t offsets[]{0, 123}; int32_t slot = 0;
    require(batch.store(&slot, 1, data, sizeof(data), offsets) == batch.faults[0] && offsets[1] == 123 && data[0] == 0xa5, "faulted observation was stored");
}
static void storage_bytes() {
    obs::Observation output;
    obs::Writer writer{output.view()};
    auto player = obs::token(obs::Kind::Player, 1);
    player.values[0] = -32768; player.values[1] = 32767;
    writer.add(player); output.view().finish(1, 0);
    uint8_t bytes[28]{};
    obs::store(output.view(), bytes);
    require(bytes[0] == 1 && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0, "record counts are not little endian");
    require(bytes[4] == 2 && bytes[8] == 1 && bytes[9] == 0, "record row order changed");
    require(bytes[16] == 0 && bytes[17] == 128 && bytes[18] == 255 && bytes[19] == 127, "signed words are not little endian");
}
static void packed_masks_and_tail() {
    obs::Observation reused;
    for (int count : {0, 1, 7, 8, 9, 127, 128, 129, 285, 384, 9, 8, 1, 0, 384, 0}) {
        obs::Observation fresh;
        for (auto* output : {&reused, &fresh}) {
            obs::Writer writer{output->view()};
            for (int i = 0; i < count; ++i) writer.add(obs::token(obs::Kind::Card, 1));
            const int candidates = count > obs::candidate_capacity ? obs::candidate_capacity : count;
            for (int i = 0; i < candidates; ++i) {
                output->candidate_kinds[i] = 1;
                for (int row = 0; row < obs::candidate_rows; ++row) output->candidate_refs[row * obs::candidate_capacity + i] = i + 1;
            }
            output->view().finish(count, candidates);
        }
        require(!std::memcmp(&reused, &fresh, sizeof(fresh)), "grow/shrink left stale padding");
        for (int i = 0; i < obs::entity_capacity; ++i)
            require(((reused.entity_mask[i / 8] >> (i % 8)) & 1) == (i < count), "entity mask bit order/count wrong");
        for (int i = 0; i < obs::candidate_capacity; ++i)
            require(((reused.candidate_mask[i / 8] >> (i % 8)) & 1) == (i < reused.candidates), "candidate mask bit order/count wrong");
    }
}
static void deep_roundtrips() {
    std::ifstream input(SPIRE_DEEP_ACTIONS);
    require(input.good(), "missing deep inputs");
    int64_t seed; int count;
    obs::Observation reused, padded;
    uint8_t storage[sizeof(obs::Observation)];
    int decisions = 0;
    while (input >> seed >> count) {
        Game game; reset(game, seed);
        obs::History history;
        for (int i = 0; i <= count; ++i) {
            ActionSet actions;
            require(!obs::encode(game, history, reused.view(), actions).fault, "deep encode fault");
            obs::Observation fresh;
            require(!obs::encode(game, history, fresh.view(), actions).fault, "fresh encode fault");
            require(!std::memcmp(&reused, &fresh, sizeof(fresh)), "reused buffer differs from fresh buffer");
            obs::store(reused.view(), storage);
            obs::pad(storage, padded.view());
            require(!std::memcmp(&reused, &padded, sizeof(padded)), "compact roundtrip changed entities/candidates/masks");
            if (i == count) break;
            ++decisions;
            int kind, index, target; input >> kind >> index >> target;
            const Action action{static_cast<ActionKind>(kind), index, target};
            obs::record_action(history, game, action); step(game, action);
        }
    }
    require(decisions == 15038, "deep fixture coverage changed");
    std::printf("compact roundtrips: %d decisions, 12 terminal observations\n", decisions);
}
int main() {
    static_assert(sizeof(Game) == 37872);
    static_assert(sizeof(obs::Observation) == 10184);
    ranges(); storage_bytes(); packed_masks_and_tail(); deep_roundtrips();
}
