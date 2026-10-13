package oracle;

import com.google.gson.*;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.actions.GameActionManager;
import com.megacrit.cardcrawl.random.Random;
import com.megacrit.cardcrawl.ui.panels.EnergyPanel;
import communicationmod.ChoiceScreenUtils;
import java.lang.reflect.*;
import java.util.*;

final class Snapshot {
    static final Map<String, Set<String>> FIELDS = new TreeMap<String, Set<String>>();
    static {
        fields("cards.AbstractCard", "cardID cost costForTurn price type rarity color isCostModified isCostModifiedForTurn retain selfRetain dontTriggerOnUseCard isInnate isUsed upgraded timesUpgraded misc energyOnUse ignoreEnergyOnUse upgradedCost upgradedDamage upgradedBlock upgradedMagicNumber uuid isFlipped isSelected exhaust returnToHand shuffleBackIntoDrawPile isEthereal tags multiDamage isMultiDamage baseDamage baseBlock baseMagicNumber baseHeal baseDraw baseDiscard damage block magicNumber heal draw discard isDamageModified isBlockModified isMagicNumberModified damageType damageTypeForTurn target purgeOnUse exhaustOnUseOnce exhaustOnFire freeToPlayOnce isInAutoplay inBottleFlame inBottleLightning inBottleTornado");
        fields("cards.CardGroup", "group type queued");
        fields("core.AbstractCreature", "id powers isPlayer isBloodied drawX flipHorizontal gold isDying isDead halfDead isEscaping lastDamageTaken currentHealth maxHealth currentBlock");
        fields("characters.AbstractPlayer", "chosenClass masterDeck drawPile hand discardPile exhaustPile limbo relics potions potionSlots energy startingMaxHP masterHandSize masterMaxOrbs gameHandSize isEndingTurn endTurnQueued cardInUse hoveredCard toHoverCard damagedThisCombat cardsPlayedThisTurn maxOrbs orbs stance blights potionFills");
        fields("monsters.AbstractMonster", "escaped escapeNext type cannotEscape damage move moveHistory nextMove intent intentBaseDmg intentDmg intentMultiAmt isMultiDmg");
        fields("powers.AbstractPower", "ID owner amount priority type isTurnBased isPostActionPower canGoNegative");
        fields("relics.AbstractRelic", "relicId counter cost tier energyBased usedUp grayscale isObtained discarded");
        fields("potions.AbstractPotion", "ID potency slot canUse isThrown targetRequired rarity size");
        fields("rooms.AbstractRoom", "phase monsters event rewards rewardAllowed eliteTrigger mugged smoked battleOver isBattleOver cannotLose combatEvent endBattleTimer skipMonsterTurn rewardTime baseRareCardChance baseUncommonCardChance rareCardChance uncommonCardChance potions relics");
        fields("map.MapRoomNode", "x y room hasEmeraldKey taken parents edges");
        fields("map.MapEdge", "srcX srcY dstX dstY isBoss");
        fields("events.AbstractEvent", "screenNum roomEventText imageEventText waitTimer type noCardsInRewards hasFocus");
        fields("events.AbstractImageEvent", "title");
        fields("events.GenericEventDialog", "optionList selectedOption");
        fields("events.RoomEventDialog", "optionList selectedOption");
        fields("ui.buttons.LargeDialogOptionButton", "slot isDisabled msg");
        fields("ui.buttons.DialogOptionButton", "slot isDisabled msg");
        fields("stances.AbstractStance", "ID");
        fields("orbs.AbstractOrb", "ID evokeAmount passiveAmount baseEvokeAmount basePassiveAmount");
        fields("rooms.MonsterRoom", "");
        fields("characters.AnimatedNpc", "");
        fields("events.shrines.GremlinMatchGame", "chosenCard hoveredCard cardFlipped gameDone cleanUpCalled attemptCount cards waitTimer cardsMatched screen matchedCards");
        fields("rooms.CampfireUI", "somethingSelected buttons");
        fields("screens.select.GridCardSelectScreen", "selectedCards targetGroup upgradePreviewCard numCards cardSelectAmount canCancel forUpgrade forTransform forPurge confirmScreenUp isJustForConfirming prevDeckSize cancelWasOn anyNumber forClarity");
        fields("screens.select.HandCardSelectScreen", "numCardsToSelect selectedCards upgradePreviewCard wereCardsRetrieved canPickZero upTo anyNumber forTransform forUpgrade numSelected waitThenClose waitToCloseTimer hand");
        fields("screens.CardRewardScreen", "rewardGroup discoveryCard hasTakenAll cardOnly rItem draft discovery chooseOne skippable draftCount codex codexCard");
        fields("screens.CombatRewardScreen", "rewards hasTakenAll mug smoke");
        fields("screens.select.BossRelicSelectScreen", "isDone relics blights");
        fields("shop.ShopScreen", "isActive coloredCards colorlessCards relics potions purgeAvailable");
        fields("rewards.RewardItem", "type gold bonusGold relic potion cards isDone ignoreReward relicLink redText");
    }

