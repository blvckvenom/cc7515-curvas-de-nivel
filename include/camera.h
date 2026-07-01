// =============================================================================
//  camera.h  -  Camara orbital (arcball simple) alrededor del centro del terreno.
//
//  Coordenadas: Z es la altura (arriba), siguiendo el enunciado ("La coordenada Z
//  corresponde a la altura"). La camara orbita el objetivo con dos angulos:
//    - yaw   (azimut): giro alrededor del eje Z.
//    - pitch (elevacion): angulo sobre el plano XY, acotado para no cruzar el cenit.
//  El radio controla el zoom (acercar/alejar).
// =============================================================================
#pragma once
#include "mathx.h"

struct Camera {
    mx::vec3 target{0.0f, 0.0f, 0.10f};  // centro del terreno (a media altura)
    float yaw   = mx::radians(45.0f);    // azimut inicial
    float pitch = mx::radians(35.0f);    // elevacion inicial
    float radius = 3.0f;                 // distancia al objetivo (zoom)
    float fov   = mx::radians(45.0f);    // campo de vision vertical
    float znear = 0.05f, zfar = 100.0f;  // znear holgado: mejor precision de profundidad

    // Posicion del ojo en coordenadas esfericas (Z arriba) respecto del objetivo.
    mx::vec3 eye() const {
        float cp = std::cos(pitch);
        mx::vec3 dir{cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch)};
        return target + dir * radius;
    }

    // Matriz de vista (up = +Z porque Z es la altura del mundo).
    mx::mat4 view() const { return mx::lookAt(eye(), target, {0.0f, 0.0f, 1.0f}); }

    // Matriz de proyeccion para la razon de aspecto actual del viewport.
    mx::mat4 proj(float aspect) const { return mx::perspective(fov, aspect, znear, zfar); }

    // Gira la camara (arrastre del mouse). Acota pitch a (-89,89) grados para que
    // la base de lookAt no degenere al alinearse con el eje +Z.
    void orbit(float dyaw, float dpitch) {
        yaw += dyaw;
        pitch += dpitch;
        const float lim = mx::radians(89.0f);
        if (pitch >  lim) pitch =  lim;
        if (pitch < -lim) pitch = -lim;
    }

    // Zoom multiplicativo (rueda del mouse / teclas). factor<1 acerca, >1 aleja.
    void zoom(float factor) {
        radius *= factor;
        if (radius < 0.3f) radius = 0.3f;   // no atravesar el terreno
        if (radius > 20.0f) radius = 20.0f;
    }
};
