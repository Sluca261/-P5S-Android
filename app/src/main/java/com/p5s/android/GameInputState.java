package com.p5s.android;

import java.util.Locale;

/**
 * Normalized input state shared by touch controls and physical gamepads.
 * This is an adapter layer only; no game runtime consumes it yet.
 */
public final class GameInputState {
    private static final String[] BUTTONS = {
            "A", "B", "X", "Y", "L", "R", "ZL", "ZR",
            "START", "SELECT", "UP", "DOWN", "LEFT", "RIGHT"
    };

    private long buttonMask;
    private float leftX;
    private float leftY;
    private float rightX;
    private float rightY;

    public synchronized void setButton(String name, boolean pressed) {
        if (name == null) return;
        for (int i = 0; i < BUTTONS.length; i++) {
            if (BUTTONS[i].equals(name)) {
                long bit = 1L << i;
                buttonMask = pressed ? buttonMask | bit : buttonMask & ~bit;
                return;
            }
        }
    }

    public synchronized boolean isPressed(String name) {
        if (name == null) return false;
        for (int i = 0; i < BUTTONS.length; i++) {
            if (BUTTONS[i].equals(name)) return (buttonMask & (1L << i)) != 0;
        }
        return false;
    }

    public synchronized long getButtonMask() {
        return buttonMask;
    }

    public synchronized void setAxes(float lx, float ly, float rx, float ry) {
        float[] left = normalizeStick(lx, ly);
        float[] right = normalizeStick(rx, ry);
        leftX = left[0];
        leftY = left[1];
        rightX = right[0];
        rightY = right[1];
    }

    private float[] normalizeStick(float x, float y) {
        x = clamp(x);
        y = clamp(y);
        float magnitude = (float) Math.sqrt(x * x + y * y);
        if (magnitude < 0.12f) return new float[]{0f, 0f};
        if (magnitude > 1f) {
            x /= magnitude;
            y /= magnitude;
        }
        return new float[]{x, y};
    }

    private float clamp(float value) {
        if (Float.isNaN(value) || Float.isInfinite(value)) return 0f;
        return Math.max(-1f, Math.min(1f, value));
    }

    public synchronized float getLeftX() { return leftX; }
    public synchronized float getLeftY() { return leftY; }
    public synchronized float getRightX() { return rightX; }
    public synchronized float getRightY() { return rightY; }

    public synchronized String describe() {
        StringBuilder pressed = new StringBuilder();
        for (int i = 0; i < BUTTONS.length; i++) {
            if ((buttonMask & (1L << i)) != 0) {
                if (pressed.length() > 0) pressed.append(',');
                pressed.append(BUTTONS[i]);
            }
        }
        if (pressed.length() == 0) pressed.append("nenhum botão");
        return String.format(Locale.ROOT,
                "botões [%s] | esquerdo %.2f, %.2f | câmera %.2f, %.2f",
                pressed, leftX, leftY, rightX, rightY);
    }
}
