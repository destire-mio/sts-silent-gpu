#pragma once
#include "obs/encode.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace spire::host {
struct Episode {
    uint64_t decisions = 0;
    int64_t seed_index = -1;
    bool finished = false;
};

struct TrainingBatch {
    const int size, threads;
    const uint64_t decision_cap;
    std::vector<Game> games;
    std::vector<ActionSet> actions;
    std::vector<Episode> episodes;
    std::vector<obs::History> reveals;
    std::vector<int16_t> ids, values, candidate_refs;
    std::vector<uint8_t> tags, candidate_kinds;
    std::vector<uint8_t> entity_mask, candidate_mask, done, truncated, reset_mask, advanced;
    std::vector<float> rewards;
    std::vector<uint32_t> faults;
    std::vector<int32_t> entity_counts, candidate_counts;
    std::vector<int64_t> seed_indices, decision_counts, timing_ns;

    TrainingBatch(int size, int threads, uint64_t cap, const std::vector<int64_t>& seeds)
        : size(size), threads(threads), decision_cap(cap), games(size), actions(size), episodes(size), reveals(size),
          ids(size * obs::id_rows * obs::entity_capacity), values(size * obs::value_rows * obs::entity_capacity),
          candidate_refs(size * obs::candidate_rows * obs::candidate_capacity),
          tags(size * obs::tag_rows * obs::entity_capacity), candidate_kinds(size * obs::candidate_capacity),
          entity_mask(size * obs::entity_mask_bytes), candidate_mask(size * obs::candidate_mask_bytes),
          done(size), truncated(size), reset_mask(size), advanced(size), rewards(size), faults(size),
          entity_counts(size), candidate_counts(size), seed_indices(size), decision_counts(size), timing_ns(size * 3),
          seeds_(seeds), next_seeds_(size) {
        SPIRE_ASSERT(size > 0 && threads > 0 && threads <= size && cap > 0);
        for (auto& episode : episodes) episode.finished = true;
        for (int i = 1; i < threads; ++i) workers_.emplace_back([this, i] { worker(i); });
        step(nullptr);
    }
    TrainingBatch(const TrainingBatch&) = delete;
    TrainingBatch& operator=(const TrainingBatch&) = delete;
    ~TrainingBatch() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        for (auto& worker : workers_) worker.join();
    }
    obs::View view(int i) {
        return {tags.data() + i * obs::tag_rows * obs::entity_capacity,
            ids.data() + i * obs::id_rows * obs::entity_capacity,
            values.data() + i * obs::value_rows * obs::entity_capacity,
            candidate_kinds.data() + i * obs::candidate_capacity,
            candidate_refs.data() + i * obs::candidate_rows * obs::candidate_capacity,
            entity_mask.data() + i * obs::entity_mask_bytes,
            candidate_mask.data() + i * obs::candidate_mask_bytes,
            &entity_counts[i], &candidate_counts[i]};
    }
    void publish(int i) {
        const auto start = Clock::now();
        const auto result = obs::encode(games[i], reveals[i], view(i), actions[i]);
        timing_ns[i * 3 + 2] += elapsed(start);
        if (result.fault) {
            faults[i] = result.fault; episodes[i].finished = true;
            done[i] = truncated[i] = 0; rewards[i] = 0;
        }
    }
    void step(const int32_t* selected) {
        std::lock_guard<std::mutex> operation(operation_mutex_);
        selected_ = selected;
        for (int i = 0; i < size; ++i) {
            next_seeds_[i] = -1;
            if (episodes[i].finished && next_seed_ < seeds_.size()) next_seeds_[i] = next_seed_++;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            remaining_ = threads - 1;
            ++generation_;
        }
        wake_.notify_all();
        run(0);
        std::unique_lock<std::mutex> lock(mutex_);
        completed_.wait(lock, [this] { return remaining_ == 0; });
    }
    void extend_seeds(const std::vector<int64_t>& seeds) {
        std::lock_guard<std::mutex> operation(operation_mutex_);
        seeds_.insert(seeds_.end(), seeds.begin(), seeds.end());
    }
    void clone(int source, int target) {
        std::lock_guard<std::mutex> operation(operation_mutex_);
        SPIRE_ASSERT(source >= 0 && source < size && target >= 0 && target < size && source != target);
        games[target] = games[source]; episodes[target] = episodes[source]; reveals[target] = reveals[source];
        done[target] = done[source]; truncated[target] = truncated[source]; faults[target] = faults[source];
        rewards[target] = 0; reset_mask[target] = advanced[target] = 0;
        seed_indices[target] = seed_indices[source]; decision_counts[target] = decision_counts[source];
        if (faults[target]) fail(target, faults[target]);
        else publish(target);
    }

private:
    using Clock = std::chrono::steady_clock;
    std::vector<int64_t> seeds_, next_seeds_;
    std::size_t next_seed_ = 0;
    const int32_t* selected_ = nullptr;
    std::vector<std::thread> workers_;
    std::mutex mutex_, operation_mutex_;
    std::condition_variable wake_, completed_;
    uint64_t generation_ = 0;
    int remaining_ = 0;
    bool stop_ = false;

    static int64_t elapsed(Clock::time_point start) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    }
    void worker(int index) {
        uint64_t seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this, seen] { return stop_ || generation_ != seen; });
                if (stop_) return;
                seen = generation_;
            }
            run(index);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --remaining_;
            }
            completed_.notify_one();
        }
    }
    void fail(int i, uint32_t code) {
        faults[i] = code; episodes[i].finished = true;
        done[i] = truncated[i] = 0; rewards[i] = 0;
        view(i).finish(0, 0); actions[i].clear();
    }
    void run(int worker) {
        for (int i = worker; i < size; i += threads) {
            auto& episode = episodes[i];
            auto& g = games[i];
            rewards[i] = 0; done[i] = truncated[i] = reset_mask[i] = advanced[i] = 0; faults[i] = 0;
            if (episode.finished) {
                if (next_seeds_[i] < 0) { fail(i, static_cast<uint32_t>(obs::Fault::SeedsExhausted)); continue; }
                const auto start = Clock::now();
                spire::reset(g, seeds_[next_seeds_[i]]);
                reveals[i] = {};
                timing_ns[i * 3] += elapsed(start);
                episode = Episode{0, next_seeds_[i], false};
                seed_indices[i] = episode.seed_index; decision_counts[i] = 0; reset_mask[i] = 1;
                publish(i);
                continue;
            }
            if (g.fault) { fail(i, g.fault); continue; }
            if (actions[i].empty()) { fail(i, static_cast<uint32_t>(obs::Fault::NoDecision)); continue; }
            const int candidate = selected_[i];
            if (candidate < 0 || candidate >= actions[i].size) { fail(i, static_cast<uint32_t>(obs::Fault::InvalidCandidate)); continue; }
            const auto start = Clock::now();
            obs::record_action(reveals[i], g, actions[i][candidate]);
            spire::step(g, actions[i][candidate]);
            timing_ns[i * 3 + 1] += elapsed(start);
            ++episode.decisions; decision_counts[i] = episode.decisions; advanced[i] = 1;
            if (g.fault) { fail(i, g.fault); continue; }
            if (g.screen == Screen::Victory || g.screen == Screen::Death) {
                episode.finished = true; done[i] = 1;
                rewards[i] = g.screen == Screen::Victory ? 1.0f : 0.0f;
            } else if (episode.decisions == decision_cap) {
                episode.finished = true; truncated[i] = 1;
            }
            publish(i);
            if (!episode.finished && actions[i].empty()) fail(i, static_cast<uint32_t>(obs::Fault::NoDecision));
        }
    }
};
}
