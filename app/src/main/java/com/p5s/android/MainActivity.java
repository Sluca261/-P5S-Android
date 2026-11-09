package com.p5s.android;

import android.app.Activity;
import android.content.res.AssetManager;
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
    private native String renderVulkanLoop(Surface surface, AssetManager assets);
    private native void prepareVulkanRendering();
    private native void requestStopVulkanRendering();

    private TextView surfaceResult;
    private volatile Thread renderThread;
    private volatile boolean surfaceAvailable = false;
    private volatile Surface activeSurface;
    private int lastSurfaceWidth = -1;
    private int lastSurfaceHeight = -1;

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
        subtitle.setText("Etapa 12 — rotação 3D em tempo real");
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
        notice.setText("Teste experimental de renderização Vulkan. O cubo é uma geometria de diagnóstico com rotação contínua; ainda não é o jogo Persona 5 Strikers.");
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

    private void startRenderer(Surface surface) {
        if (!stopRenderer()) {
            surfaceResult.setText("Aguardando a thread Vulkan anterior encerrar com segurança...");
            return;
        }
        if (surface == null || !surface.isValid()) {
            surfaceResult.setText("Aguardando uma superfície Android válida...");
            return;
        }
        activeSurface = surface;
        surfaceResult.setText("Renderização Vulkan contínua em execução...\nA GPU está apresentando o triângulo em uma thread dedicada.");
        prepareVulkanRendering();
        Thread worker = new Thread(() -> {
            Thread current = Thread.currentThread();
            String result = renderVulkanLoop(surface, getAssets());
            runOnUiThread(() -> {
                if (renderThread == current) surfaceResult.setText(result);
            });
        }, "P5S-Vulkan-Render");
        renderThread = worker;
        worker.start();
    }

    private boolean stopRenderer() {
        Thread worker = renderThread;
        if (worker != null && worker.isAlive()) {
            requestStopVulkanRendering();
            try {
                worker.join(3000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        if (worker != null && worker.isAlive()) return false;
        if (renderThread == worker) renderThread = null;
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        surfaceAvailable = true;
        android.graphics.Rect frame = holder.getSurfaceFrame();
        lastSurfaceWidth = frame.width();
        lastSurfaceHeight = frame.height();
        startRenderer(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (width == lastSurfaceWidth && height == lastSurfaceHeight) return;
        lastSurfaceWidth = width;
        lastSurfaceHeight = height;
        if (surfaceAvailable) startRenderer(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceAvailable = false;
        activeSurface = null;
        stopRenderer();
        surfaceResult.setText("Superfície Android encerrada. Recursos Vulkan liberados.");
    }

    @Override
    protected void onPause() {
        stopRenderer();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (surfaceAvailable && activeSurface != null && activeSurface.isValid()) {
            startRenderer(activeSurface);
        }
    }
}
