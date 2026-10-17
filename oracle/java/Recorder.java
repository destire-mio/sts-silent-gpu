package oracle;

import com.badlogic.gdx.math.MathUtils;
import com.badlogic.gdx.math.RandomXS128;
import com.google.gson.*;
import com.megacrit.cardcrawl.cards.AbstractCard;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.helpers.CardLibrary;
import com.megacrit.cardcrawl.helpers.PotionHelper;
import com.megacrit.cardcrawl.helpers.RelicLibrary;
import com.megacrit.cardcrawl.helpers.TipTracker;
import com.megacrit.cardcrawl.map.MapRoomNode;
import com.megacrit.cardcrawl.potions.PotionSlot;
import com.megacrit.cardcrawl.relics.AbstractRelic;
import com.megacrit.cardcrawl.rooms.*;
import com.megacrit.cardcrawl.dungeons.TheCity;
import com.megacrit.cardcrawl.dungeons.TheBeyond;
import com.megacrit.cardcrawl.dungeons.TheEnding;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.characters.AbstractPlayer;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.zip.GZIPOutputStream;

public final class Recorder {
    static final Gson JSON = new GsonBuilder().serializeNulls().disableHtmlEscaping().create();
    static long decisions;
    static ArrayList<String> initialRelicExclusions;
    static Map<String, String> initialBossSeen;
    static boolean playtimeAtLeast800;

    static void tick() {
        if (!Platform.running) throw new AssertionError("Game requested exit");
        Runnable job;
        while ((job = Platform.jobs.poll()) != null) job.run();
        Platform.frame++;
        CardCrawlGame.playtime = playtimeAtLeast800 ? 800.0f : 799.0f;
        Platform.listener.render();
    }

    static void settle() throws Exception { settle(false); }

    static void settle(boolean acquisition) throws Exception {
        int ticks = 0;
        do {
            tick();
            if (AbstractDungeon.screen == AbstractDungeon.CurrentScreen.FTUE) {
                com.megacrit.cardcrawl.ui.buttons.GotItButton button = Choices.get(AbstractDungeon.ftue, "button");
                button.hb.clicked = true;
                AbstractDungeon.ftue.update();
            }
            if (++ticks > 36000) throw new AssertionError("No decision: " + AbstractDungeon.screen + " settled=" + Platform.effectsSettled()
                + " phase=" + AbstractDungeon.getCurrRoom().phase + " choices=" + Choices.names() + " confirm=" + Choices.confirm()
                + " actions=" + AbstractDungeon.actionManager.actions + " current=" + AbstractDungeon.actionManager.currentAction);
        } while (!Platform.effectsSettled() || !ready(acquisition));
    }

