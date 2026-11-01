package io.github.bryful.linecalc;

import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.text.InputType;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;
import java.util.Arrays;

public final class MainActivity extends Activity
{
    private static final int PICK_FOLDER = 10;
    private static final int BACKGROUND = Color.rgb(20, 35, 44);
    private static final int SURFACE = Color.rgb(30, 48, 58);
    private static final int ACCENT = Color.rgb(124, 231, 191);
    private CalculatorSession session;
    private EditText input;
    private TextView status;
    private LinearLayout controls;
    private CalculatorKeyboard keyboard;
    private final ArrayList<Dialog> dialogs = new ArrayList<>();

    @Override public void onCreate(Bundle state)
    {
        super.onCreate(state);
        if (android.os.Build.VERSION.SDK_INT >= 30) getWindow().setDecorFitsSystemWindows(false);
        session = (CalculatorSession)getLastNonConfigurationInstance();
        boolean fresh = session == null;
        if (fresh) session = new CalculatorSession(this);
        session.activity = this;
        configureSoftInput();
        buildScreen();
        if (fresh) execute("", 1, -1, false);
        else setBusy(session.busy);
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    private GradientDrawable background(int color, int radius)
    {
        GradientDrawable result = new GradientDrawable();
        result.setColor(color);
        result.setCornerRadius(dp(radius));
        return result;
    }

    private Button button(String label, Runnable action)
    {
        Button result = new Button(this);
        result.setText(label);
        result.setTextSize(14);
        result.setTextColor(ACCENT);
        result.setAllCaps(false);
        result.setMinWidth(0);
        result.setMinimumWidth(0);
        result.setMinHeight(0);
        result.setMinimumHeight(0);
        result.setMaxLines(1);
        result.setHorizontallyScrolling(false);
        result.setEllipsize(TextUtils.TruncateAt.END);
        result.setIncludeFontPadding(false);
        result.setAutoSizeTextTypeUniformWithConfiguration(12, 14, 1, TypedValue.COMPLEX_UNIT_SP);
        result.setPadding(dp(8), 0, dp(8), 0);
        result.setBackground(background(SURFACE, 8));
        result.setOnClickListener(view -> action.run());
        return result;
    }

    private void buildScreen()
    {
        boolean landscape = getResources().getConfiguration().orientation
            == android.content.res.Configuration.ORIENTATION_LANDSCAPE;
        getWindow().setStatusBarColor(BACKGROUND);
        getWindow().setNavigationBarColor(BACKGROUND);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BACKGROUND);
        root.setPadding(dp(12), dp(8), dp(12), dp(8));
        if (android.os.Build.VERSION.SDK_INT >= 30) root.setOnApplyWindowInsetsListener((view, insets) ->
        {
            android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars());
            android.graphics.Insets ime = insets.getInsets(WindowInsets.Type.ime());
            view.setPadding(dp(12) + bars.left, dp(8) + bars.top,
                dp(12) + bars.right, dp(8) + Math.max(bars.bottom, ime.bottom));
            return insets;
        });
        setContentView(root);

        HorizontalScrollView toolbar = new HorizontalScrollView(this);
        toolbar.setHorizontalScrollBarEnabled(false);
        controls = new LinearLayout(this);
        controls.setBaselineAligned(false);
        addTool("履歴", this::showHistory);
        addTool("メモリ", this::showMemories);
        addTool("関数", () -> execute("funcs()", 0, -1, false));
        addTool("再読込", () -> execute("", 1, -1, false));
        Button menu = button("⋮", () -> {});
        menu.setContentDescription("その他の機能");
        menu.setOnClickListener(view -> showMenu(view));
        controls.addView(menu, new LinearLayout.LayoutParams(dp(48), dp(landscape ? 40 : 48)));
        toolbar.addView(controls);
        root.addView(toolbar, new LinearLayout.LayoutParams(-1, dp(landscape ? 44 : 52)));

