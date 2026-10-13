package oracle;

import com.badlogic.gdx.math.RandomXS128;

public final class Unseeded {
    public static final class Stream extends RandomXS128 {
        public long counter;
        public Stream(long seed) { super(seed); }
        @Override public long nextLong() { counter++; return super.nextLong(); }
    }
    public static Stream rng = new Stream(0L);
    public static long uuid;
    public static int random(int range) { return rng.nextInt(range + 1); }
    public static int random(int start, int end) { return start + rng.nextInt(end - start + 1); }
    public static long random(long range) { return (long)(rng.nextDouble() * range); }
    public static float random() { return rng.nextFloat(); }
    public static float random(float range) { return rng.nextFloat() * range; }
    public static float random(float start, float end) { return start + rng.nextFloat() * (end - start); }
    public static boolean randomBoolean() { return rng.nextBoolean(); }
    public static boolean randomBoolean(float chance) { return rng.nextFloat() < chance; }
}
