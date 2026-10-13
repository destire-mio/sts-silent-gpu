package oracle;

import com.google.gson.*;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.actions.GameActionManager;
import com.megacrit.cardcrawl.random.Random;
import com.megacrit.cardcrawl.ui.panels.EnergyPanel;
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
        fields("screens.select.GridCardSelectScreen", "selectedCards targetGroup hoveredCard upgradePreviewCard ritualAnimTimer numCards cardSelectAmount canCancel forUpgrade forTransform forPurge confirmScreenUp isJustForConfirming prevDeckSize cancelWasOn anyNumber forClarity");
        fields("screens.select.HandCardSelectScreen", "numCardsToSelect selectedCards upgradePreviewCard wereCardsRetrieved canPickZero upTo anyNumber forTransform forUpgrade numSelected waitThenClose waitToCloseTimer hand");
        fields("screens.CardRewardScreen", "rewardGroup discoveryCard hasTakenAll cardOnly rItem draft discovery chooseOne skippable draftCount codex codexCard");
        fields("screens.CombatRewardScreen", "rewards hasTakenAll mug smoke");
        fields("screens.select.BossRelicSelectScreen", "isDone relics blights");
        fields("shop.ShopScreen", "isActive coloredCards colorlessCards relics potions purgeAvailable");
        fields("rewards.RewardItem", "type gold bonusGold relic potion cards isDone ignoreReward relicLink redText");
    }

    static final Map<String, String> EXCLUDED = new TreeMap<String, String>();
    static {
        exclude("cards.CardGroup", "queued", "renderHand scratch list controlling draw order, cleared after rendering.");
        exclude("potions.AbstractPotion", "size isThrown", "Bottle artwork and drink/throw tooltip wording; use and target rules are separate.");
        exclude("characters.AbstractPlayer", "damagedThisCombat", "Vanilla increments and resets this unused counter but never reads it.");
        exclude("relics.AbstractRelic", "discarded", "Acquisition/disposal marker with no vanilla rule reads.");
        exclude("orbs.Frost", "hFlip1 hFlip2", "Ice-sprite horizontal orientation.");
        exclude("neow.NeowEvent", "npc", "Neow animation object; rewards and event stage are separate.");
        exclude("events.beyond.SpireHeart", "npc", "Heart animation object; event stage is separate.");
        exclude("actions.AbstractGameAction", "duration startDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.animations.ShoutAction", "bubbleDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.animations.TalkAction", "bubbleDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.animations.VFXAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.common.ApplyPoisonOnRandomMonsterAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.common.ApplyPowerAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.common.ExhaustSpecificCardAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.unique.ApplyStasisAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.unique.CalculatedGambleAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.unique.ExhaustAllNonAttackAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.unique.FiendFireAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.utility.ScryAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.watcher.ClarityAction", "startingDuration", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("events.AbstractEvent", "waitTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("events.beyond.SpireHeart", "eventFadeTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("events.city.TheJoust", "joustTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("events.shrines.GremlinMatchGame", "waitTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("events.shrines.GremlinWheelGame", "bounceTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("monsters.beyond.AwakenedOne", "fireTimer", "Schedules flame particles in update; no rule reads this clock.");
        exclude("monsters.beyond.Nemesis", "fireTimer", "Schedules flame particles in update; no rule reads this clock.");
        exclude("monsters.city.TheCollector", "fireTimer", "Schedules flame particles in update; no rule reads this clock.");
        exclude("monsters.city.TorchHead", "fireTimer", "Schedules flame particles in update; no rule reads this clock.");
        exclude("monsters.exordium.HexaghostOrb", "activateTimer particleTimer", "Schedules orb ignition artwork and particles.");
        exclude("rooms.AbstractRoom", "endBattleTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("rooms.TreasureRoom", "shinyTimer", "Schedules chest shine particles.");
        exclude("rooms.TreasureRoomBoss", "shinyTimer", "Schedules chest shine particles.");
        exclude("screens.select.GridCardSelectScreen", "ritualAnimTimer", "Schedules a disposable transform preview using presentation RNG; confirmation ignores the preview.");
        exclude("screens.select.HandCardSelectScreen", "waitToCloseTimer", "Update pacing; decisions wait for its callback or an input-blocked selection, not a remaining frame count.");
        exclude("actions.AbstractGameAction", "attackEffect", "Attack artwork and sound selection, not damage calculation.");
        exclude("actions.GameActionManager", "previousAction turnStartCurrentAction", "Historical action references are assigned or cleared but never read by vanilla rules.");
        exclude("actions.animations.ShoutAction", "msg used", "Speech presentation and its one-shot display flag.");
        exclude("actions.animations.TalkAction", "msg player used", "Speech presentation and its one-shot display flag.");
        exclude("actions.animations.VFXAction", "isTopLevelEffect", "Chooses a rendering effect list.");
        exclude("actions.common.DamageAction", "muteSfx", "Controls damage sound playback.");
        exclude("actions.common.DarkOrbEvokeAction", "muteSfx", "Controls damage sound playback.");
        exclude("actions.common.DamageAction", "skipWait", "Controls action pacing after damage.");
        exclude("actions.common.ApplyPowerToRandomEnemyAction", "effect isFast", "Attack artwork and effect pacing.");
        exclude("actions.common.AttackDamageRandomEnemyAction", "effect", "Attack artwork.");
        exclude("actions.common.MakeTempCardInDrawPileAction", "autoPosition x y", "Card flight coordinates; destination pile and insertion choice are separate fields.");
        exclude("actions.common.ReviveMonsterAction", "healingEffect", "Chooses healing artwork.");
        exclude("actions.common.SetMoveAction", "theNextName", "Localized intent label; move byte and damage are separate fields.");
        exclude("actions.unique.IncreaseMaxHpAction", "showEffect", "Controls the maximum-HP popup.");
        exclude("actions.utility.SFXAction", "adjust key pitchVar", "Sound identifier and pitch.");
        exclude("actions.utility.ShakeScreenAction", "intensity shakeDur startDur", "Screen-shake presentation.");
        exclude("cards.AbstractCard", "isUsed isSelected", "UI selection flags with no vanilla rule reads.");
        exclude("cards.AbstractCard", "isDamageModified isBlockModified isMagicNumberModified upgradedDamage upgradedBlock upgradedMagicNumber upgradedCost isCostModified isCostModifiedForTurn", "Card text color or upgrade display; numeric stats and upgrade count remain recorded.");
        exclude("characters.AbstractPlayer", "hoveredCard toHoverCard", "Transient input hover targets; each play action supplies its own card and monster.");
        exclude("events.AbstractImageEvent", "title", "Localized event heading.");
        exclude("ui.buttons.LargeDialogOptionButton", "msg", "Localized label; action identity is the original button slot.");
        exclude("ui.buttons.DialogOptionButton", "msg", "Localized label; action identity is the original button slot.");
        exclude("events.shrines.GremlinMatchGame", "hoveredCard", "Cursor target; pair-resolution callbacks finish before the next choice.");
        exclude("screens.select.GridCardSelectScreen", "upgradePreviewCard", "Display copy; confirmation uses hoveredCard, not this copy.");
        exclude("screens.select.HandCardSelectScreen", "upgradePreviewCard waitThenClose", "Display copy and auto-close pacing; auto-close finishes before a decision.");
        exclude("rewards.RewardItem", "redText", "Reward label tint.");
        exclude("monsters.exordium.Cultist", "saidPower talky", "Dialogue selection and death-animation pacing only (takeTurn/die).");
        exclude("monsters.beyond.AwakenedOne", "saidPower wParticles", "Dialogue flag and wing particles only.");
        exclude("monsters.exordium.AcidSlime_L", "saveY", "Vertical spawn art position; spawn ordering reads X only.");
        exclude("monsters.exordium.SpikeSlime_L", "saveY", "Vertical spawn art position; spawn ordering reads X only.");
        exclude("monsters.exordium.Hexaghost", "body orbs", "Body/orb artwork; attack-cycle index orbActiveCount is retained.");
        exclude("monsters.exordium.HexaghostBody", "m plasma1Angle plasma2Angle plasma3Angle rotationSpeed targetRotationSpeed", "Hexaghost artwork; these objects do not choose damage or moves.");
        exclude("monsters.exordium.HexaghostOrb", "activated hidden playedSfx x y", "Hexaghost artwork; these objects do not choose damage or moves.");
        exclude("monsters.exordium.LouseNormal", "isOpen", "Shell animation state; CurlUpPower owns the block rule.");
        exclude("monsters.exordium.LouseDefensive", "isOpen", "Shell animation state; CurlUpPower owns the block rule.");
        exclude("ui.campfire.AbstractCampfireOption", "label scale", "Campfire button label and draw scale; usable is retained.");
        exclude("shop.Merchant", "idleMessages modX modY saidWelcome", "Merchant speech and position; shop inventory is separate.");
        exclude("neow.NeowReward", "optionLabel", "Localized reward label; reward type is retained.");
        exclude("neow.NeowReward$NeowRewardDef", "desc", "Localized reward description.");
        exclude("neow.NeowReward$NeowRewardDrawbackDef", "desc", "Localized drawback description.");
        exclude("events.beyond.SpireHeart", "damageDealt globalDamageDealt totalDamageDealt winstreak x y", "End-screen statistics and placement, not future game decisions.");
        exclude("events.city.TheJoust", "clangCount", "Joust sound count; ownerWins is retained.");
        exclude("events.exordium.DeadAdventurer", "x y", "Corpse artwork position.");
        exclude("events.shrines.GremlinWheelGame", "bounceIn resultAngle tmpAngle wheelAngle spinVelocity", "Wheel motion; result is selected by miscRng and retained.");
    }
    static void exclude(String cls, String names, String reason) {
        for (String name : names.split(" ")) EXCLUDED.put(cls + "." + name, reason);
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
        GameActionManager.turn = 0;
        GameActionManager.playerHpLastTurn = 0;
        EnergyPanel.totalCount = 0;
        AbstractDungeon.nextRoom = null;
        AbstractDungeon.lastCombatMetricKey = null;
        AbstractDungeon.is_victory = false;
        AbstractDungeon.gridSelectScreen = new com.megacrit.cardcrawl.screens.select.GridCardSelectScreen();
        for (String[] spec : STATICS) for (String name : spec[1].split(" ")) {
            Field field = Class.forName("com.megacrit.cardcrawl." + spec[0]).getDeclaredField(name);
            field.setAccessible(true);
            if (field.getType() == boolean.class) field.setBoolean(null, false);
            else field.setInt(null, 0);
        }
        com.megacrit.cardcrawl.events.RoomEventDialog.selectedOption = -1;
        com.megacrit.cardcrawl.events.RoomEventDialog.waitForInput = true;
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
        screen.addProperty("choice", Choices.type());
        screen.add("options", Recorder.JSON.toJsonTree(Choices.names()));
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
        if (Modifier.isStatic(field.getModifiers())) return false;
        if (EXCLUDED.containsKey(field.getDeclaringClass().getName().replace("com.megacrit.cardcrawl.", "") + "." + field.getName())) return false;
        Set<String> allowed = FIELDS.get(field.getDeclaringClass().getName());
        if (allowed != null) return allowed.contains(field.getName());
        String name = field.getName().toLowerCase(Locale.ROOT);
        if (name.contains("vfx") || name.contains("sound") || name.contains("texture")
                || name.contains("img") || name.contains("hover") || name.contains("speech")
                || (name.equals("tips") || name.equals("tipmsg")) || name.contains("anim")
                || name.equals("description") || name.equals("name")) return false;
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
        if (!value.getClass().getName().startsWith("com.megacrit.cardcrawl.")) throw new AssertionError("Unsupported snapshot object " + value.getClass());
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
                    fields.put(cls.getName().substring(cls.getName().lastIndexOf('.') + 1) + "." + field.getName(), field);
                }
            JsonObject data = new JsonObject();
            object.add("fields", data);
            for (Map.Entry<String, Field> entry : fields.entrySet()) {
                entry.getValue().setAccessible(true);
                if (entry.getKey().equals("GridCardSelectScreen.hoveredCard") && !AbstractDungeon.gridSelectScreen.confirmScreenUp)
                    data.add(entry.getKey(), JsonNull.INSTANCE);
                else data.add(entry.getKey(), value(entry.getValue().get(value)));
            }
        }
        JsonObject ref = new JsonObject();
        ref.addProperty("ref", ids.get(value));
        return ref;
    }
}
