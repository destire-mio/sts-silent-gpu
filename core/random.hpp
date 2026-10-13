#pragma once

#include "platform.hpp"
#include <cstdint>

namespace spire {

SPIRE_FN inline std::int64_t signed_long(std::uint64_t bits) {
    return bits <= 0x7fffffffffffffffULL
        ? static_cast<std::int64_t>(bits)
        : -1 - static_cast<std::int64_t>(~bits);
}

SPIRE_FN inline std::int64_t java_long_cast(double value) {
    if (value >= 9223372036854775808.0) return 0x7fffffffffffffffLL;
    if (value <= -9223372036854775808.0) return (-0x7fffffffffffffffLL - 1);
    return static_cast<std::int64_t>(value);
}

SPIRE_FN inline std::uint64_t rng_murmur_hash(std::uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    return value ^ (value >> 33);
}

struct RandomXS128 {
    std::uint64_t s0 = 0;
    std::uint64_t s1 = 0;

    SPIRE_FN RandomXS128() = default;
    SPIRE_FN explicit RandomXS128(std::uint64_t seed) { reset(seed); }

    SPIRE_FN void reset(std::uint64_t seed) {
        // RandomXS128.setSeed maps zero to Long.MIN_VALUE before hashing.
        s0 = rng_murmur_hash(seed == 0 ? 0x8000000000000000ULL : seed);
        s1 = rng_murmur_hash(s0);
    }

    SPIRE_FN void set_state(std::uint64_t first, std::uint64_t second) {
        s0 = first;
        s1 = second;
    }

    SPIRE_FN std::uint64_t next_bits64() {
        std::uint64_t x = s0;
        const std::uint64_t y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }

    SPIRE_FN std::int64_t nextLong() { return signed_long(next_bits64()); }

    SPIRE_FN std::int64_t nextLong(std::int64_t bound) {
        SPIRE_ASSERT(bound > 0);
        const auto n = static_cast<std::uint64_t>(bound);
        for (;;) {
            const std::uint64_t bits = next_bits64() >> 1;
            const std::uint64_t value = bits % n;
            if (bits - value + n - 1 <= 0x7fffffffffffffffULL)
                return static_cast<std::int64_t>(value);
        }
    }

    SPIRE_FN std::int32_t nextInt(std::int32_t bound) {
        return static_cast<std::int32_t>(nextLong(bound));
    }

    SPIRE_FN double nextDouble() {
        return static_cast<double>(next_bits64() >> 11) * 0x1p-53;
    }

    SPIRE_FN float nextFloat() {
        return static_cast<float>(static_cast<double>(next_bits64() >> 40) * 0x1p-24);
    }

    SPIRE_FN bool nextBoolean() { return (next_bits64() & 1) != 0; }
};

struct Random {
    RandomXS128 generator;
    std::int32_t counter = 0;

    SPIRE_FN Random() = default;
    SPIRE_FN explicit Random(std::uint64_t seed, std::int32_t calls = 0) {
        reset(seed, calls);
    }

    SPIRE_FN void reset(std::uint64_t seed, std::int32_t calls = 0) {
        generator.reset(seed);
        counter = 0;
        // Random(Long,int) advances through random(999), not a state jump.
        for (std::int32_t i = 0; i < calls; ++i) random(999);
    }

    SPIRE_FN void set_state(std::uint64_t first, std::uint64_t second,
                            std::int32_t calls) {
        generator.set_state(first, second);
        counter = calls;
    }

    SPIRE_FN void setCounter(std::int32_t target) {
        while (counter < target) randomBoolean();
    }

    SPIRE_FN std::int32_t random(std::int32_t range) {
        ++counter;
        SPIRE_ASSERT(range >= 0 && range < 0x7fffffff);
        return generator.nextInt(range + 1);
    }

    SPIRE_FN std::int32_t random(std::int32_t start, std::int32_t end) {
        ++counter;
        const std::int64_t width = static_cast<std::int64_t>(end) - start + 1;
        SPIRE_ASSERT(width > 0 && width <= 0x7fffffff);
        return start + generator.nextInt(static_cast<std::int32_t>(width));
    }

