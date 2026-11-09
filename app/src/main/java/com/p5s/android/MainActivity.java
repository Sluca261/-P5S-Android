package com.p5s.android;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Typeface;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("native-lib");
    }

    private native String getVulkanInfo();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(28, 36, 28, 28);
        content.setGravity(Gravity.CENTER_HORIZONTAL);

        TextView title = new TextView(this);
        title.setText("P5S ANDROID");
        title.setTextSize(28);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);

        TextView subtitle = new TextView(this);
        subtitle.setText("Diagnóstico inicial de compatibilidade Vulkan");
        subtitle.setTextSize(16);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 12, 0, 28);

        TextView result = new TextView(this);
        result.setText(getVulkanInfo());
        result.setTextSize(15);
        result.setTextIsSelectable(true);
        result.setPadding(0, 12, 0, 12);

        TextView notice = new TextView(this);
        notice.setText("Aviso: este aplicativo é apenas um diagnóstico técnico. Não contém o jogo e não representa uma versão jogável.");
        notice.setTextSize(13);
        notice.setPadding(0, 28, 0, 0);

        content.addView(title);
        content.addView(subtitle);
        content.addView(result);
        content.addView(notice);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        setContentView(scroll);
    }
}