        LinearLayout entry = new LinearLayout(this);
        entry.setBaselineAligned(false);
        entry.setGravity(Gravity.CENTER_VERTICAL);
        entry.setPadding(dp(4), dp(landscape ? 2 : 10), 0, dp(landscape ? 2 : 6));
        input = new EditText(this);
        input.setId(R.id.calculation);
        input.setSingleLine(true);
        input.setTextSize(24);
        input.setTextColor(Color.WHITE);
        input.setHintTextColor(Color.rgb(139, 161, 172));
        input.setHint("式を入力");
        input.setTypeface(Typeface.MONOSPACE);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        input.setShowSoftInputOnFocus(session.systemKeyboard);
        input.setImeOptions(EditorInfo.IME_ACTION_GO | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        input.setContentDescription("計算式と結果");
        input.setText(session.input);
        input.setSelection(Math.min(session.selectionStart, input.length()), Math.min(session.selectionEnd, input.length()));
        input.setOnEditorActionListener((view, action, event) ->
        {
            if (action == EditorInfo.IME_ACTION_GO || (event != null
                && event.getKeyCode() == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_DOWN))
            {
                calculate();
                return true;
            }
            return false;
        });
        input.setOnKeyListener((view, code, event) ->
        {
            if (event.getAction() != KeyEvent.ACTION_DOWN || session.busy) return false;
            if (code == KeyEvent.KEYCODE_DPAD_UP) { navigateHistory(-1); return true; }
            if (code == KeyEvent.KEYCODE_DPAD_DOWN) { navigateHistory(1); return true; }
            return false;
        });
        entry.addView(input, new LinearLayout.LayoutParams(0, dp(landscape ? 48 : 64), 1));
        Button run = button("実行", this::calculate);
        run.setContentDescription("計算を実行");
        run.setMinimumWidth(dp(60));
        entry.addView(run, new LinearLayout.LayoutParams(-2, dp(landscape ? 44 : 52)));
        root.addView(entry);

        status = new TextView(this);
        status.setTextColor(Color.rgb(139, 161, 172));
        status.setTextSize(12);
        status.setPadding(dp(4), dp(landscape ? 2 : 10), 0, 0);
        status.setText(R.string.ready);
        root.addView(status);
        keyboard = new CalculatorKeyboard(this, input, this::calculate);
        keyboard.setMode(session.keyboardMode, session.uppercaseKeyboard);
        keyboard.setVisibility(session.systemKeyboard ? View.GONE : View.VISIBLE);
        root.addView(keyboard, new LinearLayout.LayoutParams(-1, 0, 1));
        focusKeyboard();
    }

    private void addTool(String title, Runnable action)
    {
        boolean landscape = getResources().getConfiguration().orientation
            == android.content.res.Configuration.ORIENTATION_LANDSCAPE;
        LinearLayout.LayoutParams parameters = new LinearLayout.LayoutParams(-2, dp(landscape ? 40 : 48));
        parameters.setMargins(0, 0, dp(4), 0);
        Button tool = button(title, action);
        tool.setMinimumWidth(dp(68));
        controls.addView(tool, parameters);
    }

    private void rememberInput()
    {
        session.input = input.getText().toString();
        session.selectionStart = Math.max(0, input.getSelectionStart());
        session.selectionEnd = Math.max(0, input.getSelectionEnd());
        session.keyboardMode = keyboard.getMode();
        session.uppercaseKeyboard = keyboard.isUppercase();
    }

    @Override public Object onRetainNonConfigurationInstance()
    {
        rememberInput();
        return session;
    }

    @Override protected void onDestroy()
    {
        for (Dialog dialog : new ArrayList<>(dialogs)) dialog.dismiss();
        if (session.activity == this) session.activity = null;
        if (!isChangingConfigurations()) session.close();
        super.onDestroy();
    }

    @Override protected void onPause()
    {
        keyboard.stopRepeat();
        super.onPause();
    }

    private void setBusy(boolean busy)
    {
        input.setEnabled(!busy);
        keyboard.setKeysEnabled(!busy);
        for (int i = 0; i < controls.getChildCount(); ++i) controls.getChildAt(i).setEnabled(!busy);
        status.setText(busy ? R.string.busy : R.string.ready);
        if (!busy) focusKeyboard();
    }

    private void focusKeyboard()
    {
        input.requestFocus();
        input.post(() ->
        {
            if (session.busy || !hasWindowFocus()) return;
            InputMethodManager manager = (InputMethodManager)getSystemService(INPUT_METHOD_SERVICE);
            if (session.systemKeyboard) manager.showSoftInput(input, InputMethodManager.SHOW_IMPLICIT);
            else manager.hideSoftInputFromWindow(input.getWindowToken(), 0);
        });
    }

    @Override public void onWindowFocusChanged(boolean focused)
    {
        super.onWindowFocusChanged(focused);
        if (focused && input != null && !session.busy) focusKeyboard();
    }