    static void fields(String type, String names) {
        FIELDS.put("com.megacrit.cardcrawl." + type, new HashSet<String>(Arrays.asList(names.split(" "))));
    }

    static final String[][] STATICS = {
        {"actions.common.DiscardAction", "numDiscarded"},
        {"actions.common.ExhaustAction", "numExhausted"},
        {"actions.common.PutOnBottomOfDeckAction", "numPlaced"},
        {"actions.common.PutOnDeckAction", "numPlaced"},
        {"actions.unique.CodexAction", "numPlaced"},
        {"actions.unique.NightmareAction", "numDiscarded"},
        {"events.shrines.FaceTrader", "goldReward damage"},
        {"powers.TheBombPower", "bombIdOffset"},
        {"relics.CentennialPuzzle", "usedThisCombat"},
        {"relics.OrangePellets", "SKILL POWER ATTACK"}
    };

    static void resetStatics() throws Exception {
        for (String[] spec : STATICS) for (String name : spec[1].split(" ")) {
            Field field = Class.forName("com.megacrit.cardcrawl." + spec[0]).getDeclaredField(name);
            field.setAccessible(true);
            if (field.getType() == boolean.class) field.setBoolean(null, false);
            else field.setInt(null, 0);
        }
        com.megacrit.cardcrawl.events.GenericEventDialog.selectedOption = -1;
        com.megacrit.cardcrawl.events.GenericEventDialog.waitForInput = true;
        com.megacrit.cardcrawl.shop.ShopScreen.actualPurgeCost = 75;
        com.megacrit.cardcrawl.actions.defect.ScrapeAction.scrapedCards.clear();
    }

    static final class Schema {
    static final JsonObject DATA = new JsonParser().parse(new java.io.InputStreamReader(
        Snapshot.class.getResourceAsStream("/snapshot-fields.json"), java.nio.charset.StandardCharsets.UTF_8)).getAsJsonObject();
    }
    final IdentityHashMap<Object, Integer> ids = new IdentityHashMap<Object, Integer>();
    final Map<UUID, Integer> uuids = new HashMap<UUID, Integer>();
    final JsonArray objects = new JsonArray();

