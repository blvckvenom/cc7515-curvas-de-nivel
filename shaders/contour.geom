#version 330 core
// contour.geom  -  Geometry shader: EXTRACCION DE LA CURVA DE NIVEL.
//
// Recibe un triangulo del terreno (3 posiciones en world space) y la altura objetivo
// uHeight. Donde el plano z=uHeight corta al triangulo, emite el SEGMENTO de iso-
// contorno. Esto implementa lo que pide el enunciado: "el geometry shader debe hacer
// que todos los triangulos que intersecten dicha altura generen un segmento".
//
// Algoritmo (marching triangles): un plano horizontal corta a un triangulo en 0 o 2
// aristas. Para una arista (A,B) cuyos extremos quedan a distinto lado de uHeight, el
// punto de cruce es P = mix(A,B,t), con t = (H - zA)/(zB - zA).
//
// En vez de un line_strip (cuyo grosor via glLineWidth NO es portable en core profile),
// el segmento se EXPANDE a un quad de ancho fijo en pixeles (triangle_strip de 4
// vertices): asi las curvas tienen grosor consistente en cualquier driver/GPU.

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 gWorldPos[];        // las 3 posiciones del triangulo (world space)

uniform float uHeight;      // altura del iso-contorno (world z)
uniform mat4  uViewProj;    // proyeccion * vista
uniform vec2  uViewport;    // tamano del viewport en pixeles (para el grosor)
uniform float uThicknessPx; // grosor de la curva en pixeles

// Si la arista (a,b) cruza uHeight, devuelve el punto de cruce ya proyectado a clip
// space y marca ok=true. Convencion estricta z<H para no doble-contar vertices en H.
vec4 crossClip(vec3 a, vec3 b, out bool ok) {
    bool aBelow = a.z < uHeight;
    bool bBelow = b.z < uHeight;
    ok = (aBelow != bBelow);
    if (!ok) return vec4(0.0);
    float t = (uHeight - a.z) / (b.z - a.z);
    return uViewProj * vec4(mix(a, b, t), 1.0);
}

void main() {
    // Recolectar los (hasta 2) puntos de cruce del triangulo.
    vec4 P[2]; int c = 0; bool ok; vec4 p;
    p = crossClip(gWorldPos[0], gWorldPos[1], ok); if (ok && c < 2) P[c++] = p;
    p = crossClip(gWorldPos[1], gWorldPos[2], ok); if (ok && c < 2) P[c++] = p;
    p = crossClip(gWorldPos[2], gWorldPos[0], ok); if (ok && c < 2) P[c++] = p;
    if (c < 2) return;                              // el triangulo no cruza la altura

    // Pasar a NDC y expandir el segmento a un quad de uThicknessPx pixeles de ancho.
    vec2 ndc0 = P[0].xy / P[0].w;
    vec2 ndc1 = P[1].xy / P[1].w;
    vec2 dirPx = (ndc1 - ndc0) * uViewport * 0.5;   // direccion del segmento en pixeles
    if (dot(dirPx, dirPx) < 1e-12) return;          // segmento degenerado
    vec2 perp = normalize(vec2(-dirPx.y, dirPx.x)); // normal unitaria en pixeles
    vec2 offNdc = perp / (uViewport * 0.5) * (uThicknessPx * 0.5);  // medio ancho en NDC

    float z0 = P[0].z / P[0].w;
    float z1 = P[1].z / P[1].w;
    // 4 vertices del quad (triangle_strip): a+, a-, b+, b-. w=1 porque ya dividimos.
    gl_Position = vec4(ndc0 + offNdc, z0, 1.0); EmitVertex();
    gl_Position = vec4(ndc0 - offNdc, z0, 1.0); EmitVertex();
    gl_Position = vec4(ndc1 + offNdc, z1, 1.0); EmitVertex();
    gl_Position = vec4(ndc1 - offNdc, z1, 1.0); EmitVertex();
    EndPrimitive();
}