    private void calculate()
    {
        String source = input.getText().toString();
        if (source.isBlank() || session.busy) return;
        session.history.add(source);
        session.historyIndex = session.history.size();
        session.draft = "";
        execute(source, 0, -1, true);
    }

    private void execute(String source, int action, int memory, boolean replace)
    {
        if (session.busy) return;
        rememberInput();
        session.busy = true;
        setBusy(true);
        CalculatorSession current = session;
        current.worker.execute(() ->
        {
            String[] result = null;
            String failure = null;
            try { result = current.execute(source, action, memory); }
            catch (Exception | LinkageError error) { failure = error.getMessage(); }
            final String[] completed = result;
            final String errorText = failure;
            runOnUiThread(() ->
            {
                current.busy = false;
                if (completed != null)
                {
                    if ("1".equals(completed[4]))
                    {
                        current.history.clear();
                        current.historyIndex = 0;
                        current.draft = "";
                    }
                    if (replace && "1".equals(completed[0]))
                    {
                        current.input = completed[1];
                        current.selectionStart = current.selectionEnd = current.input.length();
                    }
                }
                MainActivity active = current.activity;
                if (active == null) return;
                active.setBusy(false);
                if (errorText != null)
                {
                    active.showText("エラー", errorText);
                    return;
                }
                if (completed == null) return;
                if (!"1".equals(completed[0])) active.showText("エラー", completed[2]);
                else
                {
                    if (replace) active.setInput(current.input);
                    Toast.makeText(active, action == 2 ? "JSの検証が完了しました" : "OK", Toast.LENGTH_SHORT).show();
                }
                for (int i = 5; i + 1 < completed.length; i += 2)
                {
                    if ("関数一覧".equals(completed[i])) active.showFunctions(completed[i + 1]);
                    else active.showText(completed[i], completed[i + 1]);
                }
            });
        });
    }

    private void setInput(String value)
    {
        input.setText(value);
        input.setSelection(input.length());
        rememberInput();
    }

    private void insert(String value, boolean function)
    {
        if (session.busy) return;
        int start = Math.max(0, input.getSelectionStart());
        int end = Math.max(0, input.getSelectionEnd());
        input.getText().replace(Math.min(start, end), Math.max(start, end), value);
        input.setSelection(Math.min(start, end) + value.length() - (function ? 1 : 0));
        input.requestFocus();
        rememberInput();
    }

    private void navigateHistory(int delta)
    {
        if (session.history.isEmpty()) return;
        if (session.historyIndex == session.history.size()) session.draft = input.getText().toString();
        else session.history.set(session.historyIndex, input.getText().toString());
        session.historyIndex = Math.max(0, Math.min(session.history.size(), session.historyIndex + delta));
        setInput(session.historyIndex == session.history.size() ? session.draft : session.history.get(session.historyIndex));
    }

    private LinearLayout sheet(String title)
    {
        Dialog dialog = new Dialog(this);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(16), dp(12), dp(16), dp(16));
        content.setBackground(background(SURFACE, 16));
        LinearLayout heading = new LinearLayout(this);
        TextView label = new TextView(this);
        label.setText(title);
        label.setTextColor(Color.WHITE);
        label.setTextSize(20);
        label.setGravity(Gravity.CENTER_VERTICAL);
        heading.addView(label, new LinearLayout.LayoutParams(0, dp(48), 1));
        heading.addView(button("閉じる", dialog::dismiss), new LinearLayout.LayoutParams(dp(72), dp(48)));
        content.addView(heading);
        ScrollView scroll = new ScrollView(this);
        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(body);
        content.addView(scroll, new LinearLayout.LayoutParams(-1, -2));
        dialog.setContentView(content);
        Window window = dialog.getWindow();
        if (window != null)
        {
            window.setBackgroundDrawableResource(android.R.color.transparent);
            window.addFlags(WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
            window.setGravity(Gravity.BOTTOM);
            window.setLayout(-1, Math.min(dp(440), getResources().getDisplayMetrics().heightPixels * 2 / 3));
        }
        dialogs.add(dialog);
        dialog.setOnDismissListener(ignored -> dialogs.remove(dialog));
        dialog.show();
        if (window != null) window.setLayout(-1, Math.min(dp(440), getResources().getDisplayMetrics().heightPixels * 2 / 3));
        body.setTag(dialog);
        return body;
    }

