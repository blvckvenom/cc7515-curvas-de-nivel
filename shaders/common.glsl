// -----------------------------------------------------------------------------
//  common.glsl  -  Campo de altura fractal del terreno.
//  Se INYECTA tras la linea #version en terrain.vert y contour.vert (ver shader.h)
//  para que ambos pases calculen exactamente la misma superficie.
//
//  Uniforms (se setean por-programa desde C++):
//    uSeed        : semilla; al cambiarla (tecla G) nace una superficie nueva.
//    uHeightScale : altura maxima en unidades de mundo (z in [0, uHeightScale]).
//    uNoiseScale  : frecuencia espacial del ruido (cuantas "colinas").
// -----------------------------------------------------------------------------
uniform float uSeed;
uniform float uHeightScale;
uniform float uNoiseScale;
uniform float uTime;     // tiempo (s) para la animacion de oleaje
uniform float uWaveAmp;  // amplitud del oleaje: 0 = terreno estatico

// Hash pseudoaleatorio 2D->1D, desplazado por la semilla.
float hash21(vec2 p) {
    p = fract(p * vec2(127.31, 311.7) + uSeed * 0.137);
    p += dot(p, p + 47.21);
    return fract((p.x + p.y) * p.x);
}

// Ruido de valor con interpolacion suave (smoothstep) en una celda unitaria.
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);             // smoothstep
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// fBm: suma de 6 octavas de ruido (fractal Brownian motion). Devuelve ~[0,1].
float fbm(vec2 p) {
    float v = 0.0, amp = 0.5, freq = 1.0, norm = 0.0;
    for (int o = 0; o < 6; ++o) {
        v += amp * vnoise(p * freq);
        norm += amp;
        freq *= 2.0;
        amp *= 0.5;
    }
    return v / norm;                               // normalizado a [0,1]
}

// Altura del terreno (unidades de mundo) en la coordenada (x,y) del plano base.
// Usa DOMAIN WARPING: se distorsiona el dominio con ruido antes de evaluar el fBm,
// lo que rompe los artefactos de grilla del value-noise y da relieve mas organico.
// Si uWaveAmp>0 suma un oleaje animado (sin/cos en x,y desplazado por uTime).
float terrainHeight(vec2 xy) {
    vec2 p = xy * uNoiseScale;
    vec2 warp = vec2(vnoise(p + 5.2), vnoise(p - 9.1)) - 0.5;  // desplazamiento del dominio
    float h = fbm(p + 0.9 * warp) * uHeightScale;
    h += uWaveAmp * uHeightScale * 0.18 * sin(xy.x * 6.0 + uTime * 1.6)
                                        * cos(xy.y * 5.0 - uTime * 1.2);
    return h;
}
