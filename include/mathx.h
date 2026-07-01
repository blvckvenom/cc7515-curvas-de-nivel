// =============================================================================
//  mathx.h  -  Mini-libreria de algebra lineal para graficos (reemplaza a GLM).
//
//  Solo lo necesario para el pipeline: vec3, mat4 (column-major, como espera
//  OpenGL) y las matrices de camara/proyeccion. Header-only e inline para que
//  el proyecto dependa unicamente de GLFW + GLAD.
//
//  Convencion de matrices: column-major. El arreglo float m[16] guarda las
//  columnas una tras otra, de modo que m[col*4 + row] es el elemento (row,col).
//  Asi se puede pasar el puntero directo a glUniformMatrix4fv(..., GL_FALSE, ...).
// =============================================================================
#pragma once
#include <cmath>

namespace mx {

// Pasa grados a radianes (las funciones trigonometricas de <cmath> usan radianes).
inline float radians(float deg) { return deg * 3.14159265358979323846f / 180.0f; }

// ----------------------------- vec3 -----------------------------------------
// Vector de 3 componentes. Se usa para posiciones, direcciones, normales y color.
struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

inline vec3 operator+(vec3 a, vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline vec3 operator-(vec3 a, vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline vec3 operator*(vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline vec3 operator*(float s, vec3 a) { return {a.x * s, a.y * s, a.z * s}; }

inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Producto cruz: vector perpendicular a 'a' y 'b' (usado para construir la base
// de la camara y normales).
inline vec3 cross(vec3 a, vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float length(vec3 a) { return std::sqrt(dot(a, a)); }

// Normaliza a longitud 1 (con guarda anti division-por-cero).
inline vec3 normalize(vec3 a) {
    float l = length(a);
    return (l > 1e-8f) ? a * (1.0f / l) : a;
}

// ----------------------------- mat4 -----------------------------------------
// Matriz 4x4 column-major. m[0..3] = 1a columna, m[4..7] = 2a, etc.
struct mat4 {
    float m[16];
};

// Matriz identidad (no transforma nada).
inline mat4 identity() {
    mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

// Producto a*b. Convencion column-major: el resultado aplica primero b y luego a
// (igual que en GLM/OpenGL: P*V*M transforma modelo -> vista -> clip).
inline mat4 operator*(const mat4& a, const mat4& b) {
    mat4 r{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k)
                s += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = s;
        }
    return r;
}

/**
 * Matriz de proyeccion en perspectiva (igual a glm::perspective).
 * @param fovyRad  campo de vision vertical, en radianes.
 * @param aspect   razon ancho/alto del viewport.
 * @param znear    plano cercano (>0).
 * @param zfar     plano lejano.
 * @returns mat4 que mapea el frustum a clip-space OpenGL ([-1,1] en z).
 */
inline mat4 perspective(float fovyRad, float aspect, float znear, float zfar) {
    float f = 1.0f / std::tan(fovyRad * 0.5f);  // cotangente de medio fov
    mat4 r{};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

/**
 * Matriz de vista que ubica la camara mirando hacia un punto (igual a
 * glm::lookAt). Construye una base ortonormal {s, u, -f} y traslada al ojo.
 * @param eye    posicion de la camara.
 * @param center punto al que mira.
 * @param up     vector "arriba" del mundo (aqui usamos +Z, porque Z es la altura).
 */
inline mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);   // hacia adelante
    vec3 s = normalize(cross(f, up));   // hacia la derecha
    vec3 u = cross(s, f);               // arriba real (re-ortogonalizado)
    mat4 r = identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] =  dot(f, eye);
    return r;
}

}  // namespace mx
