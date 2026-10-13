package oracle;

import com.badlogic.gdx.Application;
import com.badlogic.gdx.ApplicationListener;
import com.badlogic.gdx.ApplicationLogger;
import com.badlogic.gdx.Audio;
import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.Graphics;
import com.badlogic.gdx.Input;
import com.badlogic.gdx.InputProcessor;
import com.badlogic.gdx.Net;
import com.badlogic.gdx.Preferences;
import com.badlogic.gdx.backends.lwjgl.LwjglFiles;
import com.badlogic.gdx.backends.lwjgl.LwjglPreferences;
import com.badlogic.gdx.audio.Sound;
import com.badlogic.gdx.graphics.Cursor;
import com.badlogic.gdx.graphics.glutils.GLVersion;
import com.badlogic.gdx.utils.Clipboard;
import com.badlogic.gdx.utils.GdxNativesLoader;
import com.evacipated.cardcrawl.modthespire.lib.SpirePatch;
import com.evacipated.cardcrawl.modthespire.lib.SpirePrefixPatch;
import com.evacipated.cardcrawl.modthespire.lib.SpireReturn;
import com.megacrit.cardcrawl.audio.MockMusic;
import com.megacrit.cardcrawl.actions.GameActionManager;
import com.megacrit.cardcrawl.cards.SoulGroup;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.desktop.DesktopLauncher;
import com.megacrit.cardcrawl.dungeons.AbstractDungeon;
import com.megacrit.cardcrawl.helpers.controller.CInputHelper;
import com.megacrit.cardcrawl.helpers.steamInput.SteamInputHelper;
import com.megacrit.cardcrawl.monsters.AbstractMonster;
import com.megacrit.cardcrawl.rooms.AbstractRoom;
import com.megacrit.cardcrawl.vfx.AbstractGameEffect;
import com.megacrit.cardcrawl.vfx.PlayerTurnEffect;
import com.megacrit.cardcrawl.vfx.combat.BattleStartEffect;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;

public final class Platform {
    static final float DELTA = 1.0f / 60.0f;
    static long frame;
    static int nextObject = 1, framebuffer, activeTexture = 33984;
    public static int nextTextureIdentity;
    public static final java.util.Random deviceRandom = new com.badlogic.gdx.math.RandomXS128(0L);
    static final int[] viewport = {0, 0, 1280, 720};
    static final Map<Integer, Integer> textures = new HashMap<Integer, Integer>();
    static ApplicationListener listener;
    static InputProcessor inputProcessor;
    static boolean running = true;
    static final ConcurrentLinkedQueue<Runnable> jobs = new ConcurrentLinkedQueue<Runnable>();
    static final Map<String, Preferences> preferences = new HashMap<String, Preferences>();
    static final int WIDTH = 1280, HEIGHT = 720;
    @SpirePatch(clz=DesktopLauncher.class, method="main")
    public static class Launcher {
        @SpirePrefixPatch
        public static SpireReturn<Void> before(String[] arg) throws Exception {
            Recorder.run();
            return SpireReturn.Return(null);
        }
    }

    @SpirePatch(clz=CInputHelper.class, method="initializeIfAble")
    public static class NoControllerWindow {
        @SpirePrefixPatch
        public static SpireReturn<Void> before() {
            return SpireReturn.Return(null);
        }
    }

    @SpirePatch(clz=SteamInputHelper.class, method=SpirePatch.CONSTRUCTOR)
    public static class NoControllerThread {
        @SpirePrefixPatch
        public static SpireReturn<Void> before() {
            return SpireReturn.Return(null);
        }
    }

