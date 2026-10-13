#include "core/random.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

static void require(bool condition, const char* field) {
    if (!condition) {
        std::fprintf(stderr, "RNG mismatch: %s\n", field);
        std::exit(1);
    }
}

static std::uint32_t float_bits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct Vector {
    std::uint64_t seed, initial0, initial1, first_long;
    int inclusive, ranged;
    bool boolean, chance;
    std::uint32_t unit, scaled, ranged_float;
    std::int64_t scaled_long, ranged_long;
    std::uint64_t last_long, final0, final1;
    int counter;
};

// Random/RandomXS128 values from desktop-1.0.jar dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a.
constexpr Vector vectors[] = {
    {0x0000000000000000ULL,0x8f780810af31a493ULL,0xd1f9a22af8e83383ULL,0x28d01376a138a279ULL,60,8,false,false,0x3f72367bU,0x3ff62318U,0x3eb195b8U,169279313812LL,2520480332470LL,0xd5bca2a05d3e8a20ULL,0x6613268d2f6823a8ULL,0x6fa97c132dd66678ULL,11},
    {0x0000000000000001ULL,0xb456bcfc34c2cb2cULL,0x7d6e4ac38b2b1be2ULL,0x34913d26a7e2cd2fULL,5,-3,true,false,0x3f5395d5U,0x3ff94c11U,0x3e1d5c20U,122706934827LL,-2117204024793LL,0xc55b7b04680fcc84ULL,0xd9d56d691df959c2ULL,0xeb860d9b4a1672c2ULL,11},
    {0xffffffffffffffffULL,0x64b5720b4b825f21ULL,0xfa605f44aea3667dULL,0x95d13c1b62c7ec73ULL,62,13,true,false,0x3e797ec4U,0x4047057bU,0x40ef88beU,255054469988LL,-2325857546378LL,0x03b4afb0f7b5db1cULL,0xa3044a184b21ebe3ULL,0x60b06598ac93ef39ULL,11},
    {0x8000000000000000ULL,0x8f780810af31a493ULL,0xd1f9a22af8e83383ULL,0x28d01376a138a279ULL,60,8,false,false,0x3f72367bU,0x3ff62318U,0x3eb195b8U,169279313812LL,2520480332470LL,0xd5bca2a05d3e8a20ULL,0x6613268d2f6823a8ULL,0x6fa97c132dd66678ULL,11},
    {0x7fffffffffffffffULL,0xabb93df0a930edeaULL,0xe72306068b6e596aULL,0x9bf1905658ca74a4ULL,87,2,false,false,0x3f7e283cU,0x3f7c6546U,0xbdbac120U,155674886325LL,1669942029323LL,0x4fc7985d006714ebULL,0xe28d9b3ded1358e8ULL,0x6d39fd1f1353bc03ULL,11},
    {0x00000000075bcd15ULL,0x8f7c29206384f886ULL,0x424f1c266a80dc24ULL,0x9f5214f296ee199eULL,63,10,false,true,0x3f660122U,0x3ffb33dfU,0x40d28fa8U,450181367875LL,2305245218224LL,0x69041bff4b91303dULL,0xbffe2f7929d4fa01ULL,0xa905ec8621bc363cULL,11},
};

static void test_game_random() {
    for (const auto& v : vectors) {
        spire::Random rng(v.seed);
        require(rng.generator.s0 == v.initial0 && rng.generator.s1 == v.initial1, "seed hashing");
        require(static_cast<std::uint64_t>(rng.randomLong()) == v.first_long, "first long");
        require(rng.random(99) == v.inclusive, "inclusive int");
        require(rng.random(-7, 13) == v.ranged, "ranged int");
        require(rng.randomBoolean() == v.boolean, "boolean");
        require(rng.randomBoolean(0.375f) == v.chance, "chance");
        require(float_bits(rng.random()) == v.unit, "unit float");
        require(float_bits(rng.random(3.25f)) == v.scaled, "scaled float");
        require(float_bits(rng.random(-2.5f, 7.75f)) == v.ranged_float, "ranged float");
        require(rng.random(std::int64_t{1000000000000LL}) == v.scaled_long, "scaled long");
        require(rng.random(std::int64_t{-4000000000000LL}, std::int64_t{3000000000000LL}) == v.ranged_long, "ranged long");
        require(static_cast<std::uint64_t>(rng.randomLong()) == v.last_long, "last long");
        require(rng.generator.s0 == v.final0 && rng.generator.s1 == v.final1, "mixed calls final state");
        require(rng.counter == v.counter, "mixed calls counter");
    }
    spire::Random resumed(123456789, 9);
    require(resumed.generator.s0 == 0x4ca5c3ab85e410afULL && resumed.generator.s1 == 0x26995260c5359badULL, "constructor call count");
    require(resumed.counter == 9, "constructor counter");
    spire::Random advanced(123456789);
    advanced.setCounter(9);
    require(advanced.generator.s0 == resumed.generator.s0 && advanced.generator.s1 == resumed.generator.s1, "setCounter state");
    advanced.setCounter(3);
    require(advanced.counter == 9, "setCounter does not rewind");
    spire::Random restored;
    restored.set_state(resumed.generator.s0, resumed.generator.s1, resumed.counter);
    require(restored.randomLong() == resumed.randomLong() && restored.counter == resumed.counter, "restored state");
    spire::Random single(1);
    require(single.random(0) == 0 && single.random(5, 5) == 5 && single.counter == 2, "singleton ranges consume RNG");
}

