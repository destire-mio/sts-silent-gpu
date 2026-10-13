package oracle.audit;

import java.io.*;
import java.lang.instrument.*;
import java.security.*;
import java.util.*;
import java.util.jar.*;

public final class BytecodeAudit {
    static final Map<String, byte[]> originals = new HashMap<String, byte[]>();
    static final Map<String, byte[]> changed = new TreeMap<String, byte[]>();
    static final Set<String> loaded = new TreeSet<String>();

    public static void premain(String argument, Instrumentation instrumentation) throws Exception {
        try (JarFile jar = new JarFile("desktop-1.0.jar")) {
            Enumeration<JarEntry> entries = jar.entries();
            while (entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                if (!entry.getName().startsWith("com/megacrit/cardcrawl/") || !entry.getName().endsWith(".class")) continue;
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                try (InputStream input = jar.getInputStream(entry)) {
                    byte[] buffer = new byte[8192];
                    int count;
                    while ((count = input.read(buffer)) != -1) bytes.write(buffer, 0, count);
                }
                originals.put(entry.getName().substring(0, entry.getName().length() - 6), bytes.toByteArray());
            }
        }
        instrumentation.addTransformer(new ClassFileTransformer() {
            public byte[] transform(ClassLoader loader, String name, Class<?> redefining, ProtectionDomain domain, byte[] bytes) {
                byte[] original = originals.get(name);
                if (original != null) {
                    loaded.add(name);
                    if (!Arrays.equals(original, bytes)) changed.put(name, bytes);
                }
                return null;
            }
        });
    }
    public static void write() throws Exception {
        try (PrintWriter writer = new PrintWriter("bytecode-audit.txt", "UTF-8")) {
            writer.println("loaded " + loaded.size() + " original " + originals.size());
            for (Map.Entry<String, byte[]> entry : changed.entrySet())
                writer.println(entry.getKey().replace('/', '.') + " " + hash(originals.get(entry.getKey())) + " " + hash(entry.getValue()));
        }
    }
    static String hash(byte[] bytes) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder out = new StringBuilder();
        for (byte value : digest) out.append(String.format("%02x", value & 255));
        return out.toString();
    }
}