    static boolean effectsSettled() {
        if (CardCrawlGame.mode != CardCrawlGame.GameMode.GAMEPLAY || AbstractDungeon.player == null)
            return true;
        if (SoulGroup.isActive()) return false;
        AbstractRoom room = AbstractDungeon.getCurrRoom();
        if (room == null || room.phase == null) return false;
        if (room.phase == AbstractRoom.RoomPhase.COMBAT) {
            if (room.monsters == null) return false;
            if (!AbstractDungeon.isScreenUp && !AbstractDungeon.player.isDead) {
                GameActionManager manager = AbstractDungeon.actionManager;
                if (manager.phase != GameActionManager.Phase.WAITING_ON_USER
                    || !manager.actions.isEmpty() || !manager.preTurnActions.isEmpty()
                    || !manager.cardQueue.isEmpty() || AbstractDungeon.player.endTurnQueued) return false;
            }
            for (AbstractMonster monster : room.monsters.monsters) {
                if (monster.isDying && !monster.isDead) return false;
                if (!monster.isDeadOrEscaped() && monster.intent == AbstractMonster.Intent.DEBUG) return false;
            }
        }
        return !hasPendingCallback(AbstractDungeon.topLevelEffects)
            && !hasPendingCallback(AbstractDungeon.topLevelEffectsQueue)
            && !hasPendingCallback(AbstractDungeon.effectList)
            && !hasPendingCallback(AbstractDungeon.effectsQueue);
    }

    static boolean hasPendingCallback(ArrayList<AbstractGameEffect> effects) {
        if (effects == null) throw new IllegalStateException("Effect list unavailable");
        for (AbstractGameEffect effect : effects) {
            String name = effect.getClass().getName();
            if (!effect.isDone && (name.startsWith("com.megacrit.cardcrawl.vfx.cardManip.")
                    || name.startsWith("com.megacrit.cardcrawl.vfx.campfire.") && !AbstractDungeon.isScreenUp
                    || effect instanceof PlayerTurnEffect || effect instanceof BattleStartEffect
                    || effect instanceof com.megacrit.cardcrawl.vfx.ObtainKeyEffect)) return true;
        }
        return false;
    }

    @SuppressWarnings("unchecked")
    static <T> T proxy(Class<T> type, InvocationHandler handler) {
        return (T)Proxy.newProxyInstance(type.getClassLoader(), new Class<?>[]{type}, handler);
    }

    static Object objectMethod(Object proxy, Method method, Object[] args) {
        if (method.getName().equals("toString")) return "Oracle:" + proxy.getClass().getInterfaces()[0].getSimpleName();
        if (method.getName().equals("hashCode")) return System.identityHashCode(proxy);
        if (method.getName().equals("equals")) return proxy == args[0];
        throw new UnsupportedOperationException(method.toString());
    }

    static Object zero(Method method) {
        Class<?> result = method.getReturnType();
        if (result == void.class) return null;
        if (result == boolean.class) return false;
        if (result == int.class) return 0;
        if (result == long.class) return 0L;
        if (result == float.class) return 0.0f;
        if (result == double.class) return 0.0;
        return null;
    }

