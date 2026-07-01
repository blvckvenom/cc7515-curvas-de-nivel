#version 330 core
// contour.frag  -  Fragment shader del PASE DE CURVAS DE NIVEL.
// Pinta cada curva con el color de su nivel (lo decide la CPU por draw call), de
// modo que "las curvas de cada nivel sean distintas" (enunciado).

uniform vec3 uColor;   // color del nivel actual

out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