    static boolean ready(boolean acquisition) throws Exception {
        if (CardCrawlGame.mode != CardCrawlGame.GameMode.GAMEPLAY) return CardCrawlGame.mode == CardCrawlGame.GameMode.CHAR_SELECT && !CardCrawlGame.mainMenuScreen.isFadingOut;
        if (AbstractDungeon.isFadingOut || AbstractDungeon.isFadingIn || AbstractDungeon.player == null) return false;
        if (terminal(false)) return true;
        // TrueVictoryRoom.update advances Cutscene until it opens the victory screen.
        if (AbstractDungeon.getCurrRoom() instanceof TrueVictoryRoom) return false;
        // AbstractRoom.update opens rewards after endBattle changes isBattleOver.
        if (AbstractDungeon.getCurrRoom().isBattleOver && AbstractDungeon.getCurrRoom().phase == AbstractRoom.RoomPhase.COMBAT) return false;
        // ShopScreen.updatePurge consumes the GRID result on the next SHOP update.
        if ((!AbstractDungeon.isScreenUp || AbstractDungeon.screen == AbstractDungeon.CurrentScreen.SHOP)
                && !AbstractDungeon.gridSelectScreen.selectedCards.isEmpty()) return false;
        if (AbstractDungeon.screen == AbstractDungeon.CurrentScreen.HAND_SELECT
                && Choices.<Boolean>get(AbstractDungeon.handCardSelectScreen, "waitThenClose")) return false;
        if (AbstractDungeon.screen == AbstractDungeon.CurrentScreen.MAP)
            for (ArrayList<MapRoomNode> row : AbstractDungeon.map) for (MapRoomNode node : row)
                if (Choices.<Float>get(node, "animWaitTimer") != 0.0f) return false;
        // AbstractImageEvent.update restores focus on its next EventRoom.update callback.
        if (!AbstractDungeon.isScreenUp && AbstractDungeon.getCurrRoom().event instanceof com.megacrit.cardcrawl.events.AbstractImageEvent
                && !Choices.<Boolean>get(AbstractDungeon.getCurrRoom().event, "combatTime")
                && !AbstractDungeon.getCurrRoom().event.hasFocus) return false;
        if (acquisition && !AbstractDungeon.isScreenUp) return true;
        String choice = Choices.type();
        if (choice.equals("EVENT")) {
            if (Choices.options().isEmpty()) return false;
            if (Choices.imageDialog() && (Choices.<Float>get(AbstractDungeon.getCurrRoom().event.imageEventText, "animateTimer") != 0.0f
                    || !com.megacrit.cardcrawl.events.GenericEventDialog.waitForInput)) return false;
            if (!com.megacrit.cardcrawl.events.RoomEventDialog.waitForInput) return false;
            for (Object option : Choices.options())
                if (option instanceof com.megacrit.cardcrawl.ui.buttons.LargeDialogOptionButton
                        && ((com.megacrit.cardcrawl.ui.buttons.LargeDialogOptionButton)option).pressed) return false;
        }
        if ((choice.equals("SHOP_ROOM") || choice.equals("CHEST") || choice.equals("COMPLETE")
                || choice.equals("REST") && AbstractDungeon.getCurrRoom().phase == com.megacrit.cardcrawl.rooms.AbstractRoom.RoomPhase.COMPLETE)
                && !Choices.confirm()) return false;
        return !Actions.legal().isEmpty();
    }

    static boolean terminal(boolean scenario) {
        return AbstractDungeon.player.isDead || AbstractDungeon.screen == AbstractDungeon.CurrentScreen.DEATH
            || AbstractDungeon.screen == AbstractDungeon.CurrentScreen.VICTORY
            || (scenario && AbstractDungeon.getCurrRoom().isBattleOver);
    }

