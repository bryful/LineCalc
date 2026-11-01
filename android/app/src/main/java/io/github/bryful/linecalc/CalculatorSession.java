package io.github.bryful.linecalc;

import android.content.Context;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

final class CalculatorSession
{
    final ExecutorService worker = Executors.newSingleThreadExecutor(
        task -> new Thread(null, task, "LineCalc", 8 * 1024 * 1024));
    final ArrayList<String> history = new ArrayList<>();
    final DefinitionStore store;
    final File snapshot;
    String input = "";
    int selectionStart;
    int selectionEnd;
    int historyIndex;
    String draft = "";
    boolean busy;
    int keyboardMode;
    boolean uppercaseKeyboard;
    boolean systemKeyboard;
    MainActivity activity;
    private long handle;
    private final Context context;

    CalculatorSession(Context context)
    {
        this.context = context.getApplicationContext();
        systemKeyboard = this.context.getSharedPreferences("linecalc", Context.MODE_PRIVATE)
            .getBoolean("systemKeyboard", false);
        store = new DefinitionStore(context);
        snapshot = new File(context.getCacheDir(), "LineCalc.js");
    }

    private byte[] asset(String name) throws IOException
    {
        try (var stream = context.getAssets().open(name);
             var buffer = new java.io.ByteArrayOutputStream())
        {
            byte[] chunk = new byte[8192];
            int count;
            while ((count = stream.read(chunk)) != -1) buffer.write(chunk, 0, count);
            return buffer.toByteArray();
        }
    }

    void initialize() throws IOException
    {
        if (handle == 0)
            handle = NativeCalculator.create(asset("acorn.js"), asset("Runtime.js"),
                snapshot.getAbsolutePath().getBytes(StandardCharsets.UTF_8));
    }

    private void refresh() throws IOException
    {
        String source = store.read();
        Files.write(snapshot.toPath(), source.getBytes(StandardCharsets.UTF_8));
    }

    String[] execute(String source, int action, int memory) throws IOException
    {
        initialize();
        if (action == 1 || action == 2) refresh();
        String[] result = NativeCalculator.execute(handle, source.getBytes(StandardCharsets.UTF_8), action, memory);
        if ((action == 0 || action == 3) && "1".equals(result[3]))
        {
            refresh();
            String[] loaded = NativeCalculator.execute(handle, new byte[0], 1, -1);
            if ("1".equals(result[4])) loaded[4] = "1";
            if ("0".equals(result[0]) && "1".equals(loaded[0]))
            {
                loaded[0] = result[0];
                loaded[2] = result[2];
            }
            String[] combined = java.util.Arrays.copyOf(loaded, loaded.length + result.length - 5);
            System.arraycopy(result, 5, combined, loaded.length, result.length - 5);
            result = combined;
        }
        result[2] = result[2].replace(snapshot.getAbsolutePath(), store.location());
        return result;
    }

    String[] memories() throws IOException
    {
        initialize();
        return NativeCalculator.memories(handle);
    }

    void close()
    {
        worker.execute(() ->
        {
            if (handle != 0) NativeCalculator.destroy(handle);
            handle = 0;
        });
        worker.shutdown();
    }
}
