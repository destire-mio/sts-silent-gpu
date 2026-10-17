#pragma once
#include "obs/storage.hpp"
#include <vector>

namespace spire::host {
struct ObservationBatch {
    const int size;
    std::vector<int16_t> ids, values, candidate_refs;
    std::vector<uint8_t> tags, candidate_kinds, entity_mask, candidate_mask;
    std::vector<int32_t> entity_counts, candidate_counts;

    explicit ObservationBatch(int size)
        : size(size), ids(size * obs::id_rows * obs::entity_capacity), values(size * obs::value_rows * obs::entity_capacity),
          candidate_refs(size * obs::candidate_rows * obs::candidate_capacity),
          tags(size * obs::tag_rows * obs::entity_capacity), candidate_kinds(size * obs::candidate_capacity),
          entity_mask(size * obs::entity_mask_bytes), candidate_mask(size * obs::candidate_mask_bytes),
          entity_counts(size), candidate_counts(size) { SPIRE_ASSERT(size > 0); }

    obs::View view(int i) {
        return obs::BatchView{tags.data(), ids.data(), values.data(), candidate_kinds.data(),
            candidate_refs.data(), entity_mask.data(), candidate_mask.data(),
            entity_counts.data(), candidate_counts.data()}.view(i);
    }
    uint32_t store(const int32_t* slots, int count, uint8_t* data, uint64_t capacity, uint64_t* offsets) {
        uint64_t bytes = 0;
        for (int i = 0; i < count; ++i) bytes += obs::stored_bytes(entity_counts[slots[i]], candidate_counts[slots[i]]);
        if (offsets[0] > capacity || bytes > capacity - offsets[0]) return static_cast<uint32_t>(obs::Fault::StorageCapacity);
        for (int i = 0; i < count; ++i) {
            const int slot = slots[i];
            obs::store(view(slot), data + offsets[i]);
            offsets[i + 1] = offsets[i] + obs::stored_bytes(entity_counts[slot], candidate_counts[slot]);
        }
        return 0;
    }
    void pad(const uint8_t* data, const uint64_t* offsets, const int32_t* indices) {
        for (int i = 0; i < size; ++i) obs::pad(data + offsets[indices[i]], view(i));
    }
};
}
