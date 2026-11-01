package io.github.bryful.linecalc;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;

final class DefinitionStore
{
    static final String DEFAULT_LOCATION = "Documents/LineCalc/LineCalc.js";
    private static final String FILE_NAME = "LineCalc.js";
    private static final String TEMPLATE = "// LineCalc ユーザー定義ファイル（UTF-8）\n"
        + "// 起動時と resetJS() 実行時に読み込みます。\n"
        + "// 外部のテキストエディターで編集できます。\n"
        + "// function triple(v) { return v * 3; }\n";
    private final Context context;
    private final SharedPreferences preferences;

    DefinitionStore(Context context)
    {
        this.context = context.getApplicationContext();
        preferences = this.context.getSharedPreferences("linecalc", Context.MODE_PRIVATE);
    }

    String location()
    {
        String tree = preferences.getString("tree", null);
        return tree == null ? DEFAULT_LOCATION : Uri.decode(tree) + "/" + FILE_NAME;
    }

    private Uri treeFile(Uri tree) throws IOException
    {
        ContentResolver resolver = context.getContentResolver();
        String root = DocumentsContract.getTreeDocumentId(tree);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, root);
        try (Cursor cursor = resolver.query(children, new String[] {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME }, null, null, null))
        {
            if (cursor == null) throw new IOException("フォルダーを読み取れません。");
            while (cursor.moveToNext())
            {
                if (FILE_NAME.equals(cursor.getString(1)))
                    return DocumentsContract.buildDocumentUriUsingTree(tree, cursor.getString(0));
            }
        }
        Uri file = DocumentsContract.createDocument(resolver,
            DocumentsContract.buildDocumentUriUsingTree(tree, root), "text/javascript", FILE_NAME);
        if (file == null) throw new IOException("LineCalc.jsを作成できません。");
        writeTemplate(file);
        return file;
    }

    private Uri defaultFile() throws IOException
    {
        ContentResolver resolver = context.getContentResolver();
        Uri collection = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
        try (Cursor cursor = resolver.query(collection,
            new String[] { MediaStore.MediaColumns._ID },
            MediaStore.MediaColumns.RELATIVE_PATH + "=? AND " + MediaStore.MediaColumns.DISPLAY_NAME + "=?",
            new String[] { "Documents/LineCalc/", FILE_NAME }, null))
        {
            if (cursor == null) throw new IOException("保存先を確認できません。");
            if (cursor.moveToFirst()) return ContentUris.withAppendedId(collection, cursor.getLong(0));
        }
        // 所有権を失った既存ファイルを別名の雛形で置き換えない。
        java.io.File existing = new java.io.File(
            android.os.Environment.getExternalStoragePublicDirectory(android.os.Environment.DIRECTORY_DOCUMENTS),
            "LineCalc/LineCalc.js");
        if (existing.exists())
            throw new IOException("既存のLineCalc.jsにアクセスできません。設定からDocuments/LineCalcを選択してください。");
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, FILE_NAME);
        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/javascript");
        values.put(MediaStore.MediaColumns.RELATIVE_PATH, "Documents/LineCalc/");
        values.put(MediaStore.MediaColumns.IS_PENDING, 1);
        Uri file = resolver.insert(collection, values);
        if (file == null) throw new IOException("初期ファイルを作成できません。");
        try
        {
            try (Cursor created = resolver.query(file,
                new String[] { MediaStore.MediaColumns.DISPLAY_NAME }, null, null, null))
            {
                if (created == null || !created.moveToFirst() || !FILE_NAME.equals(created.getString(0)))
                    throw new IOException("既存のLineCalc.jsがあります。設定からDocuments/LineCalcを選択してください。");
            }
            writeTemplate(file);
            ContentValues ready = new ContentValues();
            ready.put(MediaStore.MediaColumns.IS_PENDING, 0);
            if (resolver.update(file, ready, null, null) != 1)
                throw new IOException("初期ファイルを公開できません。");
        }
        catch (IOException | RuntimeException error)
        {
            try { resolver.delete(file, null, null); }
            catch (RuntimeException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
        return file;
    }

    Uri file() throws IOException
    {
        String tree = preferences.getString("tree", null);
        return tree == null ? defaultFile() : treeFile(Uri.parse(tree));
    }

    private void writeTemplate(Uri file) throws IOException
    {
        try (OutputStream output = context.getContentResolver().openOutputStream(file, "wt"))
        {
            if (output == null) throw new IOException("雛形を書き込めません。");
            output.write(TEMPLATE.getBytes(StandardCharsets.UTF_8));
        }
    }

    String read() throws IOException
    {
        try (InputStream input = context.getContentResolver().openInputStream(file()))
        {
            if (input == null) throw new IOException("ファイルを開けません。");
            // 極端に大きなファイルで計算機のメモリを使い切らない。
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            byte[] chunk = new byte[8192];
            int count;
            while ((count = input.read(chunk)) != -1)
            {
                if (buffer.size() + count > 4 * 1024 * 1024)
                    throw new IOException("JSファイルは4MiB以内にしてください。");
                buffer.write(chunk, 0, count);
            }
            byte[] data = buffer.toByteArray();
            String text = StandardCharsets.UTF_8.newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(data)).toString();
            return text.startsWith("\uFEFF") ? text.substring(1) : text;
        }
        catch (IOException | RuntimeException error)
        {
            throw new IOException(location() + "\n" + error.getMessage(), error);
        }
    }

    void selectTree(Uri tree) throws IOException
    {
        treeFile(tree);
        preferences.edit().putString("tree", tree.toString()).apply();
    }

    void useDefault()
    {
        preferences.edit().remove("tree").apply();
    }
}