static void test_rejection() {
    spire::RandomXS128 raw(0xffffffffffffffffULL);
    require(raw.nextLong(4611686018427387905LL) == 3412722986677424662LL, "XS128 rejection value");
    require(raw.s0 == 0x9b70dcd6b42485f6ULL && raw.s1 == 0xc347fe1c7f4b1e37ULL, "XS128 rejection state");
    constexpr int expected[] = {215764588,880641847,874970313,446064254,77814904,714504434,13136569,327998473};
    spire::JavaRandom java(1);
    for (int value : expected) require(java.nextInt(1073741825) == value, "java.util.Random rejection");
}

static void test_shuffle() {
    constexpr int expected[][12] = {
        {0,8,5,1,6,2,7,10,3,4,11,9},
        {7,11,9,10,0,2,1,4,3,8,5,6},
        {9,10,3,6,7,8,4,0,11,1,2,5},
        {0,8,5,1,6,2,7,10,3,4,11,9},
        {5,10,0,9,6,2,11,1,3,8,7,4},
        {2,5,0,6,7,9,11,1,8,4,10,3},
    };
    for (int i = 0; i < 6; ++i) {
        int cards[12];
        for (int j = 0; j < 12; ++j) cards[j] = j;
        spire::Random rng(vectors[i].seed);
        spire::shuffle(cards, 12, rng);
        for (int j = 0; j < 12; ++j) require(cards[j] == expected[i][j], "Collections.shuffle card order");
        require(rng.counter == 1, "shuffle stream counter");
    }
    spire::Random empty(1);
    int card = 1;
    spire::shuffle(&card, 0, empty);
    require(empty.counter == 1, "empty CardGroup shuffle consumes seed");
    constexpr int persistent_expected[][12] = {
        {4,10,3,8,1,7,11,5,2,9,6,0},
        {9,11,4,0,1,6,3,7,8,10,2,5},
    };
    spire::JavaRandom persistent(0);
    for (const auto& order : persistent_expected) {
        int cards[12];
        for (int i = 0; i < 12; ++i) cards[i] = i;
        spire::shuffle_java(cards, 12, persistent);
        for (int i = 0; i < 12; ++i) require(cards[i] == order[i], "persistent Collections.shuffle stream");
    }
}

static void test_streams() {
    static_assert(std::is_standard_layout<spire::RngStreams>::value);
    static_assert(std::is_trivially_copyable<spire::RngStreams>::value);
    spire::RngStreams streams;
    streams.map.reset(123456789);
    streams.generate_seeds(0);
    spire::Random* run_streams[] = {
        &streams.monster, &streams.event, &streams.merchant, &streams.card,
        &streams.treasure, &streams.relic, &streams.potion,
    };
    spire::Random* floor_streams[] = {
        &streams.monster_hp, &streams.ai, &streams.shuffle, &streams.card_random, &streams.misc,
    };
    for (auto* rng : run_streams) {
        require(rng->generator.s0 == vectors[0].initial0 && rng->generator.s1 == vectors[0].initial1, "generateSeeds run stream");
        rng->randomLong();
    }
    for (auto* rng : floor_streams) {
        require(rng->generator.s0 == vectors[0].initial0 && rng->generator.s1 == vectors[0].initial1, "generateSeeds floor stream");
        rng->randomLong();
    }
    streams.reseed_floor(0, 1);
    for (auto* rng : run_streams) {
        require(rng->counter == 1, "floor leaves run stream counter");
        require(rng->generator.s0 == 0xd1f9a22af8e83383ULL && rng->generator.s1 == 0x56d6714ba8506ef6ULL, "floor leaves run stream state");
    }
    for (auto* rng : floor_streams) {
        require(rng->generator.s0 == vectors[1].initial0 && rng->generator.s1 == vectors[1].initial1, "floor reseed state");
        require(rng->counter == 0, "floor reseed counter");
    }
    require(streams.map.generator.s0 == vectors[5].initial0, "generateSeeds and floor leave map stream");
    streams.reseed_floor(0x7fffffffffffffffULL, 1);
    require(streams.ai.generator.s0 == vectors[3].initial0, "floor seed signed wrap");
    constexpr int map_offsets[] = {1,200,600,1200};
    for (int act = 1; act <= 4; ++act) {
        streams.reseed_map(123456789, act);
        spire::Random expected(123456789 + map_offsets[act - 1]);
        require(streams.map.generator.s0 == expected.generator.s0 && streams.map.generator.s1 == expected.generator.s1, "act map seed");
        require(streams.map.counter == 0, "act map counter");
    }
}

