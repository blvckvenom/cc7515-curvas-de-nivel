#version 330 core
// terrain.frag  -  Fragment shader del PASE DE SUPERFICIE.
// Iluminacion Phong con una luz PUNTUAL + termino AMBIENTAL (pedido por el
// enunciado), y color base segun la altura (colormap tipo mapa topografico).

in vec3  vWorldPos;
in vec3  vNormal;
in float vHeight01;

uniform vec3  uLightPos;   // posicion de la luz puntual (world space)
uniform vec3  uViewPos;    // posicion de la camara (para el especular)
uniform float uAmbient;    // intensidad de la luz ambiental [0,1]

out vec4 FragColor;

// Colormap topografico: azul (bajo) -> verde -> marron -> blanco (cima).
vec3 colormap(float t) {
    vec3 c0 = vec3(0.10, 0.30, 0.65);  // agua / valle
    vec3 c1 = vec3(0.20, 0.55, 0.25);  // pradera
    vec3 c2 = vec3(0.55, 0.45, 0.25);  // roca / tierra
    vec3 c3 = vec3(0.95, 0.95, 0.98);  // nieve
    if (t < 0.33) return mix(c0, c1, t / 0.33);
    if (t < 0.66) return mix(c1, c2, (t - 0.33) / 0.33);
    return mix(c2, c3, (t - 0.66) / 0.34);
}

void main() {
    vec3 base = colormap(clamp(vHeight01, 0.0, 1.0));

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);                 // difuso (Lambert)
    float spec = pow(max(dot(V, R), 0.0), 24.0) * 0.25; // especular suave

    // ambiente + difuso modulan el color base; el especular suma brillo blanco.
    vec3 color = base * (uAmbient + (1.0 - uAmbient) * diff) + vec3(spec);
    FragColor = vec4(color, 1.0);
}
