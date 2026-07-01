// =============================================================================
//  shader.h  -  Carga, compila y enlaza programas GLSL (con geometry shader).
//
//  Soporta inyectar un "chunk comun" (p.ej. la funcion de ruido del terreno)
//  justo despues de la linea #version, para que el vertex shader de la superficie
//  y el del contorno usen EXACTAMENTE el mismo desplazamiento y no se desincronicen.
// =============================================================================
#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "mathx.h"

// Lee un archivo de texto completo a un std::string. Aborta con mensaje si falla.
inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "ERROR: no pude abrir el shader: " << path << "\n"; std::exit(1); }
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// Inserta 'chunk' inmediatamente despues de la primera linea (#version ...).
// GLSL exige que #version sea lo primero, por eso no se puede prepender sin mas.
inline std::string injectAfterVersion(const std::string& src, const std::string& chunk) {
    if (chunk.empty()) return src;
    size_t nl = src.find('\n');
    if (nl == std::string::npos) return src + "\n" + chunk;
    return src.substr(0, nl + 1) + chunk + "\n" + src.substr(nl + 1);
}

// Compila una etapa (VERTEX/GEOMETRY/FRAGMENT) y reporta errores de compilacion.
inline GLuint compileStage(GLenum type, const std::string& src, const char* tag) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log);
        std::cerr << "ERROR compilando " << tag << ":\n" << log << "\n";
        std::exit(1);
    }
    return s;
}

// Programa GLSL: maneja el id de OpenGL y expone setters de uniforms tipados.
struct Shader {
    GLuint id = 0;

    /**
     * Construye el programa desde archivos. El geometry shader es opcional
     * (pasar "" lo omite). 'common' se inyecta tras #version en TODAS las etapas
     * que se compilen (vertex/geometry/fragment).
     */
    void load(const std::string& vertPath, const std::string& fragPath,
              const std::string& geomPath = "", const std::string& common = "") {
        GLuint vs = compileStage(GL_VERTEX_SHADER,
                                 injectAfterVersion(readFile(vertPath), common), "vertex");
        GLuint fs = compileStage(GL_FRAGMENT_SHADER,
                                 injectAfterVersion(readFile(fragPath), common), "fragment");
        GLuint gs = 0;
        if (!geomPath.empty())
            gs = compileStage(GL_GEOMETRY_SHADER,
                              injectAfterVersion(readFile(geomPath), common), "geometry");

        id = glCreateProgram();
        glAttachShader(id, vs);
        glAttachShader(id, fs);
        if (gs) glAttachShader(id, gs);
        glLinkProgram(id);
        GLint ok = 0; glGetProgramiv(id, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048]; glGetProgramInfoLog(id, 2048, nullptr, log);
            std::cerr << "ERROR enlazando programa:\n" << log << "\n";
            std::exit(1);
        }
        // Los shaders ya estan dentro del programa enlazado; se pueden borrar.
        glDeleteShader(vs); glDeleteShader(fs); if (gs) glDeleteShader(gs);
    }

    void use() const { glUseProgram(id); }

    // --- Setters de uniforms (glGetUniformLocation se resuelve por nombre) ---
    void set(const char* n, float v) const { glUniform1f(glGetUniformLocation(id, n), v); }
    void set(const char* n, int v)   const { glUniform1i(glGetUniformLocation(id, n), v); }
    void set(const char* n, mx::vec3 v) const {
        glUniform3f(glGetUniformLocation(id, n), v.x, v.y, v.z);
    }
    void set(const char* n, const mx::mat4& m) const {
        // GL_FALSE: no transponer, porque mathx ya guarda column-major.
        glUniformMatrix4fv(glGetUniformLocation(id, n), 1, GL_FALSE, m.m);
    }
};