static void test_unseeded() {
    struct UnseededVector {
        std::uint64_t seed;
        int inclusive, ranged;
        bool boolean, chance;
        std::uint32_t unit, scaled, ranged_float;
        std::int64_t scaled_long;
        std::uint64_t final0, final1;
    };
    constexpr UnseededVector expected[] = {
        {0,72,6,true,false,0x3ec79804,0x4044cc44,0x406423a6,2777,0xa427991d6d1fcfd8ULL,0xa2f2709904a41b20ULL},
        {1,55,12,false,true,0x3f741b2a,0x402be9bd,0x40691f56,2588,0x8de30ea50ab309e5ULL,0xb463e00d661948c3ULL},
        {0xffffffffffffffffULL,41,-3,true,true,0x3f19d0d8,0x3f4ab6ff,0x40e9d768,9740,0x524ebededd3fd087ULL,0xa715bd3c48d252ceULL},
        {123456789,47,7,false,false,0x3d596ba0,0x403ae0ec,0x406c2090,8857,0x9621c5f83ac27c02ULL,0x4ca5c3ab85e410afULL},
    };
    for (const auto& v : expected) {
        spire::UnseededRandom rng(v.seed);
        require(rng.random(99) == v.inclusive, "pinned inclusive int");
        require(rng.random(-7, 13) == v.ranged, "pinned ranged int");
        require(rng.randomBoolean() == v.boolean, "pinned boolean");
        require(rng.randomBoolean(0.375f) == v.chance, "pinned chance");
        require(float_bits(rng.random()) == v.unit, "pinned unit float");
        require(float_bits(rng.random(3.25f)) == v.scaled, "pinned scaled float");
        require(float_bits(rng.random(-2.5f, 7.75f)) == v.ranged_float, "pinned ranged float");
        require(rng.random(std::int64_t{9999}) == v.scaled_long, "pinned scaled long");
        require(rng.counter == 8 && rng.generator.s0 == v.final0 && rng.generator.s1 == v.final1, "pinned mixed draw state");
    }
    spire::UnseededRandom rejected(0xffffffffffffffffULL);
    require(rejected.nextLong(4611686018427387905LL) == 3412722986677424662LL, "pinned bounded long");
    require(rejected.counter == 2, "pinned rejection counts each raw nextLong");
    spire::UnseededRandom constructors(1);
    const auto constructed = spire::make_unseeded_random(constructors);
    require(constructed.counter == 5 && constructed.generator.s0 == 0x67ca77a31b4740ddULL && constructed.generator.s1 == 0x9aac60212bbec212ULL, "default Random constructor state");
    require(constructors.counter == 2 && constructors.generator.s0 == 0xb722f2631cb7b14dULL && constructors.generator.s1 == 0xab89235541870f7dULL, "default Random consumes two rule draws");
    const spire::Random source(42, 7);
    const auto copied = spire::copy_random(source, constructors);
    require(copied.counter == source.counter && copied.generator.s0 == source.generator.s0 && copied.generator.s1 == source.generator.s1, "Random.copy preserves stream");
    require(constructors.counter == 4, "Random.copy consumes default constructor draws");
    constexpr int orders[][12] = {
        {0,8,5,1,6,2,7,10,3,4,11,9},
        {8,3,6,4,0,7,2,5,1,10,9,11},
        {8,2,3,9,4,7,1,5,11,10,6,0},
    };
    spire::UnseededRandom shuffle_rng(0);
    for (int draw = 0; draw < 3; ++draw) {
        int cards[12];
        for (int i = 0; i < 12; ++i) cards[i] = i;
        spire::shuffle_unseeded(cards, 12, shuffle_rng);
        for (int i = 0; i < 12; ++i) require(cards[i] == orders[draw][i], "pinned Collections.shuffle order");
        require(shuffle_rng.counter == draw + 1, "pinned shuffle consumes one raw draw");
    }
    int only_card = 7;
    spire::shuffle_unseeded(&only_card, 0, shuffle_rng);
    spire::shuffle_unseeded(&only_card, 1, shuffle_rng);
    require(only_card == 7 && shuffle_rng.counter == 5, "empty and singleton pinned shuffle consume seeds");
}

int main() {
    test_game_random();
    test_rejection();
    test_shuffle();
    test_streams();
    test_unseeded();
    std::puts("RNG vectors, rejection, shuffle, stream seeding, and pinned rule stream passed");
}
