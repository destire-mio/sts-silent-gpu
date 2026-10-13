#include "host/run_state.hpp"
#include <cstdio>
#include <cstdlib>

static void require(bool condition, const char* field) {
    if (!condition) {
        std::fprintf(stderr, "Fresh run mismatch: %s\n", field);
        std::exit(1);
    }
}

int main() {
    struct Vector {
        std::uint64_t seed;
        std::int64_t monster[3], map[3], relic[3];
    };
    // Original JAR startup snapshots; scenario floor reseeding leaves these three streams intact.
    constexpr Vector vectors[] = {
        {1,{31,-1304625053744211988LL,-2510297468744452913LL},{94,3204412061179569318LL,-195014409968800471LL},{5,3173957875332129302LL,-4031002467881526275LL}},
        {104,{36,-5462089779753727613LL,-6819021634785095323LL},{98,-8125574174337671890LL,-4259502040931528428LL},{5,3593947107779400971LL,5200315013279975419LL}},
        {201,{32,516689160874106788LL,-9078339067177549702LL},{94,3665870469274836801LL,-4878907004603707874LL},{5,-2617413102550895381LL,-7456142652290018428LL}},
        {202,{33,855700651965451917LL,-1289026912527320117LL},{92,-5702369756633826661LL,-3883520980493091116LL},{5,-7631641600522246377LL,-6789121887924640040LL}},
        {203,{32,-6822866812491479596LL,7251187929428482721LL},{93,5779270541738023728LL,7280380584932819539LL},{5,2272504982086007125LL,5762380842074392675LL}},
    };
    for (const auto& vector : vectors) {
        spire::Game game;
        spire::reset(game, vector.seed);
        spire::host::FreshRunState run;
        spire::host::initialize_fresh_run(run);
        const spire::Random* streams[] = {&game.rng.monster, &game.rng.map, &game.rng.relic};
        const std::int64_t* states[] = {vector.monster, vector.map, vector.relic};
        for (int i = 0; i < 3; ++i) {
            require(streams[i]->counter == states[i][0], "startup RNG counter");
            require(spire::signed_long(streams[i]->generator.s0) == states[i][1], "startup RNG state0");
            require(spire::signed_long(streams[i]->generator.s1) == states[i][2], "startup RNG state1");
        }
        require(!game.rng.neow_initialized, "first Neow prompt leaves reward RNG null");
        require(game.rng.misc.counter == 1, "Exordium music selection consumes misc RNG");
        if (vector.seed != 1) continue;
        const auto encounters = [](const auto& list) {
            std::vector<std::string> values;
            for (int i = 0; i < list.size; ++i) values.push_back(spire::host::encounter_names[static_cast<int>(list[i])]);
            return values;
        };
        require(encounters(game.run.monster_list) == std::vector<std::string>{"Cultist", "Jaw Worm", "Small Slimes", "Exordium Wildlife", "2 Fungi Beasts", "3 Louse", "Looter", "Exordium Thugs", "Lots of Slimes", "3 Louse", "Blue Slaver", "Red Slaver", "2 Fungi Beasts", "Looter", "Exordium Wildlife", "Blue Slaver"}, "weak/strong weighted roll order");
        require(encounters(game.run.elite_list) == std::vector<std::string>{"Gremlin Nob", "3 Sentries", "Lagavulin", "3 Sentries", "Gremlin Nob", "Lagavulin", "Gremlin Nob", "Lagavulin", "Gremlin Nob", "3 Sentries"}, "elite repeat rejection");
        require(encounters(game.run.boss_list) == std::vector<std::string>{"The Guardian", "The Guardian"}, "fresh boss profile");
        require(run.relicsToRemoveOnStart == std::vector<std::string>{"Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater", "Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater", "Ring of the Snake"}, "cold-menu relic exclusion order");
        const auto& emerald = game.run.map[12][1];
        require(emerald.room == spire::RoomKind::Elite && emerald.emerald && emerald.edges == ((1 << 1) | (1 << 2)), "emerald elite and ordered outgoing edges");
        require(game.run.map[4][3].room == spire::RoomKind::Empty && game.run.map[4][3].edges == 0, "unconnected map room stays null");
        require(spire::signed_long(game.rng.misc.generator.s0) == 9038243705893100514LL && spire::signed_long(game.rng.misc.generator.s1) == -5250367708063092403LL, "startup misc RNG state");
    }
    // Original Recorder run-level starts, seeds 202/203/204, with all three keys.
    struct LaterAct {
        int act;
        std::int64_t monster[3], map[3];
        std::uint64_t map_hash;
    };
    constexpr LaterAct later[] = {
        {2, {63, 6188475007618794285LL, -2357458202950129441LL}, {99, 3672524263402578299LL, 4533119682836172242LL}, 9230394297464120242ULL},
        {3, {64, 6334402145808308614LL, -7519376247260663465LL}, {94, -2079116040172829365LL, 2600493413161874575LL}, 4386662735552125345ULL},
        {4, {34, 8167863081561544028LL, 6199929283779071388LL}, {0, -8912729707593333706LL, 7446794818026799569LL}, 4424878141246575987ULL},
    };
    for (const auto& vector : later) {
        spire::Game game;
        spire::reset(game, 200 + vector.act);
        game.run.act = vector.act;
        game.run.ruby_key = game.run.emerald_key = game.run.sapphire_key = true;
        spire::initialize_dungeon(game);
        const spire::Random* streams[] = {&game.rng.monster, &game.rng.map};
        const std::int64_t* states[] = {vector.monster, vector.map};
        for (int i = 0; i < 2; ++i) {
            require(streams[i]->counter == states[i][0], "later-act RNG counter");
            require(spire::signed_long(streams[i]->generator.s0) == states[i][1], "later-act RNG state0");
            require(spire::signed_long(streams[i]->generator.s1) == states[i][2], "later-act RNG state1");
        }
        std::uint64_t hash = 14695981039346656037ULL;
        for (int y = 0; y < (vector.act == 4 ? 5 : 15); ++y) for (const auto& node : game.run.map[y]) {
            const int values[] = {static_cast<int>(node.room), node.edges, node.emerald, node.taken};
            for (const int value : values) hash = (hash ^ value) * 1099511628211ULL;
        }
        require(hash == vector.map_hash, "later-act complete map, edges and key flags");
        require(game.run.event_list.size == (vector.act == 2 ? 13 : vector.act == 3 ? 7 : 0), "later-act event pool");
        require(game.run.shrine_list.size == (vector.act == 4 ? 0 : 6), "later-act shrine pool");
    }
    std::puts("A20 dungeon RNG, encounter lists, relic exclusions, and maps for all four acts passed");
}