    SPIRE_FN std::int64_t random(std::int64_t range) {
        ++counter;
        // Random.random(long) excludes its upper endpoint, unlike the int overload.
        return java_long_cast(generator.nextDouble() * static_cast<double>(range));
    }

    SPIRE_FN std::int64_t random(std::int64_t start, std::int64_t end) {
        ++counter;
        const auto width = signed_long(static_cast<std::uint64_t>(end) -
                                       static_cast<std::uint64_t>(start));
        const auto offset = java_long_cast(generator.nextDouble() * static_cast<double>(width));
        return signed_long(static_cast<std::uint64_t>(start) + static_cast<std::uint64_t>(offset));
    }

    SPIRE_FN std::int64_t randomLong() {
        ++counter;
        return generator.nextLong();
    }

    SPIRE_FN bool randomBoolean() {
        ++counter;
        return generator.nextBoolean();
    }

    SPIRE_FN bool randomBoolean(float chance) {
        ++counter;
        return generator.nextFloat() < chance;
    }

    SPIRE_FN float random() {
        ++counter;
        return generator.nextFloat();
    }

    SPIRE_FN float random(float range) {
        ++counter;
        return generator.nextFloat() * range;
    }

    SPIRE_FN float random(float start, float end) {
        ++counter;
        return start + generator.nextFloat() * (end - start);
    }
};

struct UnseededRandom {
    RandomXS128 generator;
    std::int64_t counter = 0;

    SPIRE_FN UnseededRandom() = default;
    SPIRE_FN explicit UnseededRandom(std::uint64_t seed) { reset(seed); }
    SPIRE_FN void reset(std::uint64_t seed) { generator.reset(seed); counter = 0; }
    SPIRE_FN void set_state(std::uint64_t first, std::uint64_t second, std::int64_t calls) {
        generator.set_state(first, second);
        counter = calls;
    }
    SPIRE_FN std::int64_t nextLong() { ++counter; return generator.nextLong(); }
    SPIRE_FN std::int64_t nextLong(std::int64_t bound) {
        SPIRE_ASSERT(bound > 0);
        const auto n = static_cast<std::uint64_t>(bound);
        for (;;) {
            const auto bits = static_cast<std::uint64_t>(nextLong()) >> 1;
            const auto value = bits % n;
            if (bits - value + n - 1 <= 0x7fffffffffffffffULL) return static_cast<std::int64_t>(value);
        }
    }
    SPIRE_FN std::int32_t nextInt(std::int32_t bound) { return static_cast<std::int32_t>(nextLong(bound)); }
    SPIRE_FN double nextDouble() { return static_cast<double>(static_cast<std::uint64_t>(nextLong()) >> 11) * 0x1p-53; }
    SPIRE_FN float nextFloat() { return static_cast<float>(static_cast<double>(static_cast<std::uint64_t>(nextLong()) >> 40) * 0x1p-24); }
    SPIRE_FN bool nextBoolean() { return (static_cast<std::uint64_t>(nextLong()) & 1) != 0; }
    SPIRE_FN std::int32_t random(std::int32_t range) { return nextInt(range + 1); }
    SPIRE_FN std::int32_t random(std::int32_t start, std::int32_t end) { return start + nextInt(end - start + 1); }
    SPIRE_FN std::int64_t random(std::int64_t range) { return java_long_cast(nextDouble() * static_cast<double>(range)); }
    SPIRE_FN float random() { return nextFloat(); }
    SPIRE_FN float random(float range) { return nextFloat() * range; }
    SPIRE_FN float random(float start, float end) { return start + nextFloat() * (end - start); }
    SPIRE_FN bool randomBoolean() { return nextBoolean(); }
    SPIRE_FN bool randomBoolean(float chance) { return nextFloat() < chance; }
};

SPIRE_FN inline Random make_unseeded_random(UnseededRandom& unseeded) {
    const auto seed = unseeded.random(std::int64_t{9999});
    const auto calls = unseeded.random(99);
    return Random(static_cast<std::uint64_t>(seed), calls);
}

SPIRE_FN inline Random copy_random(const Random& source, UnseededRandom& unseeded) {
    Random result = make_unseeded_random(unseeded);
    result.set_state(source.generator.s0, source.generator.s1, source.counter);
    return result;
}

struct JavaRandom {
    std::uint64_t state;

