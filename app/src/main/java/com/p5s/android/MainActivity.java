package com.p5s.android;

import android.app.Activity;
import android.content.res.AssetManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.database.Cursor;
import android.provider.OpenableColumns;
import android.widget.Button;
import android.widget.Toast;
import java.util.Locale;
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

    private static final int PICK_GAME_FILE = 41;
    private static final String PREFS = "p5s_launcher";
    private TextView surfaceResult;
    private TextView selectedGameStatus;
    private SharedPreferences settings;
    private volatile Thread renderThread;
    private volatile boolean surfaceAvailable = false;
    private volatile Surface activeSurface;
    private int lastSurfaceWidth = -1;
    private int lastSurfaceHeight = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        settings = getSharedPreferences(PREFS, MODE_PRIVATE);

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
        subtitle.setText("Etapa 18 — texturas independentes por material");
        subtitle.setTextSize(16);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 12, 0, 20);

        TextView launcherTitle = new TextView(this);
        launcherTitle.setText("Preparação do jogo");
        launcherTitle.setTextSize(18);
        launcherTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        launcherTitle.setPadding(0, 18, 0, 8);

        TextView launcherInfo = new TextView(this);
        launcherInfo.setText("Selecione uma cópia local de Persona 5 Strikers em formato NSP ou XCI. O aplicativo guardará a permissão de leitura do arquivo, sem copiá-lo nem enviá-lo pela internet.");
        launcherInfo.setTextSize(13);
        launcherInfo.setPadding(0, 4, 0, 10);

        Button selectGame = new Button(this);
        selectGame.setText("Selecionar arquivo NSP / XCI");
        selectGame.setOnClickListener(v -> openGamePicker());

        selectedGameStatus = new TextView(this);
        selectedGameStatus.setTextSize(13);
        selectedGameStatus.setPadding(0, 8, 0, 12);

        TextView preparationStatus = new TextView(this);
        preparationStatus.setText("Estado: importador NSP/XCI e runtime ainda não implementados. A seleção não instala, descriptografa nem executa o jogo.");
        preparationStatus.setTextSize(12);
        preparationStatus.setPadding(0, 4, 0, 14);

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
        content.addView(launcherTitle);
        content.addView(launcherInfo);
        content.addView(selectGame);
        content.addView(selectedGameStatus);
        content.addView(preparationStatus);
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
        notice.setText("Teste experimental de renderização Vulkan. O cristal 3D é carregado de OBJ com coordenadas UV, materiais MTL e texturas difusas independentes com imagens e descritores Vulkan próprios; ainda não é o jogo Persona 5 Strikers.");
        notice.setTextSize(12);
        notice.setPadding(0, 16, 0, 0);
        content.addView(notice);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        setContentView(scroll);
        refreshSelectedGame();
    }

    private void openGamePicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, PICK_GAME_FILE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_GAME_FILE || resultCode != RESULT_OK
                || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        String name = queryDisplayName(uri);
        String lowerName = name.toLowerCase(Locale.ROOT);
        if (!lowerName.endsWith(".nsp") && !lowerName.endsWith(".xci")) {
            Toast.makeText(this, "Formato não reconhecido. Escolha um arquivo .NSP ou .XCI.",
                    Toast.LENGTH_LONG).show();
            return;
        }
        try {
            getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            settings.edit().putString("game_uri", uri.toString())
                    .putString("game_name", name).apply();
            refreshSelectedGame();
            Toast.makeText(this, "Arquivo referenciado com sucesso.", Toast.LENGTH_SHORT).show();
        } catch (SecurityException e) {
            Toast.makeText(this, "O Android não permitiu manter o acesso ao arquivo. Selecione-o novamente.",
                    Toast.LENGTH_LONG).show();
        }
    }

    private String queryDisplayName(Uri uri) {
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri,
                    new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) return cursor.getString(index);
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        String segment = uri.getLastPathSegment();
        return segment == null ? "arquivo selecionado" : segment;
    }

    private void refreshSelectedGame() {
        if (selectedGameStatus == null) return;
        String name = settings.getString("game_name", null);
        String uriText = settings.getString("game_uri", null);
        if (name == null || uriText == null) {
            selectedGameStatus.setText("Nenhum arquivo de jogo selecionado.");
            return;
        }
        boolean readable = false;
        try (java.io.InputStream stream =
                     getContentResolver().openInputStream(Uri.parse(uriText))) {
            readable = stream != null;
        } catch (Exception ignored) {
        }
        selectedGameStatus.setText(readable
                ? "Arquivo selecionado: " + name + "\nAcesso local: disponível."
                : "Arquivo selecionado: " + name + "\nAcesso local: indisponível. Selecione-o novamente.");
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
        surfaceResult.setText("Renderização Vulkan contínua em execução...\nA GPU está apresentando a malha 3D em uma thread dedicada.");
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