    static void initializePlatform() {
        GdxNativesLoader.load();
        Gdx.files = new LwjglFiles();
        Gdx.gl20 = new NullGL20();
        Gdx.gl = Gdx.gl20;
        Gdx.gl30 = null;
        final GLVersion version = new GLVersion(Application.ApplicationType.Desktop,"2.1","Oracle","no-device");
        final Graphics.Monitor monitor = new Graphics.Monitor(0,0,"no-window") {};
        final Graphics.DisplayMode mode = new Graphics.DisplayMode(WIDTH,HEIGHT,60,32) {};
        Gdx.graphics = proxy(Graphics.class,(p,m,a) -> {
            if(m.getDeclaringClass()==Object.class)return objectMethod(p,m,a);
            switch(m.getName()) {
                case "getWidth": case "getBackBufferWidth": return WIDTH;
                case "getHeight": case "getBackBufferHeight": return HEIGHT;
                case "getFrameId": return frame;
                case "getDeltaTime": case "getRawDeltaTime": return DELTA;
                case "getFramesPerSecond": return 60;
                case "getGL20": return Gdx.gl20;
                case "getGLVersion": return version;
                case "getType": return Graphics.GraphicsType.LWJGL;
                case "getDensity": return 1.0f;
                case "getPpiX": case "getPpiY": return 96.0f;
                case "getPpcX": case "getPpcY": return 96.0f/2.54f;
                case "getPrimaryMonitor": case "getMonitor": return monitor;
                case "getMonitors": return new Graphics.Monitor[]{monitor};
                case "getDisplayMode": return mode;
                case "getDisplayModes": return new Graphics.DisplayMode[]{mode};
                case "getBufferFormat": return new Graphics.BufferFormat(8,8,8,8,16,0,0,false);
                case "newCursor": return proxy(Cursor.class,(x,y,z)->zero(y));
                case "setWindowedMode": return true;
                case "isContinuousRendering": return true;
                default: return zero(m);
            }
        });
        Gdx.input = proxy(Input.class,(p,m,a) -> {
            if(m.getDeclaringClass()==Object.class)return objectMethod(p,m,a);
            switch(m.getName()) {
                case "getX": case "getY": return -10000;
                case "getNativeOrientation": return Input.Orientation.Landscape;
                case "getCurrentEventTime": return frame*16666667L;
                case "setInputProcessor": inputProcessor=(InputProcessor)a[0];return null;
                case "getInputProcessor": return inputProcessor;
                case "getTextInput": throw new UnsupportedOperationException("Text input requires a protocol action");
                default: return zero(m);
            }
        });
        Gdx.audio = proxy(Audio.class,(p,m,a)-> {
            if(m.getDeclaringClass()==Object.class)return objectMethod(p,m,a);
            if(m.getName().equals("newMusic"))return new MockMusic();
            if(m.getName().equals("newSound"))return proxy(Sound.class,(x,y,z)->zero(y));
            throw new UnsupportedOperationException("Audio device unavailable: "+m.getName());
        });
        Gdx.net = proxy(Net.class,(p,m,a)-> {
            if(m.getDeclaringClass()==Object.class)return objectMethod(p,m,a);
            throw new UnsupportedOperationException("Network UI unavailable: "+m.getName());
        });
        final Clipboard clipboard=new Clipboard(){
            String contents="";
            public String getContents(){return contents;}
            public void setContents(String value){contents=value;}
        };
        final ApplicationLogger logger=new ApplicationLogger(){
            public void log(String t,String s){System.out.println(t+": "+s);}
            public void log(String t,String s,Throwable e){log(t,s);e.printStackTrace();}
            public void error(String t,String s){log(t,s);}
            public void error(String t,String s,Throwable e){log(t,s,e);}
            public void debug(String t,String s){log(t,s);}
            public void debug(String t,String s,Throwable e){log(t,s,e);}
        };
        Gdx.app = proxy(Application.class,(p,m,a)-> {
            if(m.getDeclaringClass()==Object.class)return objectMethod(p,m,a);
            switch(m.getName()) {
                case "getType": return Application.ApplicationType.Desktop;
                case "getApplicationListener": return listener;
                case "getGraphics": return Gdx.graphics;
                case "getFiles": return Gdx.files;
                case "getInput": return Gdx.input;
                case "getAudio": return Gdx.audio;
                case "getNet": return Gdx.net;
                case "getLogLevel": return Application.LOG_INFO;
                case "getApplicationLogger": return logger;
                case "getJavaHeap": return Runtime.getRuntime().totalMemory()-Runtime.getRuntime().freeMemory();
                case "getNativeHeap": return 0L;
                case "getClipboard": return clipboard;
                case "getPreferences":
                    String name=(String)a[0];
                    if(!preferences.containsKey(name))preferences.put(name,new LwjglPreferences(name,".prefs/"));
                    return preferences.get(name);
                case "postRunnable": jobs.add((Runnable)a[0]);return null;
                case "exit": running=false;return null;
                case "log": case "error": case "debug":
                    System.out.println(a[0]+": "+a[1]);
                    if(a.length>2)((Throwable)a[2]).printStackTrace();
                    return null;
                default: return zero(m);
            }
        });
    }
}
