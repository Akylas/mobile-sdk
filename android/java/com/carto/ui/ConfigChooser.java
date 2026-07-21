package com.carto.ui;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLDisplay;

import android.opengl.GLSurfaceView;

/**
 * EGL configuration chooser for MapView and TextureMapView.
 * It is not intended for public usage.
 * @hidden
 */
public class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

    private static final int EGL_OPENGL_ES2_BIT = 0x0004;
    private static final int EGL_OPENGL_ES3_BIT = 0x0040;

    private static int[][] ATTRIBUTE_TABLE = new int[][] {
        // 8-8-8-8-bit color, 8-bit stencil, 24-bit z buffer. Should work on most devices.
        new int[] { EGL10.EGL_RED_SIZE, 8, EGL10.EGL_GREEN_SIZE, 8, EGL10.EGL_BLUE_SIZE, 8, EGL10.EGL_ALPHA_SIZE, 8, EGL10.EGL_DEPTH_SIZE, 24, EGL10.EGL_STENCIL_SIZE, 8, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE },
        // 8-8-8-8-bit color, 8-bit stencil, 16-bit z buffer. Better than 5-6-5/16 bit, should also fix problems on some obscure devices.
        new int[] { EGL10.EGL_RED_SIZE, 8, EGL10.EGL_GREEN_SIZE, 8, EGL10.EGL_BLUE_SIZE, 8, EGL10.EGL_ALPHA_SIZE, 8, EGL10.EGL_DEPTH_SIZE, 16, EGL10.EGL_STENCIL_SIZE, 8, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE },
        // 5-6-5-bit color, 24-bit z buffer, unspecified stencil. Should work on most devices.
        new int[] { EGL10.EGL_RED_SIZE, 5, EGL10.EGL_GREEN_SIZE, 6, EGL10.EGL_BLUE_SIZE, 5, EGL10.EGL_DEPTH_SIZE, 24, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE },
        // 5-6-5-bit color, 16-bit z buffer, unspecified stencil. Fallback for original Tegra devices.
        new int[] { EGL10.EGL_RED_SIZE, 5, EGL10.EGL_GREEN_SIZE, 6, EGL10.EGL_BLUE_SIZE, 5, EGL10.EGL_DEPTH_SIZE, 16, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE },
        // 5-6-5-bit color, unspecified z/stencil buffer.
        new int[] { EGL10.EGL_RED_SIZE, 5, EGL10.EGL_GREEN_SIZE, 6, EGL10.EGL_BLUE_SIZE, 5, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_NONE },
    };

    private final int glesVersion;

    public ConfigChooser() {
        this(2);
    }

    public ConfigChooser(int glesVersion) {
        this.glesVersion = glesVersion;
    }

    @Override
    public EGLConfig chooseConfig(EGL10 egl, EGLDisplay eglDisplay) {
        com.carto.utils.Log.debug("ConfigChooser.chooseConfig: Model: " + android.os.Build.MODEL + ", board: " + android.os.Build.BOARD + ", product: " + android.os.Build.PRODUCT);

        // Prefer configurations matching the requested context version, then fall back to ES 2.0-renderable ones
        int[] renderableTypes = (glesVersion >= 3 ? new int[] { EGL_OPENGL_ES3_BIT, EGL_OPENGL_ES2_BIT } : new int[] { EGL_OPENGL_ES2_BIT });
        for (int renderableType : renderableTypes) {
            for (int i = 0; i < ATTRIBUTE_TABLE.length; i++) {
                int[] attributes = withRenderableType(ATTRIBUTE_TABLE[i], renderableType);
                int[] numConfigs = new int[] { 0 };
                EGLConfig[] configs = new EGLConfig[1];
                if (egl.eglChooseConfig(eglDisplay, attributes, configs, 1, numConfigs)) {
                    if (numConfigs[0] > 0) {
                        com.carto.utils.Log.debug("ConfigChooser.chooseConfig: Selected display configuration: " + i + " (renderable type " + renderableType + ")");
                        return configs[0];
                    }
                }
            }
        }
        throw new IllegalArgumentException("Failed to choose EGLConfig!");
    }

    private static int[] withRenderableType(int[] attributes, int renderableType) {
        int[] result = attributes.clone();
        for (int i = 0; i + 1 < result.length; i += 2) {
            if (result[i] == EGL10.EGL_RENDERABLE_TYPE) {
                result[i + 1] = renderableType;
            }
        }
        return result;
    }
}
