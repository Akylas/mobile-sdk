#include "GLResourceManager.h"
#include "GLTexture.h"
#include "GLSubmesh.h"

#include <vector>

namespace carto { namespace nml {

    GLuint GLResourceManager::createProgram(const std::string& vertexShader, const std::string& fragmentShader, const std::set<std::string>& defs) {
        std::pair<std::pair<std::string, std::string>, std::set<std::string>> program{ { vertexShader, fragmentShader }, defs };
        auto it = _programMap.find(program);
        if (it != _programMap.end()) {
            return it->second;
        }

        GLuint vertexShaderId = 0, fragmentShaderId = 0, programId = 0;
        try {
            fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
            std::string fragmentShaderSourceStr = createShader(fragmentShader, defs);
            const char* fragmentShaderSource = fragmentShaderSourceStr.c_str();
            glShaderSource(fragmentShaderId, 1, const_cast<const char**>(&fragmentShaderSource), NULL);
            glCompileShader(fragmentShaderId);
            GLint isShaderCompiled = 0;
            glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &isShaderCompiled);
            if (!isShaderCompiled) {
                GLint infoLogLength = 0;
                glGetShaderiv(fragmentShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
                std::vector<char> infoLog(infoLogLength + 1);
                GLsizei charactersWritten = 0;
                glGetShaderInfoLog(fragmentShaderId, infoLogLength, &charactersWritten, infoLog.data());
                throw std::runtime_error(std::string(infoLog.begin(), infoLog.begin() + charactersWritten));
            }

            vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
            std::string vertexShaderSourceStr = createShader(vertexShader, defs);
            const char* vertexShaderSource = vertexShaderSourceStr.c_str();
            glShaderSource(vertexShaderId, 1, const_cast<const char**>(&vertexShaderSource), NULL);
            glCompileShader(vertexShaderId);
            glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &isShaderCompiled);
            if (!isShaderCompiled) {
                GLint infoLogLength = 0;
                glGetShaderiv(vertexShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
                std::vector<char> infoLog(infoLogLength + 1);
                GLsizei charactersWritten = 0;
                glGetShaderInfoLog(vertexShaderId, infoLogLength, &charactersWritten, infoLog.data());
                throw std::runtime_error(std::string(infoLog.begin(), infoLog.begin() + charactersWritten));
            }

            programId = glCreateProgram();
            glAttachShader(programId, fragmentShaderId);
            glAttachShader(programId, vertexShaderId);
            glLinkProgram(programId);
            GLint isLinked = 0;
            glGetProgramiv(programId, GL_LINK_STATUS, &isLinked);
            if (!isLinked) {
                GLint infoLogLength = 0;
                glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength);
                std::vector<char> infoLog(infoLogLength + 1);
                GLsizei charactersWritten = 0;
                glGetProgramInfoLog(programId, infoLogLength, &charactersWritten, infoLog.data());
                throw std::runtime_error(std::string(infoLog.begin(), infoLog.begin() + charactersWritten));
            }

            glDeleteShader(vertexShaderId);
            glDeleteShader(fragmentShaderId);
        }
        catch (...) {
            if (vertexShaderId != 0) {
                glDeleteShader(vertexShaderId);
            }
            if (fragmentShaderId != 0) {
                glDeleteShader(fragmentShaderId);
            }
            if (programId != 0) {
                glDeleteProgram(programId);
            }
            throw;
        }

        _programMap[program] = programId;
        return programId;
    }

    GLuint GLResourceManager::allocateTexture(const std::shared_ptr<GLTexture>& texture) {
        GLuint texId = 0;
        glGenTextures(1, &texId);

        _textureMap.emplace(texture, texId);
        return texId;
    }

    GLuint GLResourceManager::allocateVBO(const std::shared_ptr<GLSubmesh>& submesh) {
        GLuint vboId = 0;
        glGenBuffers(1, &vboId);

        _vboMap.emplace(submesh, vboId);
        return vboId;
    }

    void GLResourceManager::deleteUnused() {
        for (auto it = _textureMap.begin(); it != _textureMap.end(); ) {
            if (it->first.expired()) {
                GLuint texId = it->second;
                glDeleteTextures(1, &texId);
                it = _textureMap.erase(it);
            } else {
                it++;
            }
        }

        for (auto it = _vboMap.begin(); it != _vboMap.end(); it++) {
            if (it->first.expired()) {
                GLuint vboId = it->second;
                glDeleteBuffers(1, &vboId);
                it = _vboMap.erase(it);
            } else {
                it++;
            }
        }
    }

    void GLResourceManager::deleteAll() {
        for (auto it = _programMap.begin(); it != _programMap.end(); it++) {
            GLuint programId = it->second;
            glDeleteProgram(programId);
        }
        _programMap.clear();

        for (auto it = _textureMap.begin(); it != _textureMap.end(); it++) {
            GLuint texId = it->second;
            glDeleteTextures(1, &texId);
        }
        _textureMap.clear();

        for (auto it = _vboMap.begin(); it != _vboMap.end(); it++) {
            GLuint vboId = it->second;
            glDeleteBuffers(1, &vboId);
        }
        _vboMap.clear();
    }

    void GLResourceManager::resetAll() {
        _programMap.clear();
        _textureMap.clear();
        _vboMap.clear();
    }

    std::string GLResourceManager::createShader(const std::string& shader, const std::set<std::string>& defs) {
        std::string glslDefs;
        for (auto it2 = defs.begin(); it2 != defs.end(); it2++) {
            glslDefs += "#define " + *it2 + "\n";
        }

        return glslDefs + shader;
    }

} }