    static JsonObject capture() throws Exception {
        Snapshot s = new Snapshot();
        JsonObject result = new JsonObject();
        result.add("run", s.pick(null, AbstractDungeon.class,
            "id floorNum actNum ascensionLevel isAscensionMode isDungeonBeaten is_victory ascensionCheck bossKey bossCount firstRoomChosen cardBlizzRandomizer cardBlizzStartOffset cardBlizzGrowth cardBlizzMaxOffset shrineChance cardUpgradedChance colorlessRareChance shopRoomChance restRoomChance eventRoomChance eliteRoomChance treasureRoomChance smallChestChance mediumChestChance largeChestChance commonRelicChance uncommonRelicChance rareRelicChance pathX pathY monsterList eliteMonsterList bossList eventList shrineList specialOneTimeEventList commonRelicPool uncommonRelicPool rareRelicPool shopRelicPool bossRelicPool relicsToRemoveOnStart lastCombatMetricKey"));
        result.getAsJsonObject("run").addProperty("seed", Settings.seed);
        result.getAsJsonObject("run").add("keys", Recorder.JSON.toJsonTree(new boolean[]{Settings.hasRubyKey, Settings.hasEmeraldKey, Settings.hasSapphireKey}));
        result.add("player", s.value(AbstractDungeon.player));
        result.add("room", s.value(AbstractDungeon.getCurrRoom()));
        result.add("map", s.map());
        result.add("node", s.position(AbstractDungeon.currMapNode));
        result.add("next_node", s.position(AbstractDungeon.nextRoom));
        result.add("manager", s.value(AbstractDungeon.actionManager));
        result.add("combat", s.pick(null, GameActionManager.class,
            "totalDiscardedThisTurn damageReceivedThisTurn damageReceivedThisCombat hpLossThisCombat playerHpLastTurn energyGainedThisCombat turn"));
        result.getAsJsonObject("combat").addProperty("energy", EnergyPanel.totalCount);
        JsonObject screen = new JsonObject();
        screen.addProperty("type", AbstractDungeon.screen.name());
        screen.addProperty("up", AbstractDungeon.isScreenUp);
        screen.addProperty("previous", AbstractDungeon.previousScreen == null ? null : AbstractDungeon.previousScreen.name());
        screen.addProperty("choice", ChoiceScreenUtils.getCurrentChoiceType().name());
        screen.add("options", Recorder.JSON.toJsonTree(ChoiceScreenUtils.getCurrentChoiceList()));
        switch (AbstractDungeon.screen) {
            case GRID: screen.add("state", s.value(AbstractDungeon.gridSelectScreen)); break;
            case HAND_SELECT: screen.add("state", s.value(AbstractDungeon.handCardSelectScreen)); break;
            case CARD_REWARD: screen.add("state", s.value(AbstractDungeon.cardRewardScreen)); break;
            case COMBAT_REWARD: screen.add("state", s.value(AbstractDungeon.combatRewardScreen)); break;
            case BOSS_REWARD: screen.add("state", s.value(AbstractDungeon.bossRelicScreen)); break;
            case SHOP: screen.add("state", s.value(AbstractDungeon.shopScreen)); break;
            default: screen.add("state", JsonNull.INSTANCE);
        }
        screen.add("room_dialog", s.pick(null, com.megacrit.cardcrawl.events.RoomEventDialog.class, "optionList selectedOption waitForInput"));
        screen.add("image_dialog", s.pick(null, com.megacrit.cardcrawl.events.GenericEventDialog.class, "selectedOption waitForInput"));
        result.add("screen", screen);
        JsonObject globals = new JsonObject();
        globals.add("event_chances", s.pick(null, com.megacrit.cardcrawl.helpers.EventHelper.class, "ELITE_CHANCE MONSTER_CHANCE SHOP_CHANCE TREASURE_CHANCE"));
        globals.add("potion", s.pick(null, com.megacrit.cardcrawl.rooms.AbstractRoom.class, "blizzardPotionMod"));
        globals.add("shop", s.pick(null, com.megacrit.cardcrawl.shop.ShopScreen.class, "purgeCost actualPurgeCost"));
        globals.add("drawn_cards", s.value(com.megacrit.cardcrawl.actions.common.DrawCardAction.drawnCards));
        globals.add("scraped_cards", s.value(com.megacrit.cardcrawl.actions.defect.ScrapeAction.scrapedCards));
        globals.add("obtained_cards", s.value(com.megacrit.cardcrawl.helpers.CardHelper.obtainedCards));
        for (String[] spec : STATICS) globals.add(spec[0], s.pick(null, Class.forName("com.megacrit.cardcrawl." + spec[0]), spec[1]));
        result.add("globals", globals);
        JsonObject pools = new JsonObject();
        for (String name : new String[]{"colorlessCardPool", "curseCardPool", "commonCardPool", "uncommonCardPool", "rareCardPool",
                "srcColorlessCardPool", "srcCurseCardPool", "srcCommonCardPool", "srcUncommonCardPool", "srcRareCardPool"}) {
            com.megacrit.cardcrawl.cards.CardGroup group = (com.megacrit.cardcrawl.cards.CardGroup)AbstractDungeon.class.getField(name).get(null);
            JsonArray cards = new JsonArray();
            for (com.megacrit.cardcrawl.cards.AbstractCard card : group.group) {
                JsonArray spec = new JsonArray();
                spec.add(new JsonPrimitive(card.cardID)); spec.add(new JsonPrimitive(card.timesUpgraded)); spec.add(new JsonPrimitive(card.misc));
                cards.add(spec);
            }
            pools.add(name, cards);
        }
        result.add("pools", pools);
        JsonObject rng = new JsonObject();
        for (Field field : AbstractDungeon.class.getDeclaredFields()) {
            if (field.getType() == Random.class) rng.add(field.getName(), s.rng((Random)field.get(null)));
        }
        rng.add("neow", s.rng(com.megacrit.cardcrawl.neow.NeowEvent.rng));
        rng.add("unseeded", Recorder.JSON.toJsonTree(new long[]{Unseeded.rng.counter, Unseeded.rng.getState(0), Unseeded.rng.getState(1)}));
        result.add("rng", rng);
        result.add("objects", s.objects);
        return result;
    }

    JsonElement position(com.megacrit.cardcrawl.map.MapRoomNode node) {
        return node == null ? JsonNull.INSTANCE : Recorder.JSON.toJsonTree(new int[]{node.x, node.y});
    }

    JsonArray map() {
        JsonArray rows = new JsonArray();
        for (ArrayList<com.megacrit.cardcrawl.map.MapRoomNode> row : AbstractDungeon.map) {
            JsonArray nodes = new JsonArray();
            for (com.megacrit.cardcrawl.map.MapRoomNode node : row) {
                JsonObject item = new JsonObject();
                item.add("position", position(node));
                item.addProperty("room", node.room == null ? null : node.room.getClass().getSimpleName());
                item.addProperty("emerald", node.hasEmeraldKey);
                item.addProperty("taken", node.taken);
                JsonArray edges = new JsonArray();
                for (com.megacrit.cardcrawl.map.MapEdge edge : node.getEdges())
                    edges.add(Recorder.JSON.toJsonTree(new int[]{edge.dstX, edge.dstY}));
                item.add("edges", edges);
                nodes.add(item);
            }
            rows.add(nodes);
        }
        return rows;
    }