    private void showText(String title, String text)
    {
        LinearLayout body = sheet(title);
        TextView value = new TextView(this);
        value.setText(text);
        value.setTextColor(Color.WHITE);
        value.setTextSize(16);
        value.setPadding(0, dp(12), 0, dp(12));
        value.setTextIsSelectable(true);
        body.addView(value);
    }

    private void showHistory()
    {
        LinearLayout body = sheet("入力履歴");
        if (session.history.isEmpty())
        {
            TextView empty = new TextView(this);
            empty.setText("履歴はありません。");
            empty.setTextColor(Color.WHITE);
            body.addView(empty);
        }
        for (int i = session.history.size() - 1; i >= 0; --i)
        {
            final int index = i;
            body.addView(button(session.history.get(i), () ->
            {
                if (session.historyIndex == session.history.size()) session.draft = input.getText().toString();
                session.historyIndex = index;
                setInput(session.history.get(index));
                ((Dialog)body.getTag()).dismiss();
            }), new LinearLayout.LayoutParams(-1, dp(52)));
        }
        body.addView(button("履歴を消去", () ->
        {
            session.history.clear();
            session.historyIndex = 0;
            session.draft = "";
            ((Dialog)body.getTag()).dismiss();
        }), new LinearLayout.LayoutParams(-1, dp(48)));
    }

    private void showFunctions(String userFunctions)
    {
        LinearLayout body = sheet("関数を挿入");
        ArrayList<String> functions = new ArrayList<>();
        for (String name : userFunctions.split("\\R"))
            if (name.endsWith("()")) functions.add(name);
        functions.addAll(Arrays.asList("Math.sin()", "Math.cos()", "Math.sqrt()", "Math.pow()", "Math.PI",
            "hex()", "bin()", "ans", "help()", "vars()", "funcs()", "mem()", "clearMem()", "clearHis()", "resetJS()"));
        for (String name : functions)
            body.addView(button(name, () ->
            {
                insert(name, name.endsWith("()"));
                ((Dialog)body.getTag()).dismiss();
            }), new LinearLayout.LayoutParams(-1, dp(48)));
    }

    private void showMemories()
    {
        if (session.busy) return;
        session.busy = true;
        setBusy(true);
        CalculatorSession current = session;
        current.worker.execute(() ->
        {
            try
            {
                String[] values = current.memories();
                runOnUiThread(() ->
                {
                    current.busy = false;
                    MainActivity active = current.activity;
                    if (active != null) { active.setBusy(false); active.memorySheet(values); }
                });
            }
            catch (Exception | LinkageError error)
            {
                runOnUiThread(() ->
                {
                    current.busy = false;
                    MainActivity active = current.activity;
                    if (active != null) { active.setBusy(false); active.showText("エラー", error.getMessage()); }
                });
            }
        });
    }

    private void memorySheet(String[] values)
    {
        LinearLayout body = sheet("メモリ");
        for (int i = 0; i < 12; ++i)
        {
            final int index = i;
            final String name = "M" + (i + 1);
            LinearLayout row = new LinearLayout(this);
            row.addView(button(name + "  " + values[i], () ->
            {
                insert(values[index], false);
                ((Dialog)body.getTag()).dismiss();
            }), new LinearLayout.LayoutParams(0, dp(52), 1));
            row.addView(button("保存", () ->
            {
                ((Dialog)body.getTag()).dismiss();
                execute(input.getText().toString(), 3, index, false);
            }), new LinearLayout.LayoutParams(dp(60), dp(52)));
            row.addView(button("消去", () ->
            {
                ((Dialog)body.getTag()).dismiss();
                execute(name + "=null", 0, -1, false);
            }), new LinearLayout.LayoutParams(dp(60), dp(52)));
            body.addView(row);
        }
    }