    static void scenario(JsonObject setup, long seed) throws Exception {
        JsonObject start = setup.has("start") ? setup.getAsJsonObject("start") : null;
        if (start != null) {
            // AbstractDungeon.setCurrMapNode disposes the departing room; direct run starts skip that transition.
            AbstractDungeon.getCurrRoom().dispose();
            int act = start.get("act").getAsInt();
            AbstractDungeon.floorNum = start.get("floor").getAsInt() - 1;
            JsonArray keys = start.getAsJsonArray("keys");
            Settings.hasRubyKey = keys.get(0).getAsBoolean();
            Settings.hasEmeraldKey = keys.get(1).getAsBoolean();
            Settings.hasSapphireKey = keys.get(2).getAsBoolean();
            ArrayList<String> events = new ArrayList<String>(AbstractDungeon.specialOneTimeEventList);
            AbstractDungeon.actNum = act - 1;
            switch (act) {
                case 1: AbstractDungeon.actNum = 1; break;
                case 2: CardCrawlGame.dungeon = new TheCity(AbstractDungeon.player, events); break;
                case 3: CardCrawlGame.dungeon = new TheBeyond(AbstractDungeon.player, events); break;
                case 4: CardCrawlGame.dungeon = new TheEnding(AbstractDungeon.player, events); break;
                default: throw new AssertionError("Invalid start act: " + act);
            }
            AbstractDungeon.currMapNode = new MapRoomNode(0, -1);
            AbstractDungeon.currMapNode.room = new EmptyRoom();
            AbstractDungeon.closeCurrentScreen();
            AbstractDungeon.player.gold = start.get("gold").getAsInt();
        }
        AbstractDungeon.player.masterDeck.clear();
        for (JsonElement entry : setup.getAsJsonArray("deck")) {
            JsonObject item = entry.getAsJsonObject();
            AbstractCard card = CardLibrary.getCard(item.get("id").getAsString()).makeCopy();
            for (int i = 0; i < item.get("upgrades").getAsInt(); i++) card.upgrade();
            AbstractDungeon.player.masterDeck.addToTop(card);
        }
        for (AbstractRelic relic : AbstractDungeon.player.relics) relic.onUnequip();
        AbstractDungeon.player.relics.clear();
        RandomXS128 acquisition = new RandomXS128(seed);
        for (JsonElement item : setup.getAsJsonArray("relics")) {
            RelicLibrary.getRelic(item.getAsString()).makeCopy().instantObtain(AbstractDungeon.player, AbstractDungeon.player.relics.size(), true);
            settle(true);
            while (AbstractDungeon.isScreenUp && AbstractDungeon.screen != AbstractDungeon.CurrentScreen.MAP) {
                ArrayList<JsonObject> legal = Actions.legal();
                if (legal.isEmpty()) throw new AssertionError("Empty acquisition actions: " + AbstractDungeon.screen);
                Actions.apply(legal.get(acquisition.nextInt(legal.size())));
                settle(true);
            }
        }
        AbstractDungeon.player.potions.clear();
        for (int i = 0; i < AbstractDungeon.player.potionSlots; i++) AbstractDungeon.player.potions.add(new PotionSlot(i));
        for (int i = 0; i < setup.getAsJsonArray("potions").size(); i++) {
            JsonElement item = setup.getAsJsonArray("potions").get(i);
            if (!item.isJsonNull()) AbstractDungeon.player.obtainPotion(i, PotionHelper.getPotion(item.getAsString()));
        }
        AbstractDungeon.player.maxHealth = setup.get("max_hp").getAsInt();
        AbstractDungeon.player.currentHealth = setup.get("hp").getAsInt();
        if (start != null && start.get("room").getAsString().equals("NeowRoom")) {
            AbstractDungeon.floorNum = start.get("floor").getAsInt();
            AbstractDungeon.currMapNode = new MapRoomNode(0, -1);
            AbstractDungeon.currMapNode.room = new com.megacrit.cardcrawl.neow.NeowRoom(false);
            AbstractDungeon.firstRoomChosen = false;
            return;
        }
        MapRoomNode node;
        if (start == null) {
            node = new MapRoomNode(0, 0);
            node.room = new MonsterRoom();
        } else {
            JsonArray position = start.getAsJsonArray("node");
            node = AbstractDungeon.map.get(position.get(1).getAsInt()).get(position.get(0).getAsInt());
            switch (start.get("room").getAsString()) {
                case "MonsterRoom": node.room = new MonsterRoom(); break;
                case "MonsterRoomElite": node.room = new MonsterRoomElite(); break;
                case "MonsterRoomBoss": node.room = new MonsterRoomBoss(); break;
                case "RestRoom": node.room = new RestRoom(); break;
                case "ShopRoom": node.room = new ShopRoom(); break;
                case "TreasureRoom": node.room = new TreasureRoom(); break;
                case "TreasureRoomBoss": node.room = new TreasureRoomBoss(); break;
                case "EventRoom": node.room = new EventRoom(); break;
                default: throw new AssertionError("Invalid start room: " + start.get("room"));
            }
            node.hasEmeraldKey = start.get("emerald").getAsBoolean();
            AbstractDungeon.firstRoomChosen = true;
            AbstractDungeon.pathX.add(node.x);
            AbstractDungeon.pathY.add(node.y);
        }
        if (node.room instanceof MonsterRoomElite) AbstractDungeon.eliteMonsterList.set(0, setup.get("encounter").getAsString());
        else if (node.room instanceof MonsterRoomBoss) AbstractDungeon.bossKey = setup.get("encounter").getAsString();
        else if (node.room instanceof MonsterRoom) AbstractDungeon.monsterList.set(0, setup.get("encounter").getAsString());
        AbstractDungeon.nextRoom = node;
        CardCrawlGame.dungeon.nextRoomTransition();
    }

