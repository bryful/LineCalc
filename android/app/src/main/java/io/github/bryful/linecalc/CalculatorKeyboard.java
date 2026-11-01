package io.github.bryful.linecalc;

import android.content.Context;
import android.annotation.SuppressLint;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.util.TypedValue;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import java.util.Locale;

// 入力欄と実行処理を渡して生成し、XMLからは生成しない。
@SuppressLint("ViewConstructor")
final class CalculatorKeyboard extends LinearLayout
{
    private static final int SURFACE = Color.rgb(30, 48, 58);
    private static final int ACCENT = Color.rgb(124, 231, 191);
    private final EditText input;
    private final Runnable execute;
    private final LinearLayout keys;
    private final Button[] tabs = new Button[3];
    private final boolean landscape;
    private int mode;
    private boolean uppercase;
    private boolean keysEnabled = true;
    private Button repeatingKey;
    private boolean repeated;
    private final Runnable repeat = new Runnable()
    {
        @Override public void run()
        {
            if (repeatingKey == null || !keysEnabled) return;
            repeated = true;
            repeatingKey.performClick();
            postDelayed(this, 65);
        }
    };

    CalculatorKeyboard(Context context, EditText input, Runnable execute)
    {
        super(context);
        this.input = input;
        this.execute = execute;
        landscape = getResources().getConfiguration().orientation
            == android.content.res.Configuration.ORIENTATION_LANDSCAPE;
        setOrientation(VERTICAL);
        setContentDescription("LineCalc専用キーボード");
        LinearLayout navigation = new LinearLayout(context);
        navigation.setBaselineAligned(false);
        String[] names = { "数値", "英字", "JS" };
        for (int i = 0; i < names.length; ++i)
        {
            final int index = i;
            tabs[i] = key(names[i], () -> setMode(index, uppercase));
            navigation.addView(tabs[i], keyParameters(1));
        }
        Button left = key("◀", () -> move(-1));
        left.setContentDescription("カーソルを左へ移動");
        repeatable(left);
        navigation.addView(left, keyParameters(0.8f));
        Button right = key("▶", () -> move(1));
        right.setContentDescription("カーソルを右へ移動");
        repeatable(right);
        navigation.addView(right, keyParameters(0.8f));
        addView(navigation, new LayoutParams(-1, dp(landscape ? 40 : 48)));
        keys = new LinearLayout(context);
        keys.setOrientation(VERTICAL);
        addView(keys, new LayoutParams(-1, 0, 1));
        setMode(0, false);
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    private GradientDrawable color(int value)
    {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(value);
        drawable.setCornerRadius(dp(8));
        return drawable;
    }

    private void style(Button key, boolean highlighted)
    {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[] { android.R.attr.state_pressed }, color(Color.rgb(58, 98, 94)));
        drawable.addState(new int[0], color(highlighted ? ACCENT : SURFACE));
        key.setBackground(drawable);
        key.setTextColor(highlighted ? Color.rgb(20, 35, 44) : Color.WHITE);
    }

    private Button key(String label, Runnable action)
    {
        Button button = new Button(getContext());
        button.setText(label);
        button.setTextSize(19);
        button.setAllCaps(false);
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(0);
        button.setMinimumHeight(0);
        button.setPadding(0, 0, 0, 0);
        button.setIncludeFontPadding(false);
        button.setMaxLines(1);
        button.setHorizontallyScrolling(false);
        button.setAutoSizeTextTypeUniformWithConfiguration(12, 19, 1, TypedValue.COMPLEX_UNIT_SP);
        button.setFocusable(false);
        button.setEnabled(keysEnabled);
        style(button, label.equals("実行"));
        button.setOnClickListener(view ->
        {
            if (keysEnabled && input.isEnabled()) action.run();
        });
        if (label.equals("⌫") || label.equals("BS"))
        {
            button.setContentDescription("選択範囲または直前の文字を削除");
            repeatable(button);
        }
        else if (label.equals("AC")) button.setContentDescription("入力行を消去");
        return button;
    }

    private LayoutParams keyParameters(float weight)
    {
        LayoutParams parameters = new LayoutParams(0, -1, weight);
        parameters.setMargins(dp(2), dp(2), dp(2), dp(2));
        return parameters;
    }

    // タッチとアクセシビリティのクリックをButton.performClick()に集約する。
    @SuppressLint("ClickableViewAccessibility")
    private void repeatable(Button button)
    {
        button.setOnTouchListener((view, event) ->
        {
            switch (event.getActionMasked())
            {
                case MotionEvent.ACTION_DOWN:
                    if (!keysEnabled) return false;
                    stopRepeat();
                    repeatingKey = button;
                    repeated = false;
                    button.setPressed(true);
                    postDelayed(repeat, ViewConfiguration.getLongPressTimeout());
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if (event.getX() < 0 || event.getX() >= view.getWidth()
                        || event.getY() < 0 || event.getY() >= view.getHeight()) stopRepeat();
                    return true;
                case MotionEvent.ACTION_UP:
                    boolean click = repeatingKey == button && !repeated;
                    stopRepeat();
                    if (click) view.performClick();
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    stopRepeat();
                    return true;
                default: return true;
            }
        });
    }

    void stopRepeat()
    {
        removeCallbacks(repeat);
        if (repeatingKey != null) repeatingKey.setPressed(false);
        repeatingKey = null;
    }

    @Override protected void onDetachedFromWindow()
    {
        stopRepeat();
        super.onDetachedFromWindow();
    }

