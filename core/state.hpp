#pragma once
#include "core/fixed.hpp"
#include "core/random.hpp"
#include "core/types.hpp"
#include "spire_generated/ids.hpp"

namespace spire {
static_assert(static_cast<int>(CardId::Count) <= 512, "Card IDs exceed their 9-bit storage budget");
struct Card {
    int misc = 0;
    int base_damage = -1;
    int base_block_adjust = 0;
    int damage = -1;
    int block = -1;
    int16_t cost = 0;
    int16_t cost_for_turn = 0;
    union { int16_t energy_on_use = 0; int16_t price; };
    uint16_t uuid = 0;
    uint16_t upgrades = 0;
    int16_t base_magic = -1;
    int16_t magic = -1;
    CardId id : 9;
    uint8_t multi_damage_count : 7;
    bool upgraded_cost : 1;
    CardTarget target : 3;
    bool ignore_energy_on_use : 1;
    bool exhaust : 1;
    bool ethereal : 1;
    bool retain : 1;
    bool self_retain : 1;
    bool innate : 1;
    bool free_to_play_once : 1;
    bool exhaust_on_use_once : 1;
    bool dont_trigger_on_use_card : 1;
    bool in_autoplay : 1;
    bool purge_on_use : 1;
    bool shuffle_back_into_draw_pile : 1;
    bool return_to_hand : 1;
    bool bottle_flame : 1;
    bool bottle_lightning : 1;
    bool bottle_tornado : 1;
    bool is_flipped : 1;
    bool priced : 1;
    bool master_reference : 1;
    uint8_t padding : 1;
    CardState state{};
    SPIRE_FN SPIRE_INLINE constexpr Card()
        : id(CardId::Strike_G), multi_damage_count(0), upgraded_cost(false), target(CardTarget::Enemy), ignore_energy_on_use(false), exhaust(false), ethereal(false), retain(false),
          self_retain(false), innate(false), free_to_play_once(false), exhaust_on_use_once(false),
          dont_trigger_on_use_card(false), in_autoplay(false), purge_on_use(false),
          shuffle_back_into_draw_pile(false), return_to_hand(false),
          bottle_flame(false), bottle_lightning(false), bottle_tornado(false), is_flipped(false), priced(false), master_reference(false), padding(0) {}
};
struct CardArena {
    Card values[card_capacity]{};
    uint64_t used[(card_capacity + 63) / 64]{};
    uint16_t size = 0;
    uint16_t free_head = card_capacity;
    SPIRE_FN SPIRE_INLINE bool contains(int index) const {
        return index >= 0 && index < size && (used[index / 64] & (uint64_t{1} << (index % 64))) != 0;
    }
    SPIRE_FN SPIRE_INLINE Card& operator[](int index) { SPIRE_ASSERT(contains(index)); return values[index]; }
    SPIRE_FN SPIRE_INLINE const Card& operator[](int index) const { SPIRE_ASSERT(contains(index)); return values[index]; }
    SPIRE_FN SPIRE_INLINE int count() const {
        int result = 0;
        for (auto word : used) result += __builtin_popcountll(word);
        return result;
    }
    SPIRE_FN SPIRE_INLINE int add(const Card& card) {
        const int index = free_head != card_capacity ? free_head : size;
        SPIRE_ASSERT(index < card_capacity);
        if (free_head != card_capacity) free_head = values[index].misc;
        else ++size;
        values[index] = card;
        used[index / 64] |= uint64_t{1} << (index % 64);
        return index;
    }
    SPIRE_FN SPIRE_INLINE void release(int index) {
        SPIRE_ASSERT(contains(index));
        auto* bytes = reinterpret_cast<unsigned char*>(&values[index]);
        for (unsigned i = 0; i < sizeof(Card); ++i) bytes[i] = 0xa5;
        values[index].id = CardId::Count;
        values[index].misc = free_head;
        free_head = index;
        used[index / 64] &= ~(uint64_t{1} << (index % 64));
    }
    SPIRE_FN SPIRE_INLINE void clear() {
        for (auto& word : used) word = 0;
        size = 0;
        free_head = card_capacity;
    }
};
using Pile = List<uint8_t, card_capacity>;
struct CampfireChoice { CampfireOption kind; bool usable; };
using CampfireOptions = List<CampfireChoice, 8>;
using EventChoices = List<EventChoice, 16>;
struct CampfireState {
    CampfireOptions options{};
    CampfireOption selected = CampfireOption::Count;
    bool something_selected = false;
};
struct ChestState {
    List<uint8_t, 3> relics{};
    uint8_t size = 0;
    RelicTier relic = RelicTier::Common;
    bool open = false;
    bool gold = false;
    bool cursed = false;
    bool chose_relic = false;
};
struct StoreItem { uint8_t reward, slot; int16_t price; };
struct ShopState {
    List<uint8_t, 5> colored{};
    List<uint8_t, 2> colorless{};
    List<StoreItem, 3> relics{}, potions{};
    bool purge_available = true;
};
struct Relic {
    RelicId id;
    bool grayscale = false;
    bool used_up = false;
    uint8_t padding = 0;
    int counter;
    RelicState state{};
    SPIRE_FN SPIRE_INLINE constexpr Relic(RelicId id = RelicId::SnakeRing, int counter = -1) : id(id), counter(counter) {}
};
struct Power {
    int amount;
    PowerState state{};
    int16_t id_offset = -1;
    PowerId id;
    uint8_t next = 255;
    uint32_t instance : 29;
    bool just_applied : 1;
    bool triggered : 1;
    bool skip_first : 1;
    SPIRE_FN SPIRE_INLINE constexpr Power(PowerId id = PowerId::Strength, int amount = 0)
        : amount(amount), id(id), instance(0), just_applied(false), triggered(false), skip_first(true) {}
};
struct Powers {
    uint8_t head = 255;
    uint8_t size = 0;
    template<class Objects> SPIRE_FN SPIRE_INLINE int index(const Objects& objects, int position) const {
        SPIRE_ASSERT(position >= 0 && position < size);
        int current = head;
        while (position--) current = objects[current].next;
        return current;
    }
    template<class Objects> SPIRE_FN SPIRE_INLINE auto& at(Objects& objects, int position) { return objects[index(objects, position)]; }
    template<class Objects> SPIRE_FN SPIRE_INLINE const auto& at(const Objects& objects, int position) const { return objects[index(objects, position)]; }
    template<class Objects> SPIRE_FN SPIRE_INLINE void insert(Objects& objects, int position, int value) {
        SPIRE_ASSERT(position >= 0 && position <= size && size < power_capacity);
        if (position == 0) { objects[value].next = head; head = value; }
        else {
            auto& previous = at(objects, position - 1);
            objects[value].next = previous.next;
            previous.next = value;
        }
        ++size;
    }
    template<class Objects> SPIRE_FN SPIRE_INLINE void push_back(Objects& objects, int value) { insert(objects, size, value); }
    template<class Objects> SPIRE_FN SPIRE_INLINE void erase(Objects& objects, int position) {
        SPIRE_ASSERT(position >= 0 && position < size);
        if (position == 0) head = objects[head].next;
        else { auto& previous = at(objects, position - 1); previous.next = objects[previous.next].next; }
        --size;
    }
    SPIRE_FN SPIRE_INLINE void clear() { head = 255; size = 0; }
    SPIRE_FN SPIRE_INLINE bool empty() const { return size == 0; }
};
struct Creature {
    float draw_x = 0.0f;
    int hp = 0;
    int max_hp = 0;
    int block = 0;
    int last_damage_taken = 0;
    int gold = 0;
    Powers powers{};
    bool dead : 1;
    bool dying : 1;
    bool escaped : 1;
    bool flip_horizontal : 1;
    bool half_dead : 1;
    SPIRE_FN SPIRE_INLINE constexpr Creature() : dead(false), dying(false), escaped(false), flip_horizontal(false), half_dead(false) {}
};
struct Monster {
    Creature creature{};
    float offset_x = 0.0f;
    MonsterState state{};
    MoveHistory move_history{};
    int16_t damage[3]{};
    int16_t damage_output[3]{};
    int16_t intent_base_damage = -1;
    int16_t intent_damage = -1;
    int16_t intent_hits = 0;
    int16_t pending_base_damage = -1;
    int16_t pending_hits = 0;
    uint16_t move_name = 0;
    MonsterId id = MonsterId::Cultist;
    Intent intent = Intent::Debug;
    int8_t move = -1;
    int8_t pending_move = -1;
    Intent pending_intent = Intent::None;
    uint8_t damage_extra = 0;
    uint8_t damage_modified = 0;
    uint8_t damage_count : 4;
    bool pending_multi : 1;
    bool intent_multi : 1;
    bool cannot_escape : 1;
    bool escape_next : 1;
    SPIRE_FN SPIRE_INLINE constexpr Monster() : damage_count(1), pending_multi(false), intent_multi(false), cannot_escape(false), escape_next(false) {}
};
struct Orb {
    int passive = 0;
    int evoke = 0;
};
struct OrbState {
    List<Orb, 128> objects{};
    uint8_t kinds[32]{};
    List<int8_t, 10> slots{};
    List<uint8_t, 256> channeled{};
    uint16_t turn_start = 0;
    SPIRE_FN SPIRE_INLINE OrbKind kind(int index) const {
        SPIRE_ASSERT(index >= 0 && index < objects.size);
        return static_cast<OrbKind>((kinds[index / 4] >> ((index % 4) * 2)) & 3);
    }
    SPIRE_FN SPIRE_INLINE int add(OrbKind kind, int passive, int evoke) {
        const int index = objects.size;
        objects.push_back({passive, evoke});
        const int shift = (index % 4) * 2;
        kinds[index / 4] = (kinds[index / 4] & ~(3 << shift)) | (static_cast<int>(kind) << shift);
        return index;
    }
};
struct Player {
    Creature creature{};
    int energy = 0;
    int energy_master = 3;
    int energy_per_turn = 3;
    int hand_size = 5;
    int master_hand_size = 5;
    int master_max_orbs = 0;
    int max_orbs = 0;
    int card_in_use = -1;
    int cards_played_this_turn = 0;
    int damaged_this_combat = 0;
    Stance stance = Stance::Neutral;
    bool bloodied = false;
    bool end_turn_queued = false;
    bool escaping = false;
    float escape_timer = 0.0f;
};
enum class ActionKind : uint8_t { Play, EndTurn, Choose, PotionUse, PotionDiscard, Proceed, Skip };
struct Action {
    ActionKind kind = ActionKind::EndTurn;
    int index = -1;
    int target = -1;
};
using ActionSet = List<Action, 128>;
struct PreparedPotion { PotionState state; PotionId id; };
union EffectPayload {
    EffectState state;
    Power power;
    PreparedPotion potion;
    SPIRE_FN SPIRE_INLINE constexpr EffectPayload() : state{} {}
    SPIRE_FN SPIRE_INLINE constexpr EffectPayload(const Power& value) : power(value) {}
    SPIRE_FN SPIRE_INLINE constexpr EffectPayload(const PreparedPotion& value) : potion(value) {}
};
struct Effect {
    int amount = 0;
    int info_base = 0;
    int extra = 0;
    EffectPayload payload{};
    int16_t card = -1;
    PowerId power = PowerId::Strength;
    EffectKind kind = EffectKind::Draw;
    int8_t source = player_target;
    int8_t target = player_target;
    DamageType damage_type = DamageType::Normal;
    bool power_constructed : 1;
    bool done : 1;
    bool held : 1;
    bool flag : 1;
    bool end_turn_draw : 1;
    uint8_t padding : 3;
    SPIRE_FN SPIRE_INLINE constexpr Effect() : power_constructed(false), done(false), held(false), flag(false), end_turn_draw(false), padding(0) {}
};
using DamageInfo = Effect;
struct CardQueueItem {
    int16_t card;
    int16_t energy_on_use;
    int8_t target;
    bool ignore_energy_total : 1;
    bool autoplay_card : 1;
    bool random_target : 1;
    bool is_end_turn_auto_play : 1;
    SPIRE_FN SPIRE_INLINE constexpr CardQueueItem(int card = -1, int target = no_target, int energy = 0,
                                    bool ignore = false, bool autoplay = false, bool random = false, bool end_turn = false)
        : card(card), energy_on_use(energy), target(target), ignore_energy_total(ignore), autoplay_card(autoplay),
          random_target(random), is_end_turn_auto_play(end_turn) {}
};

struct PlayedCards {
    uint8_t values[1024]{};
    uint16_t size = 0;
    uint16_t turn_start = 0;
    SPIRE_FN SPIRE_INLINE void record(int card) {
        SPIRE_ASSERT(size < 1024);
        values[size++] = static_cast<uint8_t>(card);
    }
    SPIRE_FN SPIRE_INLINE void begin_turn() {
        turn_start = size;
    }
    SPIRE_FN SPIRE_INLINE void clear() {
        size = 0;
        begin_turn();
    }
};
struct ActionManager {
    int8_t current_action = -1;
    bool has_current_action = false;
    bool has_control = true;
    bool using_card = false;
    Queue<uint8_t, action_capacity> actions{};
    Queue<uint8_t, action_capacity> pre_turn_actions{};
    Queue<CardQueueItem, 32> card_queue{};
    Queue<uint8_t, monster_capacity> monster_queue{};
    PlayedCards played_cards{};
    Phase phase = Phase::WaitingOnUser;
    bool turn_has_ended = false;
    bool monster_attacks_queued = true;
    int total_discarded_this_turn = 0;
    int damage_received_this_turn = 0;
    int damage_received_this_combat = 0;
    int hp_loss_this_combat = 0;
    int player_hp_last_turn = 0;
    int energy_gained_this_combat = 0;
    int mantra_gained = 0;
};
struct DialogOption { uint8_t slot; bool disabled; };
struct Event {
    EventId id = EventId::Count;
    EventState state{};
    int screen = 0;
    bool has_focus = true;
    bool no_cards = false;
};
struct MapNode {
    uint8_t edges = 0;
    RoomKind room : 4;
    bool emerald : 1;
    bool taken : 1;
    SPIRE_FN SPIRE_INLINE constexpr MapNode() : room(RoomKind::Empty), emerald(false), taken(false) {}
};
struct RunProfile {
    int neow_spirits = 1;
    uint16_t bosses_seen = 0;
    bool seeded = true;
    bool playtime_at_least_800 = false;
};
struct Run {
    uint64_t seed = 0;
    int floor = 0;
    int act = 1;
    int ascension = 20;
    int boss_count = 0;
    RunProfile profile{};
    float event_chances[4] = {0.0f, 0.1f, 0.03f, 0.02f};
    int8_t node_x = 0, node_y = -1;
    int8_t next_x = -1, next_y = -1;
    bool first_room_chosen = false;
    bool node_on_map = false;
    bool map_dismissable = true;
    List<int8_t, 64> path_x{}, path_y{};
    int gold = 99;
    int card_blizz_randomizer = 5;
    int potion_blizz_modifier = 0;
    int purge_cost = 75;
    int actual_purge_cost = 75;
    float card_upgraded_chance = 0.0f;
    bool ruby_key = false;
    bool emerald_key = false;
    bool sapphire_key = false;
    MapNode map[15][7]{};
    List<EncounterId, 16> monster_list{};
    List<EncounterId, 10> elite_list{};
    List<EncounterId, 3> boss_list{};
    EncounterId boss_key = EncounterId::TheGuardian;
    List<EventKey, 16> event_list{};
    List<EventKey, 8> shrine_list{};
    List<EventKey, 16> special_events{};
    List<Card, master_deck_capacity> master_deck{};
    List<uint8_t, card_pool_capacity> source_card_pools[4]{};
    List<uint8_t, 35> colorless_pool{};
    List<Relic, 64> relics{};
    List<RelicKey, 64> relic_pools[5]{};
    int potion_slots = 2;
    PotionId potions[potion_capacity]{};
    PotionState potion_state[potion_capacity]{};
    int potion_identity[potion_capacity]{};
};
enum class RewardKind : uint8_t { Card, Gold, Potion, Relic, StolenGold, SapphireKey, EmeraldKey };
struct RoomReward {
    int amount = 0;
    int bonus_gold = 0;
    PotionState potion_state{};
    Relic relic{RelicId::Count};
    List<uint8_t, reward_card_capacity> cards{};
    uint16_t identity = 0;
    int8_t relic_link = -1;
    int8_t relic_slot = -1;
    int8_t potion_slot = -1;
    RewardKind kind = RewardKind::Card;
    PotionId potion = PotionId::Empty;
    bool done : 1;
    bool ignored : 1;
    SPIRE_FN SPIRE_INLINE constexpr RoomReward() : done(false), ignored(false) {}
};
struct CardDamage {
    int base[card_capacity]{};
    uint8_t head[card_capacity]{};
    SPIRE_FN SPIRE_INLINE DamageArray get(int card) const { SPIRE_ASSERT(card >= 0 && card < card_capacity); return {base[card], head[card]}; }
    SPIRE_FN SPIRE_INLINE void set(int card, const DamageArray& damage) { SPIRE_ASSERT(card >= 0 && card < card_capacity); base[card] = damage.base; head[card] = damage.head; }
    SPIRE_FN SPIRE_INLINE void clear() { for (auto& value : head) value = 0; }
};

struct MonsterDamage { int16_t base; int16_t output; };
struct Game {
    uint32_t fault = 0;
    TextStore text{};
    Run run{};
    UnitStatics unit_statics{};
    RngStreams rng{};
    UnseededRandom unseeded{0};
    Player player{};
    OrbState orbs{};
    int stance_changes[4]{};
    CardArena cards{};
    List<uint8_t, 64> detached_master_cards{};
    uint64_t pending_card_moves[(card_capacity + 63) / 64]{};
    CardDamage multi_damage{};
    DamageExceptions damage_exceptions{};
    Pile hand{}, draw{}, discard{}, exhaust{}, selected_cards{}, limbo{}, grid_cards{};
    Pool<Effect, action_capacity> action_objects{};
    Pool<Power, power_object_capacity> power_objects{};
    uint64_t deaths_before_monster_turn = 0;
    uint16_t death_wait_bound[monster_capacity]{};
    bool player_turn_effect_pending = false;
    Pile selection_excluded{};
    List<uint8_t, hand_limit> drawn_cards{};
    List<uint8_t, 8> pending_obtains{};
    List<PreparedPotion, potion_capacity> potion_effects{};
    List<uint8_t, reward_card_capacity> reward_cards{};
    List<RoomReward, room_reward_capacity> reward_objects{};
    List<uint8_t, room_reward_capacity> room_rewards{};
    List<uint8_t, room_reward_capacity> screen_rewards{};
    int next_reward_identity = 1;
    int acquisition_reward = -1;
    int discovery_card = -1;
    MoveHistories move_histories{};
    List<Monster, monster_capacity> monsters{};
    List<MonsterDamage, 16> monster_damage_extra{};
    List<uint8_t, monster_capacity> monster_order{};
    ActionManager manager{};
    Screen screen = Screen::None;
    Screen previous_screen = Screen::None;
    Event event{};
    CampfireState campfire{};
    ChestState chest{};
    ShopState shop{};
    List<DialogOption, 8> room_dialog{}, image_dialog{};
    int room_selected_option = -1, image_selected_option = -1;
    EncounterId encounter = EncounterId::Cultist;
    EncounterState encounter_state{};
    EncounterId prepared_encounter = EncounterId::Count;
    EventId combat_metric_event = EventId::Count;
    int turn = 1;
    int discard_remaining = 0;
    int discard_action_num = 0;
    int exhaust_action_num = 0;
    PileId grid_source = PileId::Grid;
    int selection_amount = 0;
    int grid_num_cards = 0;
    bool selection_any_number = false;
    bool selection_can_pick_zero = false;
    int put_on_deck_num = 0;
    uint32_t next_power_instance = 1;
    uint32_t next_damage_info_instance = 1;
    uint32_t next_card_uuid = 1;
    uint16_t obtained_cards[static_cast<int>(CardId::Count)]{};
    bool selection_for_upgrade = false;
    bool grid_for_purge = false;
    bool grid_for_upgrade = false;
    bool grid_for_transform = false;
    bool grid_is_master_deck = false;
    bool grid_confirm_screen = false;
    int grid_hovered_card = -1;
    bool grid_can_cancel : 1;
    bool selection_confirmation : 1;
    bool selected_cards_retrieved : 1;
    bool end_turn_enabled : 1;
    bool selection_up_to : 1;
    bool reward_skippable : 1;
    bool combat_reward_skippable : 1;
    bool reward_proceed_visible : 1;
    bool reward_has_taken_all : 1;
    bool reward_relic_warning_seen : 1;
    bool skip_monster_turn : 1;
    bool battle_over : 1;
    bool battle_callbacks_done : 1;
    bool room_in_combat : 1;
    bool room_reward_time : 1;
    bool room_reward_allowed : 1;
    bool room_has_monsters : 1;
    RoomKind room_kind = RoomKind::Monster;
    RoomPhase room_phase = RoomPhase::Incomplete;
    bool room_elite_trigger = false;
    bool room_combat_event = false;
    int room_rare_card_chance = 3;
    int room_uncommon_card_chance = 37;
    uint16_t room_event_class = 0;
    bool cannot_lose = false;
    bool mugged = false;
    bool smoked = false;
    int bomb_id_offset = 0;
    uint32_t padding = 0;
    SPIRE_FN SPIRE_INLINE constexpr Game()
        : grid_can_cancel(true), selection_confirmation(false), selected_cards_retrieved(false), end_turn_enabled(true), selection_up_to(false),
          reward_skippable(false), combat_reward_skippable(false), reward_proceed_visible(false), reward_has_taken_all(false), reward_relic_warning_seen(false),
          skip_monster_turn(false), battle_over(false), battle_callbacks_done(false), room_in_combat(false), room_reward_time(false), room_reward_allowed(true), room_has_monsters(false) {}
};
static_assert(sizeof(Game) <= 37 * 1024, "Game exceeds the 37 KiB device memory budget; see docs/porting.md");

SPIRE_FN SPIRE_INLINE inline uint32_t next_power_identity(Game& g) {
    SPIRE_ASSERT(g.next_power_instance < (uint32_t{1} << 29));
    return g.next_power_instance++;
}

SPIRE_FN SPIRE_INLINE inline void bind_fault(Game& g) {
#if defined(__CUDA_ARCH__)
    device_fault_target() = &g.fault;
#else
    (void)g;
#endif
}

struct CombatSetup {
    EncounterId encounter = EncounterId::Cultist;
    int floor = 1;
    int act = 1;
    int8_t node_x = 0, node_y = 0;
    RoomKind room = RoomKind::Monster;
    bool first_room_chosen = false;
    bool keys[3]{};
    bool emerald = false;
    int hp = 70;
    int max_hp = 70;
    int gold = 99;
    List<Card, master_deck_capacity> deck{};
    List<Relic, 64> relics{};
    PotionId potions[potion_capacity]{};
};
}
