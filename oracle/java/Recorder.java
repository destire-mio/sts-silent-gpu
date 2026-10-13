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
import com.megacrit.cardcrawl.helpers.SeedHelper;
import com.megacrit.cardcrawl.helpers.TipTracker;
import com.megacrit.cardcrawl.map.MapRoomNode;
import com.megacrit.cardcrawl.potions.PotionSlot;
import com.megacrit.cardcrawl.relics.AbstractRelic;
import com.megacrit.cardcrawl.rooms.MonsterRoom;
import communicationmod.CommandExecutor;
import communicationmod.GameStateListener;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public final class Recorder {
    static final Gson JSON = new GsonBuilder().serializeNulls().disableHtmlEscaping().create();
    static long decisions;
    static ArrayList<String> initialRelicExclusions;

    static void tick() {
        if (!Platform.running) throw new AssertionError("Game requested exit");
        Runnable job;
        while ((job = Platform.jobs.poll()) != null) job.run();
        Platform.frame++;
        Platform.listener.render();
    }

    static void settle() {
        int ticks = 0;
        do {
            tick();
            if (++ticks > 36000) throw new AssertionError("No decision: " + AbstractDungeon.screen);
        } while (!GameStateListener.isWaitingForCommand() || !Platform.effectsSettled());
    }

    static boolean terminal(boolean scenario) {
        return AbstractDungeon.player.isDead || AbstractDungeon.screen == AbstractDungeon.CurrentScreen.DEATH
            || AbstractDungeon.screen == AbstractDungeon.CurrentScreen.VICTORY
            || (scenario && AbstractDungeon.getCurrRoom().isBattleOver);
    }

    static void scenario(JsonObject setup, long seed) {
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
            GameStateListener.registerCommandExecution();
            GameStateListener.registerStateChange();
            settle();
            while (AbstractDungeon.isScreenUp) {
                ArrayList<JsonObject> legal = Actions.legal();
                if (legal.isEmpty()) throw new AssertionError("Empty acquisition actions: " + AbstractDungeon.screen);
                Actions.apply(legal.get(acquisition.nextInt(legal.size())));
                settle();
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
        AbstractDungeon.monsterList.set(0, setup.get("encounter").getAsString());
        MapRoomNode node = new MapRoomNode(0, 0);
        node.room = new MonsterRoom();
        AbstractDungeon.nextRoom = node;
        CardCrawlGame.dungeon.nextRoomTransition();
        GameStateListener.registerStateChange();
        GameStateListener.registerCommandExecution();
    }

    static void record(JsonObject job) throws Exception {
        long seed = job.get("seed").getAsLong();
        boolean isScenario = !job.get("scenario").isJsonNull();
        Unseeded.rng = new Unseeded.Stream(seed);
        MathUtils.random = new RandomXS128(seed);
        Snapshot.resetStatics();
        AbstractDungeon.relicsToRemoveOnStart.clear();
        AbstractDungeon.relicsToRemoveOnStart.addAll(initialRelicExclusions);
        com.megacrit.cardcrawl.neow.NeowEvent.rng = null;
        com.megacrit.cardcrawl.actions.common.DrawCardAction.drawnCards.clear();
        for (String key : TipTracker.tips.keySet()) TipTracker.tips.put(key, true);
        CardCrawlGame.playerPref.putInteger("THE_SILENT_SPIRITS", 1);
        CommandExecutor.executeCommand("start silent 20 0" + SeedHelper.getString(seed));
        GameStateListener.registerCommandExecution();
        settle();
        if (isScenario) { scenario(job.getAsJsonObject("scenario"), seed); settle(); }
        RandomXS128 policy = new RandomXS128(seed);
        try (BufferedWriter out = Files.newBufferedWriter(Paths.get(job.get("output").getAsString()), StandardCharsets.UTF_8)) {
            JsonObject header = new JsonObject();
            header.addProperty("schema", 1);
            header.addProperty("game_sha256", "dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a");
            header.addProperty("seed", seed);
            header.addProperty("character", "THE_SILENT");
            header.addProperty("ascension", 20);
            header.add("scenario", job.get("scenario"));
            out.write(JSON.toJson(header)); out.newLine();
            int count = 0;
            while (!terminal(isScenario)) {
                ArrayList<JsonObject> legal = Actions.legal();
                if (legal.isEmpty()) throw new AssertionError("Empty actions: " + AbstractDungeon.screen);
                JsonObject action = legal.get(policy.nextInt(legal.size()));
                JsonObject line = new JsonObject();
                line.add("snapshot", Snapshot.capture());
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
            last.addProperty("terminal", AbstractDungeon.player.isDead ? "death" : isScenario ? "combat_end" : "victory");
            out.write(JSON.toJson(last)); out.newLine();
            System.out.println("ORACLE recorded seed=" + seed + " decisions=" + count);
        }
    }

    public static void run() throws Exception {
        Platform.initializePlatform();
        MathUtils.random = new RandomXS128(0L);
        Platform.listener = new CardCrawlGame(".prefs/");
        Platform.listener.create();
        com.megacrit.cardcrawl.core.Settings.UPLOAD_DATA = false;
        settle();
        initialRelicExclusions = new ArrayList<String>(AbstractDungeon.relicsToRemoveOnStart);
        JsonArray jobs;
        try (Reader reader = Files.newBufferedReader(Paths.get(System.getProperty("oracle.jobs")), StandardCharsets.UTF_8)) {
            jobs = new JsonParser().parse(reader).getAsJsonArray();
        }
        long start = System.nanoTime();
        for (int i = 0; i < jobs.size(); i++) {
            record(jobs.get(i).getAsJsonObject());
            if (i + 1 < jobs.size()) {
                CardCrawlGame.startOver();
                GameStateListener.registerCommandExecution();
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
        System.exit(0);
    }
}
