package com.p5s.android;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("native-lib");
    }

    private native String getVulkanInfo();
    private native String renderVulkanFrame(Surface surface);

    private TextView surfaceResult;

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
        subtitle.setText("Etapa 3 — teste de apresentação gráfica Vulkan");
        subtitle.setTextSize(16);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 12, 0, 20);

        TextView result = new TextView(this);
        result.setText(getVulkanInfo());
        result.setTextSize(14);
        result.setTextIsSelectable(true);
        result.setPadding(0, 8, 0, 16);

        TextView surfaceTitle = new TextView(this);
        surfaceTitle.setText("Área de teste da tela");
        surfaceTitle.setTextSize(16);
        surfaceTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        surfaceTitle.setPadding(0, 8, 0, 8);

        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        content.addView(title);
        content.addView(subtitle);
        content.addView(result);
        content.addView(surfaceTitle);
        content.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dpToPx(220)));

        surfaceResult = new TextView(this);
        surfaceResult.setText("Aguardando criação da superfície Android...");
        surfaceResult.setTextSize(14);
        surfaceResult.setTextIsSelectable(true);
        surfaceResult.setPadding(0, 14, 0, 14);
        content.addView(surfaceResult);

        TextView notice = new TextView(this);
        notice.setText("Teste experimental de apresentação Vulkan. A cor exibida é apenas um quadro de diagnóstico; ainda não é o jogo Persona 5 Strikers.");
        notice.setTextSize(12);
        notice.setPadding(0, 16, 0, 0);
        content.addView(notice);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        setContentView(scroll);
    }

    private int dpToPx(int dp) {
        return Math.round(dp * getResources().getDisplayMetrics().density);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceResult.setText(renderVulkanFrame(holder.getSurface()));
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceResult.setText("Superfície Android encerrada.");
    }
}