    static void record(JsonObject job) throws Exception {
        long seed = job.get("seed").getAsLong();
        boolean isScenario = !job.get("scenario").isJsonNull();
        boolean setupScenario = isScenario && job.getAsJsonObject("scenario").has("encounter");
        boolean combatScenario = setupScenario && !job.getAsJsonObject("scenario").has("start");
        playtimeAtLeast800 = isScenario && job.getAsJsonObject("scenario").has("playtime_at_least_800")
            && job.getAsJsonObject("scenario").get("playtime_at_least_800").getAsBoolean();
        JsonArray inputs = isScenario && job.getAsJsonObject("scenario").has("actions")
            ? job.getAsJsonObject("scenario").getAsJsonArray("actions") : new JsonArray();
        Unseeded.rng = new Unseeded.Stream(seed);
        MathUtils.random = new RandomXS128(seed);
        CardCrawlGame.tips = new com.megacrit.cardcrawl.helpers.GameTips();
        Snapshot.resetStatics();
        com.megacrit.cardcrawl.unlock.UnlockTracker.bossSeenPref.data.clear();
        com.megacrit.cardcrawl.unlock.UnlockTracker.bossSeenPref.data.putAll(initialBossSeen);
        AbstractDungeon.relicsToRemoveOnStart.clear();
        AbstractDungeon.relicsToRemoveOnStart.addAll(initialRelicExclusions);
        com.megacrit.cardcrawl.neow.NeowEvent.rng = null;
        com.megacrit.cardcrawl.actions.common.DrawCardAction.drawnCards.clear();
        for (String key : TipTracker.tips.keySet()) TipTracker.tips.put(key, true);
        JsonObject profile = isScenario && job.getAsJsonObject("scenario").has("profile") ? job.getAsJsonObject("scenario").getAsJsonObject("profile") : null;
        if (profile != null) for (JsonElement boss : profile.getAsJsonArray("bosses_seen"))
            com.megacrit.cardcrawl.unlock.UnlockTracker.bossSeenPref.putInteger(boss.getAsString(), 1);
        CardCrawlGame.playerPref.putInteger("THE_SILENT_SPIRITS", profile == null ? 1 : profile.get("neow_spirits").getAsInt());
        Settings.seed = seed;
        Settings.seedSet = profile == null || profile.get("seeded").getAsBoolean();
        AbstractDungeon.generateSeeds();
        AbstractDungeon.ascensionLevel = 20;
        AbstractDungeon.isAscensionMode = true;
        CardCrawlGame.chosenCharacter = AbstractPlayer.PlayerClass.THE_SILENT;
        CardCrawlGame.characterManager.setChosenCharacter(CardCrawlGame.chosenCharacter);
        CardCrawlGame.startOver = true;
        CardCrawlGame.mainMenuScreen.isFadingOut = true;
        CardCrawlGame.mainMenuScreen.fadeOutMusic();
        while (CardCrawlGame.mode != CardCrawlGame.GameMode.GAMEPLAY) tick();
        settle();
        if (setupScenario) { scenario(job.getAsJsonObject("scenario"), seed); settle(); }
        RandomXS128 policy = new RandomXS128(seed);
        Path output = Paths.get(job.get("output").getAsString());
        OutputStream bytes = Files.newOutputStream(output);
        if (output.toString().endsWith(".gz")) bytes = new GZIPOutputStream(bytes, 65536);
        try (BufferedWriter out = new BufferedWriter(new OutputStreamWriter(bytes, StandardCharsets.UTF_8))) {
            JsonObject header = new JsonObject();
            header.addProperty("schema", 2);
            header.addProperty("game_sha256", "dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a");
            header.addProperty("seed", seed);
            header.addProperty("character", "THE_SILENT");
            header.addProperty("ascension", 20);
            header.addProperty("playtime_at_least_800", playtimeAtLeast800);
            header.add("scenario", job.get("scenario"));
            out.write(JSON.toJson(header)); out.newLine();
            int count = 0;
            while (!terminal(combatScenario)) {
                ArrayList<JsonObject> legal = Actions.legal();
                if (legal.isEmpty()) throw new AssertionError("Empty actions: " + AbstractDungeon.screen);
                JsonObject action = legal.get(policy.nextInt(legal.size()));
                JsonObject line = new JsonObject();
                line.add("snapshot", Snapshot.capture());
                if (count < inputs.size()) {
                    action = inputs.get(count).getAsJsonObject();
                    if (!legal.contains(action)) throw new AssertionError("Illegal scenario input: " + action + " legal=" + legal + " snapshot=" + line.get("snapshot"));
                }
                line.add("legal", JSON.toJsonTree(legal));
                line.add("action", action);
                out.write(JSON.toJson(line)); out.newLine();
                Actions.apply(action);
                settle();
                decisions++;
                if (++count > 100000) throw new AssertionError("Decision limit");
            }
            JsonObject last = new JsonObject();
            last.add("snapshot", Snapshot.capture());
            last.addProperty("terminal", AbstractDungeon.player.isDead ? "death" : combatScenario ? "combat_end" : "victory");
            out.write(JSON.toJson(last)); out.newLine();
            System.out.println("ORACLE recorded seed=" + seed + " decisions=" + count);
        }
    }

