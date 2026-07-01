// =============================================================================
//  Tarea 3 - Curvas de nivel de un terreno  (CC7515 Computacion en GPU)
//  C++ + GLFW + GLAD. Pipeline con vertex + fragment + geometry shader.
//
//  Que hace:
//   - Genera un terreno fractal (>10.000 puntos, fBm con domain warping) cuya ALTURA
//     la calcula el vertex shader; al presionar G nace una superficie nueva (semilla).
//   - Dibuja la superficie con iluminacion (luz puntual movible + ambiental) y colormap.
//   - Extrae curvas de nivel con un GEOMETRY SHADER: por cada triangulo que cruza la
//     altura H emite un segmento, engrosado a un quad de ancho fijo en pixeles. Cada
//     nivel se pinta de un color distinto. Modo "curva unica" a una altura definida.
//   - Animacion opcional de oleaje del terreno; MSAA 4x; captura de imagen.
//   - Camara orbital con zoom; el usuario ajusta alturas, niveles e iluminacion.
//
//  Controles: se imprimen al iniciar (ver printControls()).
// =============================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include "mathx.h"
#include "shader.h"
#include "camera.h"
#include "terrain.h"

#ifndef SHADER_DIR
#define SHADER_DIR "shaders/"   // fallback si CMake no lo define (correr desde la raiz)
#endif

// --- Parametros del mundo (constantes de diseno) ---
static const int   GRID_N            = 160;    // 160*160 = 25.600 puntos (>10.000)
static const float WORLD_HALF        = 1.0f;   // terreno en [-1,1] x [-1,1]
static const float HEIGHT_SCALE_WORLD= 0.45f;  // z maxima en unidades de mundo
static const float NOISE_SCALE       = 3.0f;   // frecuencia espacial del fractal
static const float MAX_METERS        = 6000.0f;// altura maxima real (enunciado)

// Estado de la aplicacion (accesible desde los callbacks de GLFW).
struct AppState {
    Camera cam;
    bool   dragging = false;
    double lastX = 0, lastY = 0;
    float  seed = 1.0f;          // semilla del fractal (tecla G la cambia)
    int    numLevels = 14;       // cantidad de curvas de nivel
    float  offsetMeters = 0.0f;  // corrimiento de las alturas (teclas [ ])
    float  ambient = 0.28f;      // intensidad de luz ambiental (teclas K/J)
    bool   showSurface = true;   // mostrar la superficie (Espacio)
    bool   wireframe = false;    // malla en alambre (W)
    bool   animateLight = true;  // luz orbitando (L)
    float  lightAngle = 0.9f;    // azimut de la luz (flechas <-/-> con L apagado)
    float  lightElev = 1.6f;     // altura (z) de la luz puntual (flechas arriba/abajo)
    bool   singleMode = false;   // dibujar UNA sola curva a una altura definida (U)
    float  singleMeters = 3000.0f; // altura de esa curva, en metros [0,6000]
    bool   waves = false;        // animacion de oleaje del terreno (M)
    bool   requestShot = false;  // pedir captura en runtime (P)
};

// Colormap "jet" (azul -> cian -> verde -> amarillo -> rojo) para distinguir niveles.
static mx::vec3 jet(float t) {
    auto cl = [](float v) { return v < 0 ? 0.0f : (v > 1 ? 1.0f : v); };
    return { cl(1.5f - std::fabs(4 * t - 3)),
             cl(1.5f - std::fabs(4 * t - 2)),
             cl(1.5f - std::fabs(4 * t - 1)) };
}

// Util de verificacion: guarda el framebuffer actual como PPM (P6), volteado en Y
// (OpenGL tiene el origen abajo-izquierda). Usado por el modo --shot.
static void savePPM(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);   // empaque sin padding (anchos arbitrarios)
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y)
        f.write(reinterpret_cast<char*>(&px[static_cast<size_t>(y) * w * 3]), w * 3);
}

// Chequeo de errores GL: imprime si hubo algun GL_INVALID_* tras una etapa clave.
static void glCheck(const char* etapa) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) std::fprintf(stderr, "GL error 0x%X tras %s\n", e, etapa);
}

