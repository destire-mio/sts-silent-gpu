package oracle;

import com.google.gson.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.jar.*;

public final class FieldSchema {
    public static void main(String[] args) throws Exception {
        TreeMap<String, JsonObject> classes = new TreeMap<String, JsonObject>();
        try (JarFile jar = new JarFile(args[0])) {
            Enumeration<JarEntry> entries = jar.entries();
            while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName();
                if (!name.startsWith("com/megacrit/cardcrawl/") || !name.endsWith(".class")) continue;
                String binaryName = name.substring(0, name.length() - 6).replace('/', '.');
                Class<?> cls = Class.forName(binaryName, false, FieldSchema.class.getClassLoader());
                if (cls.isEnum() || cls.isInterface()) continue;
                TreeMap<String, String> fields = new TreeMap<String, String>();
                for (Field field : cls.getDeclaredFields()) {
                    if (Snapshot.included(field)) fields.put(field.getName(), field.getGenericType().getTypeName());
                }
                JsonObject object = new JsonObject();
                object.addProperty("extends", cls.getSuperclass() == Object.class ? null : cls.getSuperclass().getName().replace("com.megacrit.cardcrawl.", ""));
                JsonObject data = new JsonObject();
                for (Map.Entry<String, String> field : fields.entrySet()) data.addProperty(field.getKey(), field.getValue());
                object.add("fields", data);
                classes.put(binaryName.replace("com.megacrit.cardcrawl.", ""), object);
            }
        }
        JsonObject catalog = new JsonObject();
        for (Map.Entry<String, JsonObject> entry : classes.entrySet()) catalog.add(entry.getKey(), entry.getValue());
        catalog.add("excluded", new Gson().toJsonTree(Snapshot.EXCLUDED));
        Gson json = new GsonBuilder().serializeNulls().setPrettyPrinting().disableHtmlEscaping().create();
        Files.write(Paths.get(args[1]), (json.toJson(catalog) + "\n").getBytes(StandardCharsets.UTF_8));
    }
}
