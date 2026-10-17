package oracle;

import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.backends.lwjgl.LwjglNativesLoader;
import com.badlogic.gdx.graphics.Color;
import com.badlogic.gdx.graphics.GL20;
import com.badlogic.gdx.graphics.g2d.SpriteBatch;
import com.google.gson.*;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.helpers.FontHelper;
import com.megacrit.cardcrawl.helpers.ImageMaster;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.ArrayList;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.PixelFormat;

public final class Watch {
    static boolean headless, policy;
    static long delayMillis;

    public static void openDisplay() throws Exception {
        if (headless) return;
        LwjglNativesLoader.load();
        Display.setDisplayMode(new DisplayMode(1280, 720));
        Display.setTitle("Silent policy - replaying prefix");
        Display.setResizable(false);
        Display.create(new PixelFormat(8, 16, 0));
        java.lang.reflect.Constructor<?> gl = Class.forName("com.badlogic.gdx.backends.lwjgl.LwjglGL20").getDeclaredConstructor();
        gl.setAccessible(true);
        Gdx.gl20 = (GL20)gl.newInstance();
        Gdx.gl = Gdx.gl20;
    }

    public static void present() {
        if (headless) return;
        if (Display.isCloseRequested()) System.exit(130);
        Display.update();
        if (policy) Display.sync(60);
    }

    static void pause(long millis) throws Exception {
        long end = System.nanoTime() + millis * 1000000L;
        while (System.nanoTime() < end) {
            if (!headless) {
                if (Display.isCloseRequested()) System.exit(130);
                Display.processMessages();
            }
            Thread.sleep(10);
        }
    }

    static void finish(String message, boolean failed) throws Exception {
        System.out.println("WATCH " + message);
        Class.forName("oracle.audit.BytecodeAudit", true, ClassLoader.getSystemClassLoader()).getMethod("write").invoke(null);
        if (!headless) {
            Display.setTitle("Silent policy - " + message);
            SpriteBatch batch = new SpriteBatch();
            batch.begin();
            batch.setColor(0, 0, 0, 0.9f);
            batch.draw(ImageMaster.WHITE_SQUARE_IMG, 0, 0, 1280, 180);
            batch.setColor(Color.WHITE);
            FontHelper.renderSmartText(batch, FontHelper.tipBodyFont, message, 24, 155, 1230, 24, Color.WHITE);
            batch.end();
            Display.update();
            if (failed) while (!Display.isCloseRequested()) {
                Display.processMessages();
                Thread.sleep(50);
            }
            else pause(5000);
            Display.destroy();
        }
        System.exit(failed ? 2 : 0);
    }

    public static void run() throws Exception {
        JsonObject config;
        try (Reader input = Files.newBufferedReader(Paths.get(System.getProperty("watch.config")), StandardCharsets.UTF_8)) {
            config = new JsonParser().parse(input).getAsJsonObject();
        }
        headless = config.get("headless").getAsBoolean();
        delayMillis = config.get("delay_ms").getAsLong();
        try (Socket socket = new Socket(InetAddress.getLoopbackAddress(), config.get("port").getAsInt());
             BufferedReader input = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
             BufferedWriter output = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8))) {
            Recorder.initialize();
            Recorder.start(config.getAsJsonObject("job"));
            JsonObject previous = null;
            int count = 0;
            for (;;) {
                boolean terminal = Recorder.terminal(false);
                ArrayList<JsonObject> legal = terminal ? new ArrayList<JsonObject>() : Actions.legal();
                JsonObject request = new JsonObject();
                request.addProperty("seed", Settings.seed);
                request.addProperty("decision", count);
                request.add("previous", previous);
                request.add("snapshot", Snapshot.capture());
                request.add("legal", Recorder.JSON.toJsonTree(legal));
                request.add("terminal", terminal ? new JsonPrimitive(AbstractDungeon.player.isDead ? "death" : "victory") : JsonNull.INSTANCE);
                output.write(Recorder.JSON.toJson(request)); output.newLine(); output.flush();
                String line = input.readLine();
                if (line == null) { finish("Controller disconnected; stopped", true); return; }
                JsonObject response = new JsonParser().parse(line).getAsJsonObject();
                if (response.has("stop")) {
                    finish(response.get("stop").getAsString(), response.get("failed").getAsBoolean());
                    return;
                }
                int index = response.get("index").getAsInt();
                if (terminal || index < 0 || index >= legal.size()) throw new AssertionError("Invalid controller index: " + response);
                policy = response.get("policy").getAsBoolean();
                if (!headless) Display.setTitle("Silent policy - " + (policy ? "policy" : "prefix")
                    + " decision " + count + " floor " + AbstractDungeon.floorNum + " " + legal.get(index));
                if (policy) pause(delayMillis);
                Actions.apply(legal.get(index));
                previous = new JsonObject();
                previous.addProperty("index", index);
                previous.add("action", legal.get(index));
                count++;
                Recorder.settle();
            }
        }
    }
}