static void printControls() {
    std::printf(
        "\n=== Tarea 3 - Curvas de nivel ===\n"
        "  Mouse arrastrar : orbitar camara (todas las perspectivas)\n"
        "  Rueda / +,-     : zoom y un-zoom\n"
        "  G               : generar una superficie nueva (vertex shader)\n"
        "  U               : modo una sola curva a una altura definida (on/off)\n"
        "  [ , ]           : bajar / subir la altura (de las curvas, o de la curva unica)\n"
        "  , . (coma/punto): menos / mas niveles de curva\n"
        "  M               : animacion de oleaje del terreno on/off\n"
        "  K / J           : menos / mas luz ambiental\n"
        "  L               : animar la luz puntual on/off\n"
        "  Flechas         : mover la luz (azimut <-/->, altura arriba/abajo)\n"
        "  Espacio         : mostrar/ocultar la superficie (ver solo curvas)\n"
        "  T               : vista cenital (mapa de curvas, como la Figura 1)\n"
        "  W               : malla en alambre on/off\n"
        "  P               : guardar captura (captura.ppm)\n"
        "  R               : resetear camara\n"
        "  ESC             : salir\n\n");
}

// ----------------------------- Callbacks GLFW --------------------------------
static void onMouseButton(GLFWwindow* w, int button, int action, int) {
    auto* s = static_cast<AppState*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        s->dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &s->lastX, &s->lastY);
    }
}

static void onCursor(GLFWwindow* w, double x, double y) {
    auto* s = static_cast<AppState*>(glfwGetWindowUserPointer(w));
    if (s->dragging) {
        float dx = static_cast<float>(x - s->lastX);
        float dy = static_cast<float>(y - s->lastY);
        s->cam.orbit(-dx * 0.005f, dy * 0.005f);   // arrastre -> giro
        s->lastX = x; s->lastY = y;
    }
}

static void onScroll(GLFWwindow* w, double, double yoff) {
    auto* s = static_cast<AppState*>(glfwGetWindowUserPointer(w));
    s->cam.zoom(yoff > 0 ? 0.9f : 1.1f);            // arriba acerca, abajo aleja
}

static void onKey(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    auto* s = static_cast<AppState*>(glfwGetWindowUserPointer(w));
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, 1); break;
        case GLFW_KEY_G:      s->seed += 1.7f; break;            // nueva superficie
        case GLFW_KEY_U:      s->singleMode = !s->singleMode; break;  // curva unica
        // [ ] ajustan la altura: la de la curva unica (modo U) o el offset del set.
        case GLFW_KEY_LEFT_BRACKET:
            if (s->singleMode) s->singleMeters = std::fmax(0.0f, s->singleMeters - 100.0f);
            else               s->offsetMeters -= 100.0f; break;
        case GLFW_KEY_RIGHT_BRACKET:
            if (s->singleMode) s->singleMeters = std::fmin(MAX_METERS, s->singleMeters + 100.0f);
            else               s->offsetMeters += 100.0f; break;
        case GLFW_KEY_COMMA:  if (s->numLevels > 1)  s->numLevels--; break;
        case GLFW_KEY_PERIOD: if (s->numLevels < 60) s->numLevels++; break;
        case GLFW_KEY_M: s->waves = !s->waves; break;            // oleaje
        case GLFW_KEY_K: s->ambient = std::fmax(0.0f, s->ambient - 0.04f); break;
        case GLFW_KEY_J: s->ambient = std::fmin(1.0f, s->ambient + 0.04f); break;
        case GLFW_KEY_L: s->animateLight = !s->animateLight; break;
        // Flechas: mover la luz puntual (azimut y altura).
        case GLFW_KEY_LEFT:  s->lightAngle -= 0.1f; break;
        case GLFW_KEY_RIGHT: s->lightAngle += 0.1f; break;
        case GLFW_KEY_UP:    s->lightElev = std::fmin(4.0f, s->lightElev + 0.15f); break;
        case GLFW_KEY_DOWN:  s->lightElev = std::fmax(0.2f, s->lightElev - 0.15f); break;
        case GLFW_KEY_SPACE: s->showSurface = !s->showSurface; break;
        case GLFW_KEY_W: s->wireframe = !s->wireframe; break;
        case GLFW_KEY_P: s->requestShot = true; break;           // captura en runtime
        case GLFW_KEY_T:  // vista cenital (mira casi recto hacia abajo)
            s->cam.pitch = mx::radians(89.0f); s->cam.yaw = mx::radians(90.0f); break;
        case GLFW_KEY_R:  // reset
            s->cam = Camera{}; break;
        case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD:      s->cam.zoom(0.9f); break;
        case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: s->cam.zoom(1.1f); break;
        default: break;
    }
}