    JsonElement rng(Random rng) {
        return rng == null ? JsonNull.INSTANCE : Recorder.JSON.toJsonTree(new long[]{rng.counter, rng.random.getState(0), rng.random.getState(1)});
    }

    JsonObject pick(Object object, Class<?> cls, String names) throws Exception {
        JsonObject out = new JsonObject();
        for (String name : names.split(" ")) {
            Field field = cls.getDeclaredField(name);
            field.setAccessible(true);
            out.add(name, value(field.get(object)));
        }
        return out;
    }

    static boolean included(Field field) {
        if (Modifier.isStatic(field.getModifiers()) || field.isSynthetic()) return false;
        Set<String> allowed = FIELDS.get(field.getDeclaringClass().getName());
        if (allowed != null) return allowed.contains(field.getName());
        String name = field.getName().toLowerCase(Locale.ROOT);
        if (name.contains("vfx") || name.contains("sound") || name.contains("texture")
                || name.contains("img") || name.contains("hover") || name.contains("speech")
                || (name.equals("tips") || name.equals("tipmsg")) || name.contains("anim") || name.equals("description") || name.equals("name")
) return false;
        Class<?> type = field.getType();
        String typeName = type.getName();
        return type.isPrimitive() || type.isEnum() || type == String.class || type == UUID.class || type.isArray()
            || Collection.class.isAssignableFrom(type) || Map.class.isAssignableFrom(type)
            || typeName.startsWith("com.megacrit.cardcrawl.") && !typeName.startsWith("com.megacrit.cardcrawl.vfx.")
                && !typeName.contains("Hitbox") && !typeName.contains("DialogWord")
                && !typeName.contains("EnergyOrb") && !typeName.contains("CharStat")
                && !typeName.contains("Prefs") && !typeName.contains("EnergyPanel");
    }

    JsonElement value(Object value) throws Exception {
        if (value == null) return JsonNull.INSTANCE;
        if (value instanceof Number) return new JsonPrimitive((Number)value);
        if (value instanceof Boolean) return new JsonPrimitive((Boolean)value);
        if (value instanceof Character || value instanceof String || value instanceof Enum<?>) return new JsonPrimitive(value.toString());
        if (value instanceof UUID) {
            UUID uuid = (UUID)value;
            if (!uuids.containsKey(uuid)) uuids.put(uuid, uuids.size());
            return new JsonPrimitive(uuids.get(uuid));
        }
        if (value instanceof Random) return rng((Random)value);
        if (value.getClass().isArray()) {
            JsonArray array = new JsonArray();
            for (int i = 0; i < Array.getLength(value); i++) array.add(value(Array.get(value, i)));
            return array;
        }
        if (value instanceof Collection<?>) {
            JsonArray array = new JsonArray();
            for (Object item : (Collection<?>)value) array.add(value(item));
            return array;
        }
        if (value instanceof Map<?, ?>) {
            TreeMap<String, Object> sorted = new TreeMap<String, Object>();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>)value).entrySet()) sorted.put(entry.getKey().toString(), entry.getValue());
            JsonObject map = new JsonObject();
            for (Map.Entry<String, Object> entry : sorted.entrySet()) map.add(entry.getKey(), value(entry.getValue()));
            return map;
        }
        if (!value.getClass().getName().startsWith("com.megacrit.cardcrawl.")
                && value.getClass() != communicationmod.EndOfTurnAction.class) throw new AssertionError("Unsupported snapshot object " + value.getClass());
        if (!ids.containsKey(value)) {
            int id = objects.size();
            ids.put(value, id);
            JsonObject object = new JsonObject();
            objects.add(object);
            object.addProperty("class", value.getClass().getName().replace("com.megacrit.cardcrawl.", ""));
            TreeMap<String, Field> fields = new TreeMap<String, Field>();
            for (Class<?> cls = value.getClass(); cls != Object.class; cls = cls.getSuperclass())
                for (Map.Entry<String, JsonElement> entry : Schema.DATA.getAsJsonObject(cls.getName().replace("com.megacrit.cardcrawl.", "")).getAsJsonObject("fields").entrySet()) {
                    Field field = cls.getDeclaredField(entry.getKey());
                    fields.put(cls.getSimpleName() + "." + field.getName(), field);
                }
            JsonObject data = new JsonObject();
            object.add("fields", data);
            for (Map.Entry<String, Field> entry : fields.entrySet()) {
                entry.getValue().setAccessible(true);
                data.add(entry.getKey(), value(entry.getValue().get(value)));
            }
        }
        JsonObject ref = new JsonObject();
        ref.addProperty("ref", ids.get(value));
        return ref;
    }
}