    int getMode() { return mode; }
    boolean isUppercase() { return uppercase; }

    void setMode(int mode, boolean uppercase)
    {
        stopRepeat();
        this.mode = Math.max(0, Math.min(2, mode));
        this.uppercase = uppercase;
        for (int i = 0; i < tabs.length; ++i)
        {
            style(tabs[i], i == this.mode);
            tabs[i].setSelected(i == this.mode);
        }
        keys.removeAllViews();
        if (landscape && this.mode == 0)
        {
            row("(", ")", "7", "8", "9", "+", "−", "BS");
            row(",", ";", "4", "5", "6", ".", "×", "=");
            row("AC", "ans", "1", "2", "3", "0", "÷", "実行");
        }
        else if (landscape && this.mode == 2)
        {
            row("(", ")", "{", "}", "[", "]", ",", "BS");
            row("=", "===", "!==", "<", "<=", ">", ">=", ";");
            row("!", "&&", "||", "%", "**", "?", ":", "実行");
        }
        else if (this.mode == 0)
        {
            row("(", ")", "ans", "⌫");
            row("7", "8", "9", "÷");
            row("4", "5", "6", "×");
            row("1", "2", "3", "−");
            row("0", ".", ",", "+");
            row(";", "=", "AC", "実行");
        }
        else if (this.mode == 1)
        {
            letters("qwertyuiop", false);
            letters("asdfghjkl", true);
            String letters = uppercase ? "ZXCVBNM" : "zxcvbnm";
            String[] third = new String[9];
            third[0] = "⇧";
            for (int i = 0; i < letters.length(); ++i) third[i + 1] = letters.substring(i, i + 1);
            third[8] = "⌫";
            row(third);
            row("_", "(", ")", ",", ".", ";");
            row("数値", "スペース", "実行");
        }
        else
        {
            row("(", ")", "{", "}");
            row("[", "]", ",", ";");
            row("=", "===", "!==", "⌫");
            row("<", ">", "<=", ">=");
            row("!", "&&", "||", "%");
            row("?", ":", "**", "実行");
        }
    }

    private void letters(String text, boolean inset)
    {
        String letters = uppercase ? text.toUpperCase(Locale.ROOT) : text;
        String[] labels = new String[letters.length()];
        for (int i = 0; i < letters.length(); ++i) labels[i] = letters.substring(i, i + 1);
        LinearLayout row = row(labels);
        if (inset)
        {
            View left = new View(getContext());
            row.addView(left, 0, new LayoutParams(0, 1, 0.5f));
            row.addView(new View(getContext()), new LayoutParams(0, 1, 0.5f));
        }
    }

    private LinearLayout row(String... labels)
    {
        LinearLayout row = new LinearLayout(getContext());
        row.setBaselineAligned(false);
        for (String label : labels)
        {
            Button button = key(label, () -> press(label));
            if (label.equals("⇧"))
            {
                style(button, uppercase);
                button.setContentDescription(uppercase ? "小文字に切替" : "大文字に切替");
            }
            row.addView(button, keyParameters(label.equals("スペース") ? 2 : 1));
        }
        keys.addView(row, new LayoutParams(-1, 0, 1));
        return row;
    }

    private void press(String label)
    {
        switch (label)
        {
            case "⌫":
            case "BS": deleteBackward(); break;
            case "AC": input.getText().clear(); break;
            case "実行": execute.run(); break;
            case "数値": setMode(0, uppercase); break;
            case "⇧": setMode(1, !uppercase); break;
            default:
                String value = label.equals("×") ? "*" : label.equals("÷") ? "/"
                    : label.equals("−") ? "-" : label.equals("スペース") ? " " : label;
                int start = Math.max(0, Math.min(input.getSelectionStart(), input.getSelectionEnd()));
                int end = Math.max(0, Math.max(input.getSelectionStart(), input.getSelectionEnd()));
                input.getText().replace(start, end, value);
                input.setSelection(start + value.length());
                input.requestFocus();
                break;
        }
    }

    private void deleteBackward()
    {
        int start = Math.max(0, Math.min(input.getSelectionStart(), input.getSelectionEnd()));
        int end = Math.max(0, Math.max(input.getSelectionStart(), input.getSelectionEnd()));
        if (start == end && start > 0) start = Character.offsetByCodePoints(input.getText(), start, -1);
        input.getText().delete(start, end);
        input.setSelection(start);
    }

    private void move(int direction)
    {
        int start = Math.max(0, Math.min(input.getSelectionStart(), input.getSelectionEnd()));
        int end = Math.max(0, Math.max(input.getSelectionStart(), input.getSelectionEnd()));
        int position = direction < 0 ? start : end;
        if (start == end && ((direction < 0 && position > 0) || (direction > 0 && position < input.length())))
            position = Character.offsetByCodePoints(input.getText(), position, direction);
        input.setSelection(position);
        input.requestFocus();
    }

    void setKeysEnabled(boolean enabled)
    {
        stopRepeat();
        keysEnabled = enabled;
        enableChildren(this, enabled);
        setAlpha(enabled ? 1 : 0.45f);
    }

    private void enableChildren(LinearLayout parent, boolean enabled)
    {
        for (int i = 0; i < parent.getChildCount(); ++i)
        {
            View child = parent.getChildAt(i);
            child.setEnabled(enabled);
            if (child instanceof LinearLayout) enableChildren((LinearLayout)child, enabled);
        }
    }
}
