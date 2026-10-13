#pragma once
#include "state.hpp"

namespace spire {
struct MapParents {
    List<uint8_t, 6> entries{};
    uint8_t minimum = 7, maximum = 0;
    SPIRE_FN void add(int x) {
        entries.push_back(x);
        minimum = min(minimum, x);
        maximum = max(maximum, x);
    }
};

SPIRE_FN inline int map_ancestor(const MapParents (&parents)[15][7], int first, int second, int row) {
    // MapGenerator.getCommonAncestor compares node1.x with node2.y.
    int left = first < row ? first : second;
    int right = first < row ? second : first;
    for (int y = row; y >= 0 && y >= row - 5; --y) {
        const auto& lp = parents[y][left];
        const auto& rp = parents[y][right];
        if (lp.entries.empty() || rp.entries.empty()) return -1;
        left = lp.maximum;
        right = rp.minimum;
        if (left == right) return y - 1;
    }
    return -1;
}

SPIRE_FN inline unsigned map_room_bit(RoomKind room) { return 1u << static_cast<int>(room); }

SPIRE_FN inline unsigned map_forbidden_rooms(const Run& run, const MapParents (&parents)[15][7], int x, int y) {
    const unsigned rest = map_room_bit(RoomKind::Rest), elite = map_room_bit(RoomKind::Elite);
    const unsigned shop = map_room_bit(RoomKind::Shop);
    const unsigned parent_rule = rest | elite | shop | map_room_bit(RoomKind::Treasure);
    const unsigned sibling_rule = rest | elite | shop | map_room_bit(RoomKind::Monster) | map_room_bit(RoomKind::Event);
    unsigned forbidden = y <= 4 ? rest | elite : y >= 13 ? rest : 0;
    unsigned siblings = 0;
    const auto& entries = parents[y][x].entries;
    for (int i = 0; i < entries.size; ++i) {
        const auto& parent = run.map[y - 1][entries[i]];
        forbidden |= map_room_bit(parent.room) & parent_rule;
        siblings |= parent.edges;
    }
    siblings &= ~(1u << x);
    for (int other = 0; other < 7; ++other)
        if (siblings & (1u << other)) forbidden |= map_room_bit(run.map[y][other].room) & sibling_rule;
    return forbidden;
}

SPIRE_FN inline void generate_map(Game& g) {
    Run& run = g.run;
    Random& rng = g.rng.map;
    for (auto& row : run.map) for (auto& node : row) node = MapNode{};
    if (run.act == 4) {
        const RoomKind rooms[] = {RoomKind::Rest, RoomKind::Shop, RoomKind::Elite, RoomKind::Boss, RoomKind::Victory};
        for (int y = 0; y < 5; ++y) {
            run.map[y][3].room = rooms[y];
            if (y < 3) run.map[y][3].edges = 1 << 3;
        }
        return;
    }
    MapParents parents[15][7]{};
    int first = -1;
    for (int path = 0; path < 6; ++path) {
        int x = rng.random(0, 6);
        if (path == 0) first = x;
        while (path == 1 && x == first) x = rng.random(0, 6);
        for (int y = 0; y < 15; ++y) {
            MapNode& current = run.map[y][x];
            if (y == 14) { current.edges |= 1 << 3; break; }
            int next = x + rng.random(x == 0 ? 0 : -1, x == 6 ? 0 : 1);
            const auto& target_parents = parents[y + 1][next].entries;
            for (int i = 0; i < target_parents.size; ++i) {
                const int parent = target_parents[i];
                if (parent == x) continue;
                const int shared = map_ancestor(parents, parent, x, y);
                if (shared < 0 || y + 1 - shared >= 3) continue;
                if (next > x) {
                    next = x + rng.random(-1, 0);
                    if (next < 0) next = x;
                } else if (next == x) {
                    next = x + rng.random(-1, 1);
                    if (next > 6) next = x - 1;
                    else if (next < 0) next = x + 1;
                } else {
                    next = x + rng.random(0, 1);
                    if (next > 6) next = x;
                }
            }
            if (x > 0) {
                const unsigned edges = run.map[y][x - 1].edges;
                if (edges) next = max(next, 31 - __builtin_clz(edges));
            }
            if (x < 6) {
                const unsigned edges = run.map[y][x + 1].edges;
                if (edges) next = min(next, __builtin_ctz(edges));
            }
            current.edges |= 1 << next;
            parents[y + 1][next].add(x);
            x = next;
        }
    }
    uint8_t seen = 0;
    for (auto& node : run.map[0]) {
        const uint8_t edges = node.edges;
        node.edges &= ~seen;
        seen |= edges;
    }
    int count = 0;
    for (int y = 0; y < 15; ++y) for (const auto& node : run.map[y]) if (node.edges && y != 13) ++count;
    List<RoomKind, 105> rooms{};
    const RoomKind kinds[] = {RoomKind::Shop, RoomKind::Rest, RoomKind::Elite, RoomKind::Event};
    const int counts[] = {static_cast<int>(count * 0.05f + 0.5f), static_cast<int>(count * 0.12f + 0.5f),
        static_cast<int>(count * 0.08f * 1.6f + 0.5f), static_cast<int>(count * 0.22f + 0.5f)};
    for (int kind = 0; kind < 4; ++kind) for (int i = 0; i < counts[kind]; ++i) rooms.push_back(kinds[kind]);
    for (auto& node : run.map[14]) node.room = RoomKind::Rest;
    for (auto& node : run.map[0]) node.room = RoomKind::Monster;
    for (auto& node : run.map[8]) node.room = RoomKind::Treasure;
    int unassigned = 0;
    for (const auto& row : run.map) for (const auto& node : row) if (node.edges && node.room == RoomKind::Empty) ++unassigned;
    while (rooms.size < unassigned) rooms.push_back(RoomKind::Monster);
    // RoomTypeAssigner.distributeRoomsAcrossMap advances RandomXS128 without the Random counter.
    for (int i = rooms.size; i > 1; --i) {
        const int index = rng.generator.nextInt(i);
        const RoomKind room = rooms[i - 1];
        rooms[i - 1] = rooms[index];
        rooms[index] = room;
    }
    for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
        auto& node = run.map[y][x];
        if (!node.edges || node.room != RoomKind::Empty) continue;
        const unsigned forbidden = map_forbidden_rooms(run, parents, x, y);
        for (int i = 0; i < rooms.size; ++i) {
            if (forbidden & map_room_bit(rooms[i])) continue;
            node.room = rooms.erase(i);
            break;
        }
    }
    List<uint8_t, 105> elites{};
    for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
        auto& node = run.map[y][x];
        if (node.edges && node.room == RoomKind::Empty) node.room = RoomKind::Monster;
        if (node.room == RoomKind::Elite) elites.push_back(y * 7 + x);
    }
    if (!run.emerald_key) {
        const int chosen = elites[rng.random(0, elites.size - 1)];
        run.map[chosen / 7][chosen % 7].emerald = true;
    }
}
}
