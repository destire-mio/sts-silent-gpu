#pragma once
#include "platform.hpp"

namespace spire {
SPIRE_FN inline bool same_text(const char* first, const char* second) {
    while (*first && *first == *second) { ++first; ++second; }
    return *first == *second;
}
template<class T, int Capacity>
struct List {
    T values[Capacity]{};
    uint16_t size = 0;
    SPIRE_FN T& operator[](int index) { SPIRE_ASSERT(index >= 0 && index < size); return values[index]; }
    SPIRE_FN const T& operator[](int index) const { SPIRE_ASSERT(index >= 0 && index < size); return values[index]; }
    template<class Pool>
    SPIRE_FN auto& at(Pool& pool, int index) { return pool[(*this)[index]]; }
    template<class Pool>
    SPIRE_FN const auto& at(const Pool& pool, int index) const { return pool[(*this)[index]]; }
    SPIRE_FN void clear() { size = 0; }
    SPIRE_FN bool empty() const { return size == 0; }
    SPIRE_FN void push_back(const T& value) { SPIRE_ASSERT(size < Capacity); values[size++] = value; }
    SPIRE_FN T pop_back() { SPIRE_ASSERT(size > 0); return values[--size]; }
    SPIRE_FN void insert(int index, const T& value) {
        SPIRE_ASSERT(index >= 0 && index <= size && size < Capacity);
        for (int i = size; i > index; --i) values[i] = values[i - 1];
        values[index] = value;
        ++size;
    }
    SPIRE_FN T erase(int index) {
        SPIRE_ASSERT(index >= 0 && index < size);
        T value = values[index];
        for (int i = index; i + 1 < size; ++i) values[i] = values[i + 1];
        --size;
        return value;
    }
};

template<class T, int Capacity>
struct Pool {
    T values[Capacity]{};
    uint64_t used[(Capacity + 63) / 64]{};
    SPIRE_FN bool contains(int index) const {
        return (used[index / 64] & (uint64_t{1} << (index % 64))) != 0;
    }
    SPIRE_FN T& operator[](int index) {
        SPIRE_ASSERT(index >= 0 && index < Capacity && contains(index));
        return values[index];
    }
    SPIRE_FN const T& operator[](int index) const {
        SPIRE_ASSERT(index >= 0 && index < Capacity && contains(index));
        return values[index];
    }
    SPIRE_FN uint16_t add(const T& value) {
        int index = 0;
        while (index < Capacity && contains(index)) ++index;
        SPIRE_ASSERT(index < Capacity);
        used[index / 64] |= uint64_t{1} << (index % 64);
        values[index] = value;
        return static_cast<uint16_t>(index);
    }
    SPIRE_FN void release(int index) { used[index / 64] &= ~(uint64_t{1} << (index % 64)); }
    SPIRE_FN void clear() { for (auto& word : used) word = 0; }
};

template<class T, int Capacity>
struct Queue {
    T values[Capacity]{};
    uint16_t head = 0;
    uint16_t size = 0;
    SPIRE_FN bool empty() const { return size == 0; }
    SPIRE_FN void clear() { head = 0; size = 0; }
    SPIRE_FN T& operator[](int index) {
        SPIRE_ASSERT(index >= 0 && index < size);
        return values[(head + index) % Capacity];
    }
    SPIRE_FN const T& operator[](int index) const {
        SPIRE_ASSERT(index >= 0 && index < size);
        return values[(head + index) % Capacity];
    }
    SPIRE_FN void push_back(const T& value) {
        SPIRE_ASSERT(size < Capacity);
        values[(head + size++) % Capacity] = value;
    }
    SPIRE_FN void push_front(const T& value) {
        SPIRE_ASSERT(size < Capacity);
        head = (head + Capacity - 1) % Capacity;
        values[head] = value;
        ++size;
    }
    SPIRE_FN void insert(int index, const T& value) {
        SPIRE_ASSERT(index >= 0 && index <= size && size < Capacity);
        for (int i = size; i > index; --i)
            values[(head + i) % Capacity] = values[(head + i - 1) % Capacity];
        values[(head + index) % Capacity] = value;
        ++size;
    }
    SPIRE_FN T erase(int index) {
        SPIRE_ASSERT(index >= 0 && index < size);
        T value = (*this)[index];
        for (int i = index; i + 1 < size; ++i) (*this)[i] = (*this)[i + 1];
        --size;
        return value;
    }
    SPIRE_FN T pop_front() {
        SPIRE_ASSERT(size > 0);
        T value = values[head];
        head = (head + 1) % Capacity;
        --size;
        return value;
    }
};

struct MoveHistory { uint16_t head = 0; uint16_t size = 0; };
struct MoveHistories {
    int8_t moves[1024]{};
    uint8_t links[1280]{};
    SPIRE_FN int previous(int node) const {
        const int bit = node * 10;
        return ((links[bit / 8] | (links[bit / 8 + 1] << 8)) >> (bit % 8)) & 1023;
    }
    uint16_t size = 0;
    SPIRE_FN void append(MoveHistory& history, int move) {
        SPIRE_ASSERT(size < 1024);
        moves[size] = move;
        const int bit = size * 10;
        const int value = ((links[bit / 8] | (links[bit / 8 + 1] << 8)) & ~(1023 << (bit % 8))) | (history.head << (bit % 8));
        links[bit / 8] = value;
        links[bit / 8 + 1] = value >> 8;
        history.head = ++size;
        ++history.size;
    }
    SPIRE_FN int last(const MoveHistory& history, int back = 0) const {
        SPIRE_ASSERT(back >= 0 && back < history.size);
        int node = history.head;
        while (back--) node = previous(node - 1);
        return moves[node - 1];
    }
};
struct DamageExceptions {
    int values[255]{};
    uint8_t targets[255]{};
    uint8_t next[255]{};
    uint64_t used[4]{};
    SPIRE_FN uint8_t add(int target, int value, uint8_t tail) {
        int slot = 0;
        while (slot < 255 && (used[slot / 64] & (uint64_t{1} << (slot % 64)))) ++slot;
        SPIRE_ASSERT(slot < 255);
        used[slot / 64] |= uint64_t{1} << (slot % 64);
        values[slot] = value;
        targets[slot] = target;
        next[slot] = tail;
        return slot + 1;
    }
    SPIRE_FN void release(uint8_t head) {
        while (head) {
            const int slot = head - 1;
            head = next[slot];
            used[slot / 64] &= ~(uint64_t{1} << (slot % 64));
        }
    }
    SPIRE_FN void clear() { for (auto& word : used) word = 0; }
};
struct DamageArray {
    int base;
    uint8_t head;
    SPIRE_FN int at(const DamageExceptions& storage, int target) const {
        for (int entry = head; entry; entry = storage.next[entry - 1])
            if (storage.targets[entry - 1] == target) return storage.values[entry - 1];
        return base;
    }
    SPIRE_FN void assign(DamageExceptions& storage, const int* amounts, int count) {
        storage.release(head);
        head = 0;
        if (count == 0) return;
        int best = 0;
        for (int i = 0; i < count; ++i) {
            if (i && amounts[i] == base) continue;
            int repeats = 0;
            for (int j = 0; j < count; ++j) repeats += amounts[j] == amounts[i];
            if (repeats > best) { best = repeats; base = amounts[i]; }
        }
        for (int i = 0; i < count; ++i)
            if (amounts[i] != base) head = storage.add(i, amounts[i], head);
    }
};

struct TextStore {
    char values[320]{};
    uint16_t size = 1;
    SPIRE_FN uint16_t intern(const char* text) {
        for (int i = 1; i < size;) {
            if (same_text(values + i, text)) return static_cast<uint16_t>(i);
            while (values[i++]) {}
        }
        const uint16_t offset = size;
        do {
            SPIRE_ASSERT(size < sizeof(values));
            values[size++] = *text;
        } while (*text++);
        return offset;
    }
    SPIRE_FN const char* operator[](uint16_t offset) const {
        SPIRE_ASSERT(offset < size);
        return offset ? values + offset : nullptr;
    }
    SPIRE_FN void clear() { size = 1; }
};
}
