#version 330 core
// contour.vert  -  Vertex shader del PASE DE CURVAS DE NIVEL.
// Solo desplaza el vertice a su altura y entrega la posicion 3D en WORLD SPACE al
// geometry shader (que es quien decide si el triangulo cruza la altura H y proyecta).
// "common.glsl" (terrainHeight + uniforms) se inyecta tras este #version.

layout(location = 0) in vec2 aPos;

out vec3 gWorldPos;   // posicion 3D del vertice -> entra al geometry shader

void main() {
    gWorldPos = vec3(aPos, terrainHeight(aPos));
    // gl_Position no se usa (el geometry shader proyecta), pero se setea por prolijidad.
    gl_Position = vec4(gWorldPos, 1.0);
}
