// =============================================================================
//  terrain.h  -  Malla del terreno como grilla regular triangulada.
//
//  Importante: el VBO guarda SOLO las coordenadas (x,y) del plano base. La altura
//  z = f(x,y, semilla) la calcula el VERTEX SHADER (ver shaders/common.glsl), de
//  modo que al cambiar la semilla con una tecla se genera una superficie nueva sin
//  reconstruir buffers (cumple "el vertex shader debe cambiar la posicion de los
//  puntos para generar una nueva superficie").
//
//  Se dibuja con GL_TRIANGLES: el programa del terreno lo sombrea como superficie,
//  y el programa de contorno lo recibe en el geometry shader para extraer las
//  curvas de nivel triangulo por triangulo.
// =============================================================================
#pragma once
#include <glad/glad.h>
#include <vector>

struct Terrain {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    int n = 0;              // vertices por lado (n*n total)
    float halfSize = 1.0f;  // el plano cubre [-halfSize, halfSize]^2 en world space

    /**
     * Construye la grilla n x n sobre [-half, half]^2 y la sube a la GPU.
     * @param gridN  vertices por lado. n*n debe ser >= 10000 (enunciado): n>=100.
     * @param half   semiancho del terreno en unidades de mundo.
     */
    void build(int gridN, float half) {
        n = gridN; halfSize = half;

        // --- Vertices: solo (x, y) del plano base. z se calcula en el shader. ---
        std::vector<float> verts;
        verts.reserve(static_cast<size_t>(n) * n * 2);
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                float u = static_cast<float>(i) / (n - 1);  // [0,1]
                float v = static_cast<float>(j) / (n - 1);
                verts.push_back((u * 2.0f - 1.0f) * half);   // x
                verts.push_back((v * 2.0f - 1.0f) * half);   // y
            }

        // --- Indices: dos triangulos por celda de la grilla. ---
        std::vector<unsigned int> idx;
        idx.reserve(static_cast<size_t>(n - 1) * (n - 1) * 6);
        for (int j = 0; j < n - 1; ++j)
            for (int i = 0; i < n - 1; ++i) {
                unsigned int a = j * n + i;          // esquina inferior-izquierda
                unsigned int b = a + 1;              // inferior-derecha
                unsigned int c = a + n;              // superior-izquierda
                unsigned int d = c + 1;              // superior-derecha
                idx.insert(idx.end(), {a, b, d,  a, d, c});  // 2 triangulos (CCW)
            }
        indexCount = static_cast<GLsizei>(idx.size());

        // --- Subida a la GPU: VAO (estado), VBO (vertices), EBO (indices). ---
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        // atributo 0 = vec2 (x,y), stride 2 floats, offset 0.
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Emite los triangulos. Lo usan tanto el pase de superficie como el de contorno.
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    int vertexCount() const { return n * n; }
};
