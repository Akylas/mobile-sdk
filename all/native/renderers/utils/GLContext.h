/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_GLCONTEXT_H_
#define _MASSIF_GLCONTEXT_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <mutex>
#include <string>
#include <unordered_set>
#include <atomic>

namespace massif {

    class GLContext {
    public:
        static bool TEXTURE_FILTER_ANISOTROPIC;
        static bool TEXTURE_NPOT_REPEAT;
        static bool TEXTURE_NPOT_MIPMAPS;

        static bool DISCARD_FRAMEBUFFER;

        static bool PACKED_DEPTH_STENCIL;

        // An ES 3.0 context. The shaders stay GLSL ES 1.00 - this says what the API offers, not
        // what the shading language does.
        static bool ES3;
        // A depth texture can be attached to a framebuffer and sampled. ES3 core, otherwise an
        // extension. Lets the shadow pass drop its packed-RGB colour target.
        static bool DEPTH_TEXTURE;
        // Hardware depth comparison from GLSL ES 1.00 (shadow2DEXT): one filtered fetch instead of
        // four taps and a manual compare. NOT implied by ES3 - the shading language decides.
        static bool SHADOW_SAMPLERS;

        static std::size_t MAX_VERTEXBUFFER_SIZE;
    
        static bool HasGLExtension(const char* extension);
    
        static void LoadExtensions();
        
        static void CheckGLError(const char* place);

        static void DiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum* attachments);
    
    private:
        GLContext();

#ifdef GL_EXT_discard_framebuffer
        static PFNGLDISCARDFRAMEBUFFEREXTPROC _DiscardFramebufferEXT;
#endif

        static std::unordered_set<std::string> _ExtensionCache;
    
        static std::recursive_mutex _Mutex;
    };
    
}

#endif