int main(int argc, char** argv) {
    // Modo de verificacion: "--shot archivo.ppm [frames]" renderiza con la ventana
    // oculta, guarda una captura y sale (para validar el render sin pantalla en vivo).
    std::string shotPath; int shotFrames = 3;
    bool optTopdown = false, optNoSurface = false, optWaves = false;  // presets (--shot)
    float optSingle = -1.0f;  // >=0 => modo curva unica a esa altura en metros
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--shot") {
            if (i + 1 >= argc) { std::fprintf(stderr, "--shot requiere una ruta\n"); return 1; }
            shotPath = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') shotFrames = std::atoi(argv[++i]);
            if (shotFrames < 1) shotFrames = 1;
        } else if (a == "--topdown")    optTopdown = true;
        else if (a == "--no-surface")   optNoSurface = true;
        else if (a == "--waves")        optWaves = true;
        else if (a == "--single" && i + 1 < argc) optSingle = (float)std::atof(argv[++i]);
    }

    // --- 1. Ventana + contexto OpenGL 3.3 core (geometry shader es core en 3.2+) ---
    if (!glfwInit()) { std::fprintf(stderr, "No pude iniciar GLFW\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);   // requerido en macOS, inocuo aqui
    glfwWindowHint(GLFW_SAMPLES, 4);                       // MSAA 4x (antialiasing)
    if (!shotPath.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // captura sin mostrar

    GLFWwindow* win = glfwCreateWindow(1280, 800, "Tarea 3 - Curvas de nivel", nullptr, nullptr);
    if (!win) { std::fprintf(stderr, "No pude crear la ventana\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);  // v-sync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "No pude cargar OpenGL con GLAD\n"); return 1;
    }
    std::printf("OpenGL %s | GPU: %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    AppState app;
    if (optNoSurface) app.showSurface = false;
    if (optTopdown) { app.cam.pitch = mx::radians(89.0f); app.cam.yaw = mx::radians(90.0f); }
    if (optWaves) app.waves = true;
    if (optSingle >= 0.0f) { app.singleMode = true; app.singleMeters = optSingle; }
    glfwSetWindowUserPointer(win, &app);
    glfwSetMouseButtonCallback(win, onMouseButton);
    glfwSetCursorPosCallback(win, onCursor);
    glfwSetScrollCallback(win, onScroll);
    glfwSetKeyCallback(win, onKey);
    printControls();

    // --- 2. Recursos: malla del terreno + dos programas de shaders ---
    Terrain terrain;
    terrain.build(GRID_N, WORLD_HALF);
    std::printf("Terreno: %d x %d = %d puntos, %d indices\n",
                GRID_N, GRID_N, terrain.vertexCount(), (int)terrain.indexCount);

    std::string dir = SHADER_DIR;
    std::string common = readFile(dir + "common.glsl");   // se inyecta en los vertex shaders
    Shader surfaceProg, contourProg;
    surfaceProg.load(dir + "terrain.vert", dir + "terrain.frag", "", common);
    contourProg.load(dir + "contour.vert", dir + "contour.frag", dir + "contour.geom", common);

    const float texel = 2.0f * WORLD_HALF / (GRID_N - 1);  // espaciado para la normal

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);   // MSAA: antialiasing de superficie y curvas
    // (las curvas se engrosan geometricamente en contour.geom, sin depender de
    //  glLineWidth, que no es portable en core profile.)
    glCheck("setup (terreno + shaders)");

    // --- 3. Bucle de render ---
    int frame = 0;
    while (!glfwWindowShouldClose(win)) {
        int fbw, fbh;
        glfwGetFramebufferSize(win, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        float aspect = fbh > 0 ? (float)fbw / fbh : 1.0f;

        if (app.animateLight) app.lightAngle += 0.01f;

        // Oleaje: uTime avanza solo con waves activo; asi sin oleaje el render es estatico.
        float uTime = app.waves ? static_cast<float>(glfwGetTime()) : 0.0f;
        float uWaveAmp = app.waves ? 1.0f : 0.0f;

        mx::mat4 VP = app.cam.proj(aspect) * app.cam.view();
        mx::vec3 eye = app.cam.eye();
        // Luz puntual: azimut lightAngle, altura lightElev (movibles con las flechas).
        mx::vec3 lightPos{ 2.0f * std::cos(app.lightAngle),
                           2.0f * std::sin(app.lightAngle),
                           app.lightElev };

        glClearColor(0.09f, 0.10f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- 3a. Pase de superficie ---
        // Se dibuja SIEMPRE para fijar el depth buffer (asi las curvas de atras quedan
        // ocultas y el mapa de contornos se ve limpio). El toggle "ocultar superficie"
        // (tecla Espacio) solo apaga su color con glColorMask, dejando solo las curvas.
        // Empuja la superficie un poco hacia atras (polygon offset) para que las curvas,
        // que viven sobre ella, no hagan z-fighting y queden visibles encima.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        if (!app.showSurface)
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);   // solo profundidad
        else
            glPolygonMode(GL_FRONT_AND_BACK, app.wireframe ? GL_LINE : GL_FILL);

        surfaceProg.use();
        surfaceProg.set("uViewProj", VP);
        surfaceProg.set("uSeed", app.seed);
        surfaceProg.set("uHeightScale", HEIGHT_SCALE_WORLD);
        surfaceProg.set("uNoiseScale", NOISE_SCALE);
        surfaceProg.set("uTexel", texel);
        surfaceProg.set("uTime", uTime);
        surfaceProg.set("uWaveAmp", uWaveAmp);
        surfaceProg.set("uLightPos", lightPos);
        surfaceProg.set("uViewPos", eye);
        surfaceProg.set("uAmbient", app.ambient);
        terrain.draw();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_POLYGON_OFFSET_FILL);

        // --- 3b. Pase de curvas de nivel (geometry shader) ---
        contourProg.use();
        contourProg.set("uViewProj", VP);
        contourProg.set("uSeed", app.seed);
        contourProg.set("uHeightScale", HEIGHT_SCALE_WORLD);
        contourProg.set("uNoiseScale", NOISE_SCALE);
        contourProg.set("uTime", uTime);
        contourProg.set("uWaveAmp", uWaveAmp);
        // uViewport es vec2: se setea directo (Shader::set no tiene overload vec2).
        glUniform2f(glGetUniformLocation(contourProg.id, "uViewport"), (float)fbw, (float)fbh);
        contourProg.set("uThicknessPx", 2.4f);

        if (app.singleMode) {
            // Una sola curva a la altura definida por el usuario (en metros).
            float hWorld = app.singleMeters / MAX_METERS * HEIGHT_SCALE_WORLD;
            contourProg.set("uHeight", hWorld);
            contourProg.set("uColor", jet(app.singleMeters / MAX_METERS));
            terrain.draw();
        } else {
            // Conjunto de niveles equiespaciados (color distinto por nivel).
            float offsetWorld = app.offsetMeters / MAX_METERS * HEIGHT_SCALE_WORLD;
            for (int i = 0; i < app.numLevels; ++i) {
                float t = (i + 0.5f) / app.numLevels;         // [0,1] entre niveles
                float hWorld = t * HEIGHT_SCALE_WORLD + offsetWorld;
                contourProg.set("uHeight", hWorld);
                contourProg.set("uColor", jet(t));
                terrain.draw();   // el geometry shader convierte triangulos -> segmentos
            }
        }

        // --- 3c. HUD en el titulo de la ventana (alturas en METROS, 0-6000) ---
        char title[300];
        if (app.singleMode)
            std::snprintf(title, sizeof(title),
                "Tarea 3 | curva unica @ %.0f m | semilla=%.1f | ambiente=%.2f%s%s",
                app.singleMeters, app.seed, app.ambient,
                app.waves ? " | oleaje" : "", app.showSurface ? "" : " | solo curvas");
        else
            std::snprintf(title, sizeof(title),
                "Tarea 3 | %d niveles (cada ~%.0f m) | offset=%.0f m | semilla=%.1f | ambiente=%.2f%s%s",
                app.numLevels, MAX_METERS / app.numLevels, app.offsetMeters, app.seed,
                app.ambient, app.waves ? " | oleaje" : "", app.showSurface ? "" : " | solo curvas");
        glfwSetWindowTitle(win, title);

        // Captura en runtime (tecla P): guarda el frame actual a captura.ppm.
        if (app.requestShot) {
            savePPM("captura.ppm", fbw, fbh);
            std::printf("Captura guardada: captura.ppm (%dx%d)\n", fbw, fbh);
            app.requestShot = false;
        }

        // Modo captura por CLI: tras unos frames, guarda el framebuffer y termina.
        if (!shotPath.empty() && ++frame >= shotFrames) {
            savePPM(shotPath, fbw, fbh);
            std::printf("Captura guardada: %s (%dx%d)\n", shotPath.c_str(), fbw, fbh);
            break;
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
