#pragma once
#include "platform.hpp"

namespace spire {
template<class T, int Capacity>
struct List {
    T values[Capacity]{};
    uint16_t size = 0;
    SPIRE_FN T& operator[](int index) { SPIRE_ASSERT(index >= 0 && index < size); return values[index]; }
    SPIRE_FN const T& operator[](int index) const { SPIRE_ASSERT(index >= 0 && index < size); return values[index]; }
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
    SPIRE_FN T pop_front() {
        SPIRE_ASSERT(size > 0);
        T value = values[head];
        head = (head + 1) % Capacity;
        --size;
        return value;
    }
};
template<class T, int Capacity>
struct RecentHistory {
    T values[Capacity]{};
    int size = 0;
    SPIRE_FN void clear() { size = 0; }
    SPIRE_FN bool empty() const { return size == 0; }
    SPIRE_FN int first_index() const { return max(0, size - Capacity); }
    SPIRE_FN void push_back(T value) { values[size++ % Capacity] = value; }
    SPIRE_FN const T& operator[](int index) const {
        SPIRE_ASSERT(index >= first_index() && index < size);
        return values[index % Capacity];
    }
};
}
