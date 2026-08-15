package com.massifmaps.MassifDemo.demo;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.constraintlayout.widget.ConstraintLayout;

import java.util.Arrays;

/**
 * A live camera preview BEHIND the map, which is what makes a transparent map worth having: the
 * star sky drawn over what the camera sees is an augmented-reality sky.
 *
 * The arrangement is the standard one for a GL view over a camera, and it is the whole reason
 * MapView.setTranslucent also raises the z-order: a SurfaceView is composited BELOW the window, so
 * the only thing a translucent map can reveal is ANOTHER surface under it. This adds that surface -
 * a plain SurfaceView, added first so it stays at the bottom - and the map (media overlay) sits on
 * top of it.
 *
 * Camera2 with a single preview target; nothing is captured or stored.
 */
public final class DemoCameraPreview {

    private static final String TAG = "DemoCameraPreview";
    private static final int PERMISSION_REQUEST = 4711;

    private final Context context;
    private final ConstraintLayout root;

    private SurfaceView surfaceView;
    private CameraDevice camera;
    private CameraCaptureSession session;

    public DemoCameraPreview(Context context, ConstraintLayout root) {
        this.context = context;
        this.root = root;
    }

    public boolean isRunning() {
        return surfaceView != null;
    }

    /** Adds the preview under the map and starts it. Asks for the permission if it is missing. */
    public void start() {
        // The demo builds its map on a worker thread; a view hierarchy may only be touched on the
        // main one.
        if (android.os.Looper.myLooper() != android.os.Looper.getMainLooper()) {
            new android.os.Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
                public void run() { start(); }
            });
            return;
        }
        if (surfaceView != null) {
            return;
        }
        if (context.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            if (context instanceof Activity) {
                ((Activity) context).requestPermissions(new String[] { Manifest.permission.CAMERA }, PERMISSION_REQUEST);
            }
            Log.w(TAG, "no camera permission yet - grant it and switch the mode again");
            return;
        }

        surfaceView = new SurfaceView(context);
        ConstraintLayout.LayoutParams lp = new ConstraintLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        lp.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID;
        // Index 0: below every other view, and - what actually matters here - a plain surface,
        // which the map's media-overlay surface is composited on top of.
        root.addView(surfaceView, 0, lp);

        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            public void surfaceCreated(SurfaceHolder holder) {
                openCamera(holder);
            }
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            }
            public void surfaceDestroyed(SurfaceHolder holder) {
                closeCamera();
            }
        });
    }

    /** Stops the preview and takes the view back out of the layout. */
    public void stop() {
        closeCamera();
        if (surfaceView != null) {
            final View view = surfaceView;
            surfaceView = null;
            view.post(new Runnable() {
                public void run() {
                    root.removeView(view);
                }
            });
        }
    }

    private void openCamera(final SurfaceHolder holder) {
        CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        if (manager == null) {
            return;
        }
        try {
            String cameraId = null;
            for (String id : manager.getCameraIdList()) {
                Integer facing = manager.getCameraCharacteristics(id).get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    cameraId = id;
                    break;
                }
            }
            if (cameraId == null) {
                Log.w(TAG, "no back camera on this device");
                return;
            }
            manager.openCamera(cameraId, new CameraDevice.StateCallback() {
                public void onOpened(CameraDevice device) {
                    camera = device;
                    startPreview(holder);
                }
                public void onDisconnected(CameraDevice device) {
                    device.close();
                    camera = null;
                }
                public void onError(CameraDevice device, int error) {
                    Log.w(TAG, "camera error " + error);
                    device.close();
                    camera = null;
                }
            }, null);
        } catch (CameraAccessException e) {
            Log.w(TAG, "could not open the camera: " + e);
        } catch (SecurityException e) {
            Log.w(TAG, "camera permission was revoked: " + e);
        }
    }

    private void startPreview(SurfaceHolder holder) {
        try {
            final CaptureRequest.Builder request = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            request.addTarget(holder.getSurface());
            camera.createCaptureSession(Arrays.asList(holder.getSurface()), new CameraCaptureSession.StateCallback() {
                public void onConfigured(CameraCaptureSession configured) {
                    session = configured;
                    try {
                        session.setRepeatingRequest(request.build(), null, null);
                    } catch (CameraAccessException e) {
                        Log.w(TAG, "could not start the preview: " + e);
                    }
                }
                public void onConfigureFailed(CameraCaptureSession configured) {
                    Log.w(TAG, "camera session configuration failed");
                }
            }, null);
        } catch (CameraAccessException e) {
            Log.w(TAG, "could not build the preview request: " + e);
        }
    }

    private void closeCamera() {
        if (session != null) {
            session.close();
            session = null;
        }
        if (camera != null) {
            camera.close();
            camera = null;
        }
    }
}