    private void showMenu(View anchor)
    {
        PopupMenu menu = new PopupMenu(this, anchor);
        String[] items = { "設定", "定義JSを編集", "定義JSを検証", "変数一覧", "ヘルプ", "ライセンス",
            session.systemKeyboard ? "専用キーボードに切替" : "標準キーボードに切替" };
        for (int i = 0; i < items.length; ++i) menu.getMenu().add(0, i, i, items[i]);
        menu.setOnMenuItemClickListener(item ->
        {
            switch (item.getItemId())
            {
                case 0: settings(); break;
                case 1: editDefinitions(); break;
                case 2: execute("", 2, -1, false); break;
                case 3: execute("vars()", 0, -1, false); break;
                case 4: showText("ヘルプ", "入力欄にJavaScriptの式を入力し、実行またはEnterで計算します。\n"
                    + "専用キーボードは数値・英字・JSの3面を切り替えて使えます。\n"
                    + "⌫と◀／▶は長押しで連続操作。ACは入力行だけを消去します。\n"
                    + "メニューから標準キーボードにも切り替えられます。\n"
                    + "例：(15-2)*12\nMath.sin(Math.PI/2)\nfunction triple(v){return v*3;}\ntriple(5)\n\n"
                    + "ans：直前の数値結果\nM1～M12：メモリ\n↑／↓：入力履歴\n"
                    + "再読込：変数・メモリを初期化し、定義JSを読み直します。\n"
                    + "定義JSはUTF-8で保存してください。\n実行時間の上限は2秒です。\n"
                    + "Windows専用の最前面・フォント関数は使用できません。\n"
                    + "履歴・変数・メモリはアプリのプロセス終了時に消去されます。"); break;
                case 5: licenses(); break;
                case 6: toggleKeyboard(); break;
                default: return false;
            }
            return true;
        });
        menu.show();
    }

    private void toggleKeyboard()
    {
        keyboard.stopRepeat();
        session.systemKeyboard = !session.systemKeyboard;
        configureSoftInput();
        getSharedPreferences("linecalc", MODE_PRIVATE).edit()
            .putBoolean("systemKeyboard", session.systemKeyboard).apply();
        input.setShowSoftInputOnFocus(session.systemKeyboard);
        keyboard.setVisibility(session.systemKeyboard ? View.GONE : View.VISIBLE);
        focusKeyboard();
        input.getRootView().requestApplyInsets();
    }

    private void configureSoftInput()
    {
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
            | (session.systemKeyboard ? WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                : WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN));
    }

    private void settings()
    {
        LinearLayout body = sheet("設定");
        TextView location = new TextView(this);
        location.setText(getString(R.string.definition_location, session.store.location()));
        location.setTextColor(Color.WHITE);
        location.setTextIsSelectable(true);
        body.addView(location);
        body.addView(button("読込フォルダーを変更", () ->
        {
            ((Dialog)body.getTag()).dismiss();
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                Uri.parse("content://com.android.externalstorage.documents/document/primary%3ADocuments%2FLineCalc"));
            startActivityForResult(intent, PICK_FOLDER);
        }), new LinearLayout.LayoutParams(-1, dp(52)));
        body.addView(button("既定のDocuments/LineCalcに戻す", () ->
        {
            session.store.useDefault();
            ((Dialog)body.getTag()).dismiss();
            execute("", 1, -1, false);
        }), new LinearLayout.LayoutParams(-1, dp(52)));
    }

    @Override protected void onActivityResult(int request, int result, Intent data)
    {
        super.onActivityResult(request, result, data);
        if (request != PICK_FOLDER || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri tree = data.getData();
        try
        {
            getContentResolver().takePersistableUriPermission(tree,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            session.store.selectTree(tree);
            execute("", 1, -1, false);
        }
        catch (Exception error) { showText("読込先を変更できません", tree + "\n" + error.getMessage()); }
    }

    private void editDefinitions()
    {
        try
        {
            Uri file = session.store.file();
            Intent intent = new Intent(Intent.ACTION_EDIT);
            intent.setDataAndType(file, "text/plain");
            intent.setClipData(android.content.ClipData.newRawUri("LineCalc.js", file));
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            startActivity(Intent.createChooser(intent, "定義JSを編集"));
        }
        catch (Exception error) { showText("定義JSを開けません", session.store.location() + "\n" + error.getMessage()); }
    }

    private void licenses()
    {
        try
        {
            StringBuilder text = new StringBuilder();
            for (String name : new String[] { "LICENSE", "THIRD_PARTY_NOTICES.md", "quickjs-LICENSE.txt", "acorn-LICENSE.txt" })
            {
                try (var stream = getAssets().open(name);
                     var reader = new java.io.BufferedReader(new java.io.InputStreamReader(stream, java.nio.charset.StandardCharsets.UTF_8)))
                {
                    String line;
                    while ((line = reader.readLine()) != null) text.append(line).append('\n');
                }
            }
            showText("ライセンス", text.toString());
        }
        catch (Exception error) { showText("ライセンスを表示できません", error.getMessage()); }
    }
}
