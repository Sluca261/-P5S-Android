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
import java.io.InputStream;
import android.os.Bundle;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.Surface;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Color;
import android.view.View;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.ArrayList;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("native-lib");
    }

    private native String getVulkanInfo();
    private native String renderVulkanLoop(Surface surface, AssetManager assets);
    private native void prepareVulkanRendering();
    private native void requestStopVulkanRendering();
    private native void updateInputState(long buttons, long pressed, long released, long sequence, float leftX, float leftY, float rightX, float rightY);

    private final GameInputState gameInput = new GameInputState();

    private static final int PICK_GAME_FILE = 41;
    private static final int PICK_KEYS_FILE = 42;
    private static final int PICK_TITLE_KEYS_FILE = 43;
    private Uri selectedKeysUri;
    private Uri selectedTitleKeysUri;
    private static final String PREFS = "p5s_launcher";
    private TextView surfaceResult;
    private TextView selectedGameStatus;
    private TextView inputStatus;
    private SharedPreferences settings;
    private final ArrayList<Button> touchButtons = new ArrayList<>();
    private float controlScale = 1.0f;
    private float controlOpacity = 1.0f;
    private boolean controlEditMode = false;
    private volatile Thread renderThread;
    private volatile boolean surfaceAvailable = false;
    private volatile Surface activeSurface;
    private int lastSurfaceWidth = -1;
    private int lastSurfaceHeight = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        settings = getSharedPreferences(PREFS, MODE_PRIVATE);
        String savedKeysUri = settings.getString("keys_uri", null);
        String savedTitleKeysUri = settings.getString("title_keys_uri", null);
        if (savedKeysUri != null) {
            try {
                selectedKeysUri = Uri.parse(savedKeysUri);
                getContentResolver().takePersistableUriPermission(selectedKeysUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception ignored) {
                selectedKeysUri = null;
            }
        }
        if (savedTitleKeysUri != null) {
            try {
                selectedTitleKeysUri = Uri.parse(savedTitleKeysUri);
                getContentResolver().takePersistableUriPermission(selectedTitleKeysUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception ignored) {
                selectedTitleKeysUri = null;
            }
        }
        controlScale = settings.getFloat("control_scale", 1.0f);
        controlOpacity = settings.getFloat("control_opacity", 1.0f);

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
        subtitle.setText("Etapa 26 — personalização de tamanho e opacidade dos controles");
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

        Button selectKeys = new Button(this);
        selectKeys.setText("Selecionar prod.keys");
        selectKeys.setOnClickListener(v -> openKeysPicker());

        Button selectTitleKeys = new Button(this);
        selectTitleKeys.setText("Selecionar title.keys (opcional)");
        selectTitleKeys.setOnClickListener(v -> openTitleKeysPicker());

        Button launchGame = new Button(this);
        launchGame.setText("Iniciar Persona 5 Strikers");
        launchGame.setOnClickListener(v -> launchNativeRuntime());



        selectedGameStatus = new TextView(this);
        selectedGameStatus.setTextSize(13);
        selectedGameStatus.setPadding(0, 8, 0, 12);

        TextView preparationStatus = new TextView(this);
        preparationStatus.setText("O runtime ARM64 está integrado em fase inicial. A compatibilidade de inicialização, shaders e serviços específicos de Persona 5 Strikers ainda está em desenvolvimento.");
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
        content.addView(selectKeys);
        content.addView(selectTitleKeys);
        content.addView(launchGame);
        content.addView(selectedGameStatus);

        TextView controlsTitle = new TextView(this);
        controlsTitle.setText("Controles de teste");
        controlsTitle.setTextSize(18);
        controlsTitle.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        controlsTitle.setPadding(0, 18, 0, 8);
        content.addView(controlsTitle);

        LinearLayout editorRow = new LinearLayout(this);
        editorRow.setGravity(Gravity.CENTER);
        editorRow.setOrientation(LinearLayout.HORIZONTAL);
        editorRow.addView(makeEditorButton("A−", -0.1f, 0f));
        editorRow.addView(makeEditorButton("A+", 0.1f, 0f));
        editorRow.addView(makeEditorButton("Opacidade −", 0f, -0.1f));
        editorRow.addView(makeEditorButton("Opacidade +", 0f, 0.1f));
        Button moveModeButton = new Button(this);
        moveModeButton.setText("Mover controles");
        moveModeButton.setTextSize(10);
        moveModeButton.setOnClickListener(v -> {
            controlEditMode = !controlEditMode;
            moveModeButton.setText(controlEditMode ? "Concluir posição" : "Mover controles");
            Toast.makeText(this, controlEditMode ? "Arraste os botões para reposicioná-los." : "Posições salvas.", Toast.LENGTH_SHORT).show();
        });
        editorRow.addView(moveModeButton);
        content.addView(editorRow);

        TextView controlsInfo = new TextView(this);
        controlsInfo.setText("Toque ou controle físico alimentam o mesmo estado de entrada: botões, direções e quatro eixos normalizados. Os analógicos e os botões direcionais e superiores já influenciam o modelo de diagnóstico Vulkan. A e B alteram o sentido da rotação; L e R giram a orientação; o direcional desloca o modelo. A camada também registra eventos distintos de pressionar e soltar, com sequência crescente, incluindo os cliques L3 e R3.");
        controlsInfo.setTextSize(12);
        controlsInfo.setPadding(0, 0, 0, 8);
        content.addView(controlsInfo);

        inputStatus = new TextView(this);
        inputStatus.setText("Entrada Vulkan: aguardando comandos");
        inputStatus.setTextSize(13);
        inputStatus.setPadding(0, 8, 0, 8);
        content.addView(inputStatus);

        LinearLayout shoulderRow = new LinearLayout(this);
        shoulderRow.setGravity(Gravity.CENTER);
        shoulderRow.setOrientation(LinearLayout.HORIZONTAL);
        shoulderRow.addView(makeTouchButton("L", "L"));
        shoulderRow.addView(makeTouchButton("ZL", "ZL"));
        shoulderRow.addView(makeTouchButton("−", "SELECT"));
        shoulderRow.addView(makeTouchButton("+", "START"));
        shoulderRow.addView(makeTouchButton("ZR", "ZR"));
        shoulderRow.addView(makeTouchButton("R", "R"));
        content.addView(shoulderRow);

        LinearLayout stickClickRow = new LinearLayout(this);
        stickClickRow.setGravity(Gravity.CENTER);
        stickClickRow.setOrientation(LinearLayout.HORIZONTAL);
        stickClickRow.addView(makeTouchButton("L3", "L3"));
        stickClickRow.addView(makeTouchButton("R3", "R3"));
        content.addView(stickClickRow);

        LinearLayout dpadRow = new LinearLayout(this);
        dpadRow.setGravity(Gravity.CENTER);
        dpadRow.setOrientation(LinearLayout.HORIZONTAL);
        dpadRow.addView(makeTouchButton("↑", "UP"));
        dpadRow.addView(makeTouchButton("←", "LEFT"));
        dpadRow.addView(makeTouchButton("↓", "DOWN"));
        dpadRow.addView(makeTouchButton("→", "RIGHT"));
        content.addView(dpadRow);

        LinearLayout actionRow = new LinearLayout(this);
        actionRow.setGravity(Gravity.CENTER);
        actionRow.setOrientation(LinearLayout.HORIZONTAL);
        actionRow.addView(makeTouchButton("Y", "Y"));
        actionRow.addView(makeTouchButton("X", "X"));
        actionRow.addView(makeTouchButton("B", "B"));
        actionRow.addView(makeTouchButton("A", "A"));
        content.addView(actionRow);

        LinearLayout analogRow = new LinearLayout(this);
        analogRow.setGravity(Gravity.CENTER);
        analogRow.setOrientation(LinearLayout.HORIZONTAL);
        analogRow.addView(new InputPadView("ANALÓGICO ESQUERDO"),
                new LinearLayout.LayoutParams(0, dpToPx(150), 1));
        analogRow.addView(new InputPadView("CÂMERA"),
                new LinearLayout.LayoutParams(0, dpToPx(150), 1));
        content.addView(analogRow);
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

    private void openKeysPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, PICK_KEYS_FILE);
    }

    private void openTitleKeysPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, PICK_TITLE_KEYS_FILE);
    }

    private native boolean configureRuntime(int gameFd, String gameName, String keysPath, String titleKeysPath);

    private void launchNativeRuntime() {
        String gameUriText = settings.getString("game_uri", null);
        if (gameUriText == null || selectedKeysUri == null) {
            Toast.makeText(this, "Selecione o NSP/XCI e o arquivo prod.keys.", Toast.LENGTH_LONG).show();
            return;
        }
        android.os.ParcelFileDescriptor pfd = null;
        try {
            Uri gameUri = Uri.parse(gameUriText);
            pfd = getContentResolver().openFileDescriptor(gameUri, "r");
            if (pfd == null) throw new IllegalStateException("Não foi possível abrir o jogo.");
            java.io.File keysCopy = new java.io.File(getFilesDir(), "prod.keys");
            try (InputStream in = getContentResolver().openInputStream(selectedKeysUri);
                 java.io.FileOutputStream out = new java.io.FileOutputStream(keysCopy)) {
                if (in == null) throw new IllegalStateException("Não foi possível ler prod.keys.");
                byte[] buffer = new byte[8192];
                int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                out.getFD().sync();
            }
            // Transfer ownership of the game descriptor only after every other preparation
            // step succeeds. Until detachFd(), the ParcelFileDescriptor closes it on failure.
            int fd = pfd.detachFd();
            String titleKeysPath = null;
            if (selectedTitleKeysUri != null) {
                java.io.File titleKeysCopy = new java.io.File(getFilesDir(), "title.keys");
                try (InputStream in = getContentResolver().openInputStream(selectedTitleKeysUri);
                     java.io.FileOutputStream out = new java.io.FileOutputStream(titleKeysCopy)) {
                    if (in == null) throw new IllegalStateException("Não foi possível ler title.keys.");
                    byte[] buffer = new byte[8192];
                    int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                    out.getFD().sync();
                }
                titleKeysPath = titleKeysCopy.getAbsolutePath();
            }
            if (!configureRuntime(fd, settings.getString("game_name", "game.nsp"), keysCopy.getAbsolutePath(), titleKeysPath)) {
                throw new IllegalStateException("O descritor do jogo ou o acesso a prod.keys não pôde ser validado pelo runtime.");
            }
            startActivity(new Intent(this, android.app.NativeActivity.class));
        } catch (Exception e) {
            if (pfd != null) try { pfd.close(); } catch (Exception ignored) {}
            Toast.makeText(this, "Falha ao preparar o runtime: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode == PICK_TITLE_KEYS_FILE) {
            Uri titleKeysUri = data.getData();
            if (!"title.keys".equalsIgnoreCase(queryDisplayName(titleKeysUri))) {
                Toast.makeText(this, "O arquivo precisa se chamar title.keys.", Toast.LENGTH_LONG).show();
                return;
            }
            try {
                getContentResolver().takePersistableUriPermission(titleKeysUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                selectedTitleKeysUri = titleKeysUri;
                settings.edit().putString("title_keys_uri", titleKeysUri.toString()).apply();
                Toast.makeText(this, "title.keys opcional selecionado.", Toast.LENGTH_SHORT).show();
            } catch (SecurityException e) {
                Toast.makeText(this, "Não foi possível manter o acesso ao arquivo.", Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (requestCode == PICK_KEYS_FILE) {
            Uri keysUri = data.getData();
            if (!"prod.keys".equalsIgnoreCase(queryDisplayName(keysUri))) {
                Toast.makeText(this, "O arquivo precisa se chamar prod.keys.", Toast.LENGTH_LONG).show();
                return;
            }
            try {
                getContentResolver().takePersistableUriPermission(keysUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                selectedKeysUri = keysUri;
                settings.edit().putString("keys_uri", keysUri.toString()).apply();
                Toast.makeText(this, "prod.keys selecionado e salvo para os próximos usos.", Toast.LENGTH_SHORT).show();
            } catch (SecurityException e) {
                Toast.makeText(this, "Não foi possível manter o acesso ao arquivo.", Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (requestCode != PICK_GAME_FILE) return;
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
        Uri uri = Uri.parse(uriText);
        boolean permissionAvailable = false;
        for (android.content.UriPermission permission :
                getContentResolver().getPersistedUriPermissions()) {
            if (permission.getUri().equals(uri) && permission.isReadPermission()) {
                permissionAvailable = true;
                break;
            }
        }
        long sizeBytes = queryDisplaySize(uri);
        String mimeType = getContentResolver().getType(uri);
        String sizeLabel = sizeBytes >= 0 ? formatFileSize(sizeBytes) : "não informado";
        String mimeLabel = mimeType == null ? "não informado" : mimeType;
        String containerStatus = validateContainerHeader(uri, name, sizeBytes);
        String structureStatus = inspectContainerMetadata(uri, name, sizeBytes);
        selectedGameStatus.setText("Arquivo selecionado: " + name
                + "\nPermissão de leitura persistida: "
                + (permissionAvailable ? "disponível" : "não confirmada")
                + "\nTamanho informado pelo provedor: " + sizeLabel
                + "\nTipo MIME informado: " + mimeLabel
                + "\nValidação básica do cabeçalho: " + containerStatus
                + "\nEstrutura local: " + structureStatus);
    }

    // Lê somente os quatro bytes de identificação do formato; não descriptografa,
    // interpreta conteúdo interno, acessa chaves ou modifica o arquivo.
    private String validateContainerHeader(Uri uri, String name, long sizeBytes) {
        String lowerName = name.toLowerCase(Locale.ROOT);
        boolean isXci = lowerName.endsWith(".xci");
        long headerOffset = isXci ? 0x100L : 0L;
        String expected = isXci ? "HEAD" : "PFS0";

        if (!isXci && !lowerName.endsWith(".nsp")) {
            return "extensão não suportada";
        }
        if (sizeBytes >= 0 && sizeBytes < headerOffset + 4L) {
            return "arquivo pequeno demais para conter o cabeçalho esperado";
        }

        try (InputStream input = getContentResolver().openInputStream(uri)) {
            if (input == null) return "não foi possível abrir o arquivo para leitura local";
            long remaining = headerOffset;
            while (remaining > 0) {
                long skipped = input.skip(remaining);
                if (skipped <= 0) {
                    if (input.read() < 0) return "arquivo terminou antes do cabeçalho";
                    skipped = 1;
                }
                remaining -= skipped;
            }

            byte[] magic = new byte[4];
            int count = 0;
            while (count < magic.length) {
                int read = input.read(magic, count, magic.length - count);
                if (read < 0) break;
                count += read;
            }
            if (count != 4) return "cabeçalho incompleto";

            String actual = new String(magic, java.nio.charset.StandardCharsets.US_ASCII);
            if (expected.equals(actual)) {
                return "assinatura " + expected + " encontrada; cabeçalho compatível";
            }
            return "assinatura inesperada (" + actual + "); não reconhecido como "
                    + (isXci ? "XCI" : "NSP");
        } catch (Exception e) {
            return "não foi possível ler o cabeçalho: " + e.getClass().getSimpleName();
        }
    }

    // Inspeciona somente a tabela pública PFS0 de um NSP. Não abre NCA,
    // não descriptografa dados, não acessa chaves e não altera o arquivo.
    // Para XCI, limita-se à assinatura HEAD até existir um parser específico.
    private String inspectContainerMetadata(Uri uri, String name, long fileSize) {
        if (!name.toLowerCase(Locale.ROOT).endsWith(".nsp")) {
            return name.toLowerCase(Locale.ROOT).endsWith(".xci")
                    ? "XCI reconhecido; partições internas ainda não analisadas"
                    : "formato não suportado";
        }

        try (InputStream raw = getContentResolver().openInputStream(uri)) {
            if (raw == null) return "não foi possível abrir o arquivo";
            byte[] header = new byte[16];
            if (!readFully(raw, header)) return "cabeçalho PFS0 incompleto";
            if (header[0] != 'P' || header[1] != 'F'
                    || header[2] != 'S' || header[3] != '0') {
                return "sem tabela PFS0 válida";
            }

            long fileCount = uint32LE(header, 4);
            long stringTableSize = uint32LE(header, 8);
            if (fileCount == 0 || fileCount > 10000L
                    || stringTableSize == 0 || stringTableSize > 1024L * 1024L) {
                return "dimensões da tabela fora dos limites de segurança";
            }

            long entriesBytes = fileCount * 0x18L;
            long dataStart = 0x10L + entriesBytes + stringTableSize;
            if (dataStart < 0 || (fileSize >= 0 && dataStart > fileSize)) {
                return "tabela excede o tamanho informado do arquivo";
            }

            byte[] entries = new byte[(int) entriesBytes];
            if (!readFully(raw, entries)) return "tabela de entradas incompleta";
            byte[] strings = new byte[(int) stringTableSize];
            if (!readFully(raw, strings)) return "tabela de nomes incompleta";

            java.util.HashSet<String> names = new java.util.HashSet<>();
            java.util.ArrayList<long[]> ranges = new java.util.ArrayList<>();
            int ncaCount = 0;
            int cnmtCount = 0;
            StringBuilder sample = new StringBuilder();

            for (int i = 0; i < (int) fileCount; i++) {
                int p = i * 0x18;
                long offset = uint64LE(entries, p);
                long size = uint64LE(entries, p + 8);
                long nameOffset = uint32LE(entries, p + 16);
                if (offset < 0 || size <= 0 || nameOffset >= stringTableSize
                        || offset > Long.MAX_VALUE - dataStart
                        || size > Long.MAX_VALUE - dataStart - offset) {
                    return "entrada " + (i + 1) + " contém campos inválidos";
                }

                long absoluteStart = dataStart + offset;
                long absoluteEnd = absoluteStart + size;
                if (fileSize >= 0 && absoluteEnd > fileSize) {
                    return "entrada " + (i + 1) + " aponta além do arquivo";
                }

                int nameStart = (int) nameOffset;
                int nameEnd = nameStart;
                while (nameEnd < strings.length && strings[nameEnd] != 0) nameEnd++;
                if (nameEnd == strings.length || nameEnd == nameStart) {
                    return "entrada " + (i + 1) + " tem nome vazio ou sem terminador";
                }
                String entryName = new String(strings, nameStart, nameEnd - nameStart,
                        java.nio.charset.StandardCharsets.UTF_8);
                if (entryName.contains("/") || entryName.contains("\\\\")
                        || ".".equals(entryName) || "..".equals(entryName)
                        || !names.add(entryName)) {
                    return "entrada " + (i + 1) + " tem nome inválido ou duplicado";
                }

                String lowerEntryName = entryName.toLowerCase(Locale.ROOT);
                if (lowerEntryName.endsWith(".nca")) ncaCount++;
                if (lowerEntryName.endsWith(".cnmt.nca")) cnmtCount++;
                ranges.add(new long[]{absoluteStart, absoluteEnd});

                if (i < 6) {
                    if (sample.length() > 0) sample.append(" | ");
                    sample.append(entryName).append(" (").append(formatFileSize(size)).append(")");
                }
            }

            ranges.sort((a, b) -> Long.compare(a[0], b[0]));
            for (int i = 1; i < ranges.size(); i++) {
                if (ranges.get(i)[0] < ranges.get(i - 1)[1]) {
                    return "arquivos internos possuem regiões sobrepostas";
                }
            }

            return fileCount + " entrada(s) PFS0; " + ncaCount + " arquivo(s) NCA"
                    + " (" + cnmtCount + " CNMT NCA); amostra: " + sample;
        } catch (Exception e) {
            return "falha na leitura estrutural: " + e.getClass().getSimpleName();
        }
    }

    private boolean readFully(InputStream input, byte[] target) throws java.io.IOException {
        int count = 0;
        while (count < target.length) {
            int read = input.read(target, count, target.length - count);
            if (read < 0) return false;
            count += read;
        }
        return true;
    }

    private long uint32LE(byte[] data, int offset) {
        return ((long) data[offset] & 0xffL)
                | (((long) data[offset + 1] & 0xffL) << 8)
                | (((long) data[offset + 2] & 0xffL) << 16)
                | (((long) data[offset + 3] & 0xffL) << 24);
    }

    private long uint64LE(byte[] data, int offset) {
        long low = uint32LE(data, offset);
        long high = uint32LE(data, offset + 4);
        if ((high & 0x80000000L) != 0) return -1L;
        return (high << 32) | low;
    }

    // Consulta somente metadados fornecidos pelo Storage Access Framework.
    // Não abre InputStream nem lê bytes do NSP/XCI.
    private long queryDisplaySize(Uri uri) {
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri,
                    new String[]{OpenableColumns.SIZE}, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.SIZE);
                if (index >= 0 && !cursor.isNull(index)) return cursor.getLong(index);
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        return -1L;
    }

    private String formatFileSize(long bytes) {
        if (bytes < 0) return "não informado";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024L * 1024L)
            return String.format(Locale.ROOT, "%.1f KiB", bytes / 1024.0);
        if (bytes < 1024L * 1024L * 1024L)
            return String.format(Locale.ROOT, "%.2f MiB", bytes / (1024.0 * 1024.0));
        return String.format(Locale.ROOT, "%.2f GiB",
                bytes / (1024.0 * 1024.0 * 1024.0));
    }


    private Button makeTouchButton(String label, String command) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextSize(12);
        button.setMinWidth(dpToPx(48));
        touchButtons.add(button);
        applyControlStyle(button);
        String positionKey = "control_position_" + command;
        button.setTranslationX(settings.getFloat(positionKey + "_x", 0f));
        button.setTranslationY(settings.getFloat(positionKey + "_y", 0f));
        button.setPadding(dpToPx(4), 0, dpToPx(4), 0);
        final float[] dragStart = new float[2];
        final float[] initialTranslation = new float[2];
        button.setOnTouchListener((view, event) -> {
            if (controlEditMode) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    dragStart[0] = event.getRawX();
                    dragStart[1] = event.getRawY();
                    initialTranslation[0] = view.getTranslationX();
                    initialTranslation[1] = view.getTranslationY();
                    return true;
                }
                if (event.getAction() == MotionEvent.ACTION_MOVE) {
                    view.setTranslationX(initialTranslation[0] + event.getRawX() - dragStart[0]);
                    view.setTranslationY(initialTranslation[1] + event.getRawY() - dragStart[1]);
                    return true;
                }
                if (event.getAction() == MotionEvent.ACTION_UP || event.getAction() == MotionEvent.ACTION_CANCEL) {
                    String key = "control_position_" + command;
                    settings.edit().putFloat(key + "_x", view.getTranslationX())
                            .putFloat(key + "_y", view.getTranslationY()).apply();
                    return true;
                }
                return true;
            }
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                gameInput.setButton(command, true);
                reportInput("toque — " + gameInput.describe());
                view.setPressed(true);
                return true;
            }
            if (event.getAction() == MotionEvent.ACTION_UP
                    || event.getAction() == MotionEvent.ACTION_CANCEL) {
                gameInput.setButton(command, false);
                reportInput("toque — " + gameInput.describe());
                view.setPressed(false);
                return true;
            }
            return true;
        });
        return button;
    }

    private Button makeEditorButton(String label, float scaleDelta, float opacityDelta) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextSize(10);
        button.setOnClickListener(v -> {
            controlScale = Math.max(0.7f, Math.min(1.5f, controlScale + scaleDelta));
            controlOpacity = Math.max(0.3f, Math.min(1.0f, controlOpacity + opacityDelta));
            settings.edit().putFloat("control_scale", controlScale)
                    .putFloat("control_opacity", controlOpacity).apply();
            applyAllControlStyles();
        });
        return button;
    }

    private void applyControlStyle(Button button) {
        button.setScaleX(controlScale);
        button.setScaleY(controlScale);
        button.setAlpha(controlOpacity);
    }

    private void applyAllControlStyles() {
        for (Button button : touchButtons) applyControlStyle(button);
    }

    private String gamepadCommand(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A: return "A";
            case KeyEvent.KEYCODE_BUTTON_B: return "B";
            case KeyEvent.KEYCODE_BUTTON_X: return "X";
            case KeyEvent.KEYCODE_BUTTON_Y: return "Y";
            case KeyEvent.KEYCODE_BUTTON_L1: return "L";
            case KeyEvent.KEYCODE_BUTTON_R1: return "R";
            case KeyEvent.KEYCODE_BUTTON_L2: return "ZL";
            case KeyEvent.KEYCODE_BUTTON_R2: return "ZR";
            case KeyEvent.KEYCODE_BUTTON_START: return "START";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "SELECT";
            case KeyEvent.KEYCODE_BUTTON_THUMBL: return "L3";
            case KeyEvent.KEYCODE_BUTTON_THUMBR: return "R3";
            case KeyEvent.KEYCODE_DPAD_UP: return "UP";
            case KeyEvent.KEYCODE_DPAD_DOWN: return "DOWN";
            case KeyEvent.KEYCODE_DPAD_LEFT: return "LEFT";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "RIGHT";
            default: return null;
        }
    }

    private void reportInput(String message) {
        updateInputState(gameInput.getButtonMask(), gameInput.getPressedMask(),
                gameInput.getReleasedMask(), gameInput.getSequence(), gameInput.getLeftX(),
                gameInput.getLeftY(), gameInput.getRightX(), gameInput.getRightY());
        if (inputStatus != null) inputStatus.setText("Comandos nativos #" + gameInput.getSequence() + ": " + message);
        gameInput.clearEdgeEvents();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int source = event.getSource();
        boolean gamepad = (source & android.view.InputDevice.SOURCE_GAMEPAD)
                == android.view.InputDevice.SOURCE_GAMEPAD
                || (source & android.view.InputDevice.SOURCE_JOYSTICK)
                == android.view.InputDevice.SOURCE_JOYSTICK;
        if (gamepad && (event.getAction() == KeyEvent.ACTION_DOWN
                || event.getAction() == KeyEvent.ACTION_UP)) {
            String command = gamepadCommand(event.getKeyCode());
            if (command != null) {
                gameInput.setButton(command, event.getAction() == KeyEvent.ACTION_DOWN);
                reportInput("controle físico — " + gameInput.describe());
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & android.view.InputDevice.SOURCE_JOYSTICK)
                == android.view.InputDevice.SOURCE_JOYSTICK
                && event.getAction() == MotionEvent.ACTION_MOVE) {
            float rightX = event.getAxisValue(MotionEvent.AXIS_RX);
            float rightY = event.getAxisValue(MotionEvent.AXIS_RY);
            if (Math.abs(rightX) < 0.001f && Math.abs(rightY) < 0.001f) {
                rightX = event.getAxisValue(MotionEvent.AXIS_Z);
                rightY = event.getAxisValue(MotionEvent.AXIS_RZ);
            }
            gameInput.setAxes(
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y), rightX, rightY);
            reportInput("controle físico — " + gameInput.describe());
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    private class InputPadView extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final String label;
        private float x;
        private float y;
        private boolean active;

        InputPadView(String label) {
            super(MainActivity.this);
            this.label = label;
            setFocusable(true);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            float cx = getWidth() / 2f;
            float cy = getHeight() / 2f + dpToPx(8);
            float radius = Math.min(getWidth(), getHeight()) * 0.34f;
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(dpToPx(2));
            paint.setColor(Color.GRAY);
            canvas.drawCircle(cx, cy, radius, paint);
            canvas.drawLine(cx - radius, cy, cx + radius, cy, paint);
            canvas.drawLine(cx, cy - radius, cx, cy + radius, paint);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(active ? Color.rgb(80, 145, 220) : Color.DKGRAY);
            canvas.drawCircle(cx + x * radius, cy + y * radius, radius * 0.28f, paint);
            paint.setColor(Color.WHITE);
            paint.setTextSize(dpToPx(11));
            paint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText(label, cx, dpToPx(14), paint);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            float cx = getWidth() / 2f;
            float cy = getHeight() / 2f + dpToPx(8);
            float radius = Math.min(getWidth(), getHeight()) * 0.34f;
            if (event.getAction() == MotionEvent.ACTION_DOWN
                    || event.getAction() == MotionEvent.ACTION_MOVE) {
                x = Math.max(-1f, Math.min(1f, (event.getX() - cx) / radius));
                y = Math.max(-1f, Math.min(1f, (event.getY() - cy) / radius));
                active = true;
                if (label.startsWith("ANALÓGICO")) gameInput.setAxes(x, y, gameInput.getRightX(), gameInput.getRightY());
                else gameInput.setAxes(gameInput.getLeftX(), gameInput.getLeftY(), x, y);
                reportInput("toque — " + gameInput.describe());
                invalidate();
                return true;
            }
            if (event.getAction() == MotionEvent.ACTION_UP
                    || event.getAction() == MotionEvent.ACTION_CANCEL) {
                x = 0f;
                y = 0f;
                active = false;
                if (label.startsWith("ANALÓGICO")) gameInput.setAxes(0f, 0f, gameInput.getRightX(), gameInput.getRightY());
                else gameInput.setAxes(gameInput.getLeftX(), gameInput.getLeftY(), 0f, 0f);
                reportInput("toque — " + gameInput.describe());
                invalidate();
                return true;
            }
            return true;
        }
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
