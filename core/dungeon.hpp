#pragma once
#include "map.hpp"
#include "cards.hpp"

namespace spire {
struct EncounterWeight { EncounterId id; float weight; };

template<int Size>
SPIRE_FN inline void normalize_encounters(EncounterWeight (&entries)[Size]) {
    for (int i = 1; i < Size; ++i) {
        const EncounterWeight value = entries[i];
        int j = i;
        while (j > 0 && entries[j - 1].weight > value.weight) { entries[j] = entries[j - 1]; --j; }
        entries[j] = value;
    }
    float total = 0.0f;
    for (const auto& entry : entries) total += entry.weight;
    for (auto& entry : entries) entry.weight /= total;
}

template<int Size>
SPIRE_FN inline EncounterId roll_encounter(Random& rng, const EncounterWeight (&entries)[Size]) {
    const float roll = rng.random();
    float cumulative = 0.0f;
    for (const auto& entry : entries) {
        cumulative += entry.weight;
        if (roll < cumulative) return entry.id;
    }
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}

template<int Size, int Capacity>
SPIRE_FN inline void populate_encounters(Random& rng, EncounterWeight (&entries)[Size], List<EncounterId, Capacity>& list, int count, bool elites) {
    normalize_encounters(entries);
    const int end = list.size + count;
    while (list.size < end) {
        const EncounterId next = roll_encounter(rng, entries);
        if (!list.empty() && list[list.size - 1] == next) continue;
        if (!elites && list.size > 1 && list[list.size - 2] == next) continue;
        list.push_back(next);
    }
}

SPIRE_FN inline bool first_strong_excluded(EncounterId previous, EncounterId next) {
    switch (previous) {
    case EncounterId::Looter: return next == EncounterId::ExordiumThugs;
    case EncounterId::BlueSlaver: return next == EncounterId::RedSlaver || next == EncounterId::ExordiumThugs;
    case EncounterId::TwoLouse: return next == EncounterId::ThreeLouse;
    case EncounterId::SmallSlimes: return next == EncounterId::LargeSlime || next == EncounterId::LotsOfSlimes;
    case EncounterId::SphericGuardian: return next == EncounterId::SentryAndSphere;
    case EncounterId::Byrds: return next == EncounterId::ChosenAndByrds;
    case EncounterId::Chosen: return next == EncounterId::ChosenAndByrds || next == EncounterId::CultistAndChosen;
    case EncounterId::Darkling: return next == EncounterId::Darkling;
    case EncounterId::OrbWalker: return next == EncounterId::OrbWalker;
    case EncounterId::ThreeShapes: return next == EncounterId::FourShapes;
    default: return false;
    }
}

template<int Size>
SPIRE_FN inline void populate_strong_encounters(Game& g, EncounterWeight (&entries)[Size]) {
    normalize_encounters(entries);
    EncounterId next;
    do { next = roll_encounter(g.rng.monster, entries); }
    while (first_strong_excluded(g.run.monster_list[g.run.monster_list.size - 1], next));
    g.run.monster_list.push_back(next);
    const int end = g.run.monster_list.size + 12;
    while (g.run.monster_list.size < end) {
        next = roll_encounter(g.rng.monster, entries);
        const auto& list = g.run.monster_list;
        if (list[list.size - 1] != next && (list.size < 2 || list[list.size - 2] != next)) g.run.monster_list.push_back(next);
    }
}

SPIRE_FN inline void generate_encounters(Game& g) {
    auto& run = g.run;
    run.monster_list.clear();
    run.elite_list.clear();
    run.boss_list.clear();
    // Exordium/TheCity/TheBeyond.initializeBoss use the recorder's fresh boss-seen profile.
    if (run.act == 1) {
        EncounterWeight weak[] = {{EncounterId::Cultist, 2}, {EncounterId::JawWorm, 2}, {EncounterId::TwoLouse, 2}, {EncounterId::SmallSlimes, 2}};
        populate_encounters(g.rng.monster, weak, run.monster_list, 3, false);
        EncounterWeight strong[] = {{EncounterId::BlueSlaver, 2}, {EncounterId::GremlinGang, 1}, {EncounterId::Looter, 2}, {EncounterId::LargeSlime, 2}, {EncounterId::LotsOfSlimes, 1}, {EncounterId::ExordiumThugs, 1.5f}, {EncounterId::ExordiumWildlife, 1.5f}, {EncounterId::RedSlaver, 1}, {EncounterId::ThreeLouse, 2}, {EncounterId::TwoFungiBeasts, 2}};
        populate_strong_encounters(g, strong);
        EncounterWeight elites[] = {{EncounterId::GremlinNob, 1}, {EncounterId::Lagavulin, 1}, {EncounterId::ThreeSentries, 1}};
        populate_encounters(g.rng.monster, elites, run.elite_list, 10, true);
        run.boss_list.push_back(EncounterId::TheGuardian);
        run.boss_list.push_back(EncounterId::TheGuardian);
    } else if (run.act == 2) {
        EncounterWeight weak[] = {{EncounterId::SphericGuardian, 2}, {EncounterId::Chosen, 2}, {EncounterId::ShellParasite, 2}, {EncounterId::Byrds, 2}, {EncounterId::TwoThieves, 2}};
        populate_encounters(g.rng.monster, weak, run.monster_list, 2, false);
        EncounterWeight strong[] = {{EncounterId::ChosenAndByrds, 2}, {EncounterId::SentryAndSphere, 2}, {EncounterId::SnakePlant, 6}, {EncounterId::Snecko, 4}, {EncounterId::CenturionAndHealer, 6}, {EncounterId::CultistAndChosen, 3}, {EncounterId::ThreeCultists, 3}, {EncounterId::ShelledParasiteAndFungi, 3}};
        populate_strong_encounters(g, strong);
        EncounterWeight elites[] = {{EncounterId::GremlinLeader, 1}, {EncounterId::Slavers, 1}, {EncounterId::BookOfStabbing, 1}};
        populate_encounters(g.rng.monster, elites, run.elite_list, 10, true);
        run.boss_list.push_back(EncounterId::Champ);
        run.boss_list.push_back(EncounterId::Champ);
    } else if (run.act == 3) {
        EncounterWeight weak[] = {{EncounterId::Darkling, 2}, {EncounterId::OrbWalker, 2}, {EncounterId::ThreeShapes, 2}};
        populate_encounters(g.rng.monster, weak, run.monster_list, 2, false);
        EncounterWeight strong[] = {{EncounterId::SpireGrowth, 1}, {EncounterId::Transient, 1}, {EncounterId::FourShapes, 1}, {EncounterId::Maw, 1}, {EncounterId::SphereAndTwoShapes, 1}, {EncounterId::JawWormHorde, 1}, {EncounterId::Darkling, 1}, {EncounterId::WrithingMass, 1}};
        populate_strong_encounters(g, strong);
        EncounterWeight elites[] = {{EncounterId::GiantHead, 2}, {EncounterId::Nemesis, 2}, {EncounterId::Reptomancer, 2}};
        populate_encounters(g.rng.monster, elites, run.elite_list, 10, true);
        run.boss_list.push_back(EncounterId::AwakenedOne);
        run.boss_list.push_back(EncounterId::AwakenedOne);
    } else {
        SPIRE_ASSERT(run.act == 4);
        for (int i = 0; i < 3; ++i) {
            run.monster_list.push_back(EncounterId::ShieldAndSpear);
            run.elite_list.push_back(EncounterId::ShieldAndSpear);
            run.boss_list.push_back(EncounterId::TheHeart);
        }
    }
}

SPIRE_FN inline void initialize_relic_pools(Game& g) {
    const RelicKey* pools[] = {initial_common_relic_pool, initial_uncommon_relic_pool, initial_rare_relic_pool, initial_shop_relic_pool, initial_boss_relic_pool};
    const int sizes[] = {sizeof(initial_common_relic_pool), sizeof(initial_uncommon_relic_pool), sizeof(initial_rare_relic_pool), sizeof(initial_shop_relic_pool), sizeof(initial_boss_relic_pool)};
    for (int p = 0; p < 5; ++p) {
        auto& pool = g.run.relic_pools[p];
        pool.clear();
        for (int i = 0; i < sizes[p]; ++i) pool.push_back(pools[p][i]);
        shuffle(pool.values, pool.size, g.rng.relic);
    }
}

SPIRE_FN inline void initialize_dungeon(Game& g) {
    generate_encounters(g);
    g.run.boss_key = g.run.boss_list[0];
    g.run.event_list.clear();
    g.run.shrine_list.clear();
    if (g.run.act == 1) {
        const EventKey events[] = {EventKey::BigFish, EventKey::Cleric, EventKey::DeadAdventurer, EventKey::GoldenIdolEvent, EventKey::GoldenWing, EventKey::GoopPuddle, EventKey::Sssserpent, EventKey::LivingWall, EventKey::Mushrooms, EventKey::ScrapOoze, EventKey::ShiningLight};
        for (const auto event : events) g.run.event_list.push_back(event);
        const EventKey special[] = {EventKey::AccursedBlacksmith, EventKey::Bonfire, EventKey::Designer, EventKey::Duplicator, EventKey::FaceTrader, EventKey::FountainOfCurseRemoval, EventKey::KnowingSkull, EventKey::Lab, EventKey::Nloth, EventKey::SecretPortal, EventKey::TheJoust, EventKey::WeMeetAgain, EventKey::WomanInBlue};
        g.run.special_events.clear();
        for (const auto event : special) g.run.special_events.push_back(event);
    } else if (g.run.act == 2) {
        const EventKey events[] = {EventKey::Addict, EventKey::BackToBasics, EventKey::Beggar, EventKey::Colosseum, EventKey::CursedTome, EventKey::DrugDealer, EventKey::ForgottenAltar, EventKey::Ghosts, EventKey::MaskedBandits, EventKey::Nest, EventKey::TheLibrary, EventKey::TheMausoleum, EventKey::Vampires};
        for (const auto event : events) g.run.event_list.push_back(event);
    } else if (g.run.act == 3) {
        const EventKey events[] = {EventKey::Falling, EventKey::MindBloom, EventKey::MoaiHead, EventKey::MysteriousSphere, EventKey::SensoryStone, EventKey::TombRedMask, EventKey::WindingHalls};
        for (const auto event : events) g.run.event_list.push_back(event);
    }
    if (g.run.act < 4) {
        g.run.shrine_list.push_back(EventKey::GremlinMatchGame);
        if (g.run.act != 1) g.run.shrine_list.push_back(EventKey::GremlinWheelGame);
        const EventKey shrines[] = {EventKey::GoldShrine, EventKey::Transmogrifier, EventKey::PurificationShrine, EventKey::UpgradeShrine};
        for (const auto event : shrines) g.run.shrine_list.push_back(event);
        if (g.run.act == 1) g.run.shrine_list.push_back(EventKey::GremlinWheelGame);
    }
    g.run.card_upgraded_chance = g.run.act == 1 ? 0.0f : g.run.act == 2 ? 0.125f : 0.25f;
    if (g.run.act == 1) initialize_relic_pools(g);
    g.rng.reseed_map(g.run.seed, g.run.act);
    generate_map(g);
    if (g.run.act != 4) g.rng.misc.random(1);
}
}