    public static void run() throws Exception {
        try (java.util.jar.JarFile jar = new java.util.jar.JarFile("desktop-1.0.jar")) {
            Enumeration<java.util.jar.JarEntry> entries = jar.entries();
            while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName();
                if (name.startsWith("com/megacrit/cardcrawl/") && name.endsWith(".class"))
                    Class.forName(name.substring(0, name.length() - 6).replace('/', '.'), false, Recorder.class.getClassLoader());
            }
        }
        Platform.initializePlatform();
        MathUtils.random = new RandomXS128(0L);
        Platform.listener = new CardCrawlGame(".prefs/");
        Platform.listener.create();
        com.megacrit.cardcrawl.core.Settings.UPLOAD_DATA = false;
        settle();
        initialRelicExclusions = new ArrayList<String>(AbstractDungeon.relicsToRemoveOnStart);
        initialBossSeen = new HashMap<String, String>(com.megacrit.cardcrawl.unlock.UnlockTracker.bossSeenPref.data);
        JsonArray jobs;
        try (Reader reader = Files.newBufferedReader(Paths.get(System.getProperty("oracle.jobs")), StandardCharsets.UTF_8)) {
            jobs = new JsonParser().parse(reader).getAsJsonArray();
        }
        long start = System.nanoTime();
        for (int i = 0; i < jobs.size(); i++) {
            record(jobs.get(i).getAsJsonObject());
            if (i + 1 < jobs.size()) {
                CardCrawlGame.startOver();
                while (CardCrawlGame.mode != CardCrawlGame.GameMode.CHAR_SELECT) tick();
                settle();
            }
        }
        JsonObject result = new JsonObject();
        result.addProperty("games", jobs.size());
        result.addProperty("decisions", decisions);
        double seconds = (System.nanoTime() - start) / 1.0e9;
        result.addProperty("seconds", seconds);
        result.addProperty("decisions_per_second", decisions / seconds);
        Files.write(Paths.get("result.json"), JSON.toJson(result).getBytes(StandardCharsets.UTF_8));
        Class.forName("oracle.audit.BytecodeAudit", true, ClassLoader.getSystemClassLoader()).getMethod("write").invoke(null);
        System.exit(0);
    }
}
