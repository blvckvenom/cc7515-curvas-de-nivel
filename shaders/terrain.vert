#version 330 core
// terrain.vert  -  Vertex shader del PASE DE SUPERFICIE.
// Desplaza cada vertice del plano a su altura z=terrainHeight(x,y) y calcula la
// normal del terreno por diferencias finitas (para la iluminacion en el fragment).
// Aqui "common.glsl" (terrainHeight, uSeed, uHeightScale, uNoiseScale) se inyecta
// automaticamente tras esta linea #version.

layout(location = 0) in vec2 aPos;   // coordenada (x,y) del plano base

uniform mat4  uViewProj;             // matriz proyeccion * vista
uniform float uTexel;                // espaciado de la grilla (epsilon para la normal)

out vec3  vWorldPos;                 // posicion 3D del vertice (para iluminacion)
out vec3  vNormal;                   // normal del terreno
out float vHeight01;                 // altura normalizada [0,1] (para el colormap)

void main() {
    float z = terrainHeight(aPos);
    vec3 P  = vec3(aPos, z);

    // Normal analitica aproximada: n = normalize(-df/dx, -df/dy, 1), estimando las
    // derivadas con muestras vecinas del MISMO campo de altura (diferencias finitas).
    float e  = uTexel;
    float hl = terrainHeight(aPos - vec2(e, 0.0));
    float hr = terrainHeight(aPos + vec2(e, 0.0));
    float hd = terrainHeight(aPos - vec2(0.0, e));
    float hu = terrainHeight(aPos + vec2(0.0, e));
    vec3  N  = normalize(vec3(hl - hr, hd - hu, 2.0 * e));

    vWorldPos = P;
    vNormal   = N;
    vHeight01 = (uHeightScale > 0.0) ? z / uHeightScale : 0.0;

    gl_Position = uViewProj * vec4(P, 1.0);
}