    SPIRE_FN explicit JavaRandom(std::uint64_t seed)
        : state((seed ^ 0x5deece66dULL) & 0xffffffffffffULL) {}

    SPIRE_FN std::uint32_t next(std::int32_t bits) {
        state = (state * 0x5deece66dULL + 11) & 0xffffffffffffULL;
        return static_cast<std::uint32_t>(state >> (48 - bits));
    }

    SPIRE_FN std::int32_t nextInt(std::int32_t bound) {
        SPIRE_ASSERT(bound > 0);
        const auto n = static_cast<std::uint32_t>(bound);
        if ((n & (n - 1)) == 0)
            return static_cast<std::int32_t>((static_cast<std::uint64_t>(n) * next(31)) >> 31);
        for (;;) {
            const std::uint32_t bits = next(31);
            const std::uint32_t value = bits % n;
            if (bits - value + n - 1 <= 0x7fffffffU)
                return static_cast<std::int32_t>(value);
        }
    }
};

template <typename T>
SPIRE_FN inline void shuffle_java(T* items, std::int32_t count, JavaRandom& java_rng) {
    for (std::int32_t i = count; i > 1; --i) {
        const std::int32_t j = java_rng.nextInt(i);
        const T value = items[i - 1];
        items[i - 1] = items[j];
        items[j] = value;
    }
}

template <typename T>
SPIRE_FN inline void shuffle(T* items, std::int32_t count, Random& rng) {
    // CardGroup.shuffle seeds java.util.Random with exactly one shuffleRng draw.
    JavaRandom java_rng(static_cast<std::uint64_t>(rng.randomLong()));
    shuffle_java(items, count, java_rng);
}

template <typename T>
SPIRE_FN inline void shuffle_unseeded(T* items, std::int32_t count, UnseededRandom& rng) {
    JavaRandom java_rng(static_cast<std::uint64_t>(rng.nextLong()));
    shuffle_java(items, count, java_rng);
}

struct RngStreams {
    Random monster;
    Random map;
    Random event;
    Random merchant;
    Random card;
    Random treasure;
    Random relic;
    Random potion;
    Random monster_hp;
    Random ai;
    Random shuffle;
    Random card_random;
    Random misc;
    Random neow;
    bool neow_initialized = false;

    SPIRE_FN void generate_seeds(std::uint64_t seed) {
        monster.reset(seed);
        event.reset(seed);
        merchant.reset(seed);
        card.reset(seed);
        treasure.reset(seed);
        relic.reset(seed);
        monster_hp.reset(seed);
        potion.reset(seed);
        ai.reset(seed);
        shuffle.reset(seed);
        card_random.reset(seed);
        misc.reset(seed);
    }

    SPIRE_FN void reseed_floor(std::uint64_t seed, std::int32_t floor) {
        const auto floor_seed = seed + static_cast<std::uint64_t>(floor);
        monster_hp.reset(floor_seed);
        ai.reset(floor_seed);
        shuffle.reset(floor_seed);
        card_random.reset(floor_seed);
        misc.reset(floor_seed);
    }

    SPIRE_FN void reseed_map(std::uint64_t seed, std::int32_t act) {
        SPIRE_ASSERT(act >= 1 && act <= 4);
        // Exordium/TheCity/TheBeyond/TheEnding constructors use different offsets.
        const std::int32_t offset = act == 1 ? act : act * (act - 1) * 100;
        map.reset(seed + static_cast<std::uint64_t>(offset));
    }
};

}
