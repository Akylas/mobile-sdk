/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINDEPTHWORKER_H_
#define _CARTO_TERRAINDEPTHWORKER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cglib/mat.h>

namespace carto {

    /**
     * One read-back of the packed terrain depth (RGB = linear eye depth relative to the far
     * plane, A = terrain coverage), at BUFFER_DOWNSCALE resolution. Immutable once published,
     * so it can be handed to the label placement worker as a whole.
     */
    struct TerrainDepthBuffer {
        std::vector<std::uint8_t> data;
        int width = 0;
        int height = 0;
        float far = 0;
        // The camera this was rendered from. An occlusion query must project with THIS matrix,
        // not the current frame's: the buffer lags a moving camera by up to the submit interval,
        // and comparing a current-camera distance against it inverts the answer while zooming.
        cglib::mat4x4<double> mvpMatrix = cglib::mat4x4<double>::zero();
    };

    /**
     * Renders the terrain occlusion depth buffer and reads it back on a thread of its own,
     * with its own GL context.
     *
     * The read-back is a glReadPixels, i.e. a full pipeline stall - measured at 55-62 ms on an
     * Adreno 610. On the render thread that stall IS the frame, which is why the synchronous
     * path can only afford it every few hundred ms while the camera moves. Here it happens on
     * a thread whose stalling costs nothing, and the render thread only pays for collecting
     * the meshes to draw.
     *
     * The context needs nothing from the render context and is deliberately NOT shared with it:
     * the depth pass draws CPU-built meshes from client memory with its own program and its own
     * framebuffer, so there are no cross-context object lifetime or flush-ordering rules to get
     * right. Meshes are held alive through the job for as long as the worker needs them.
     *
     * EGL-only, so it is active on Android (and any ANGLE-backed build); elsewhere isSupported()
     * is false and the caller keeps the synchronous path.
     *
     * Internal class, not exposed in the public API.
     */
    class TerrainDepthWorker {
    public:
        struct DrawItem {
            cglib::mat4x4<float> mvpMat;
            std::shared_ptr<const void> owner; // keeps the mesh data alive for as long as the job runs
            const float* vertices = nullptr;
            const std::uint16_t* indices = nullptr;
            std::size_t indexCount = 0;
        };

        struct Job {
            int width = 0;
            int height = 0;
            float far = 0;
            cglib::mat4x4<double> mvpMatrix = cglib::mat4x4<double>::zero(); // carried into the result
            std::vector<DrawItem> items;
        };

        using Result = TerrainDepthBuffer;

        TerrainDepthWorker(std::string vertexShaderSource, std::string fragmentShaderSource);
        virtual ~TerrainDepthWorker();

        /**
         * False when this build has no offscreen GL context to render on. The caller must then
         * fall back to rendering and reading back on the render thread.
         */
        static bool isSupported();

        /**
         * Minimum interval (ms) between jobs while the camera moves, so the two contexts do not
         * contend on every frame. Overridable for measurement with
         * 'adb shell setprop debug.carto.asyncdepthms N'.
         */
        static int getMovingSubmitInterval(int defaultInterval);

        /**
         * False once the offscreen context turned out not to work (it is created on the worker
         * thread, so this only settles after the first job was offered). The caller must then
         * go back to the synchronous path rather than wait for results that never come.
         */
        bool isUsable() const;

        /**
         * True while a job is being rendered or read back. Submitting is pointless until it
         * clears - a newer camera would only queue up behind an already stale one.
         */
        bool isBusy() const;

        /**
         * Hands a job over. Never blocks and never touches the render context. Returns false
         * when the worker is busy or unusable, in which case nothing was taken over.
         */
        bool submit(Job job);

        /**
         * The most recently finished result, or null when nothing finished since the last call.
         */
        std::shared_ptr<const Result> takeResult();

    private:
        void threadLoop();
        bool initContext();
        void destroyContext();
        bool initFrameBuffer(int width, int height);
        bool initProgram();
        std::shared_ptr<Result> renderJob(const Job& job);

        const std::string _vertexShaderSource;
        const std::string _fragmentShaderSource;

        // EGL/GL handles, kept as opaque types so the header does not drag in the GL headers.
        // Touched only from the worker thread.
        void* _display = nullptr;
        void* _context = nullptr;
        void* _surface = nullptr;
        unsigned int _frameBufferId = 0;
        unsigned int _colorTextureId = 0;
        unsigned int _depthBufferId = 0;
        unsigned int _programId = 0;
        int _aCoord = -1;
        int _uMVPMat = -1;
        int _uFar = -1;
        int _frameBufferWidth = 0;
        int _frameBufferHeight = 0;

        mutable std::mutex _mutex;
        std::condition_variable _condition;
        std::thread _thread;
        std::unique_ptr<Job> _pendingJob;
        std::shared_ptr<const Result> _result;
        std::atomic<bool> _busy = { false };
        std::atomic<bool> _unusable = { false };
        bool _stopped = false;
    };

}

#endif
