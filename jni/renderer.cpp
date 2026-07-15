// ================================================================
// RENDERER - OPENGL ES 3.0 BACKEND
// ================================================================

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <cstring>

#define RENDER_TAG "Renderer"
#define RLOGI(...) __android_log_print(ANDROID_LOG_INFO, RENDER_TAG, __VA_ARGS__)
#define RLOGE(...) __android_log_print(ANDROID_LOG_ERROR, RENDER_TAG, __VA_ARGS__)

// ================================================================
// SHADERS
// ================================================================
static const char* vertex_shader_source = R"(
#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
out vec2 TexCoord;
out vec4 Color;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
    Color = aColor;
}
)";

static const char* fragment_shader_source = R"(
#version 300 es
precision mediump float;
in vec2 TexCoord;
in vec4 Color;
uniform sampler2D uTexture;
uniform int uUseTexture;
out vec4 FragColor;
void main() {
    if (uUseTexture == 1) {
        FragColor = texture(uTexture, TexCoord) * Color;
    } else {
        FragColor = Color;
    }
}
)";

// ================================================================
// FONT RENDERER (упрощённый, без ошибок)
// ================================================================
class FontRenderer {
private:
    GLuint texture_id = 0;
    bool initialized = false;
    GLuint shader_program = 0;
    
public:
    bool init(GLuint prog) {
        if (initialized) return true;
        shader_program = prog;
        
        // Создаём простую текстуру с белыми символами
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        int tex_w = 256, tex_h = 256;
        unsigned char* pixels = new unsigned char[tex_w * tex_h * 4];
        memset(pixels, 0, tex_w * tex_h * 4);
        
        // Просто заливаем белым (упрощённый шрифт)
        for (int i = 0; i < tex_w * tex_h * 4; i += 4) {
            pixels[i] = 255;
            pixels[i+1] = 255;
            pixels[i+2] = 255;
            pixels[i+3] = 255;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        delete[] pixels;
        
        initialized = true;
        RLOGI("Font initialized");
        return true;
    }
    
    float get_text_width(const std::string& text, float scale = 1.0f) {
        return text.length() * 10 * scale;
    }
    
    void draw_text(const std::string& text, float x, float y, float r, float g, float b, float a, float scale = 1.0f) {
        if (!initialized || text.empty()) return;
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUseProgram(shader_program);
        glUniform1i(glGetUniformLocation(shader_program, "uTexture"), 0);
        glUniform1i(glGetUniformLocation(shader_program, "uUseTexture"), 1);
        
        float w = 10 * scale;
        float h = 14 * scale;
        float cx = x;
        
        for (char c : text) {
            float vertices[32] = {
                cx, y,     0, 0, r, g, b, a,
                cx + w, y, 1, 0, r, g, b, a,
                cx, y + h, 0, 1, r, g, b, a,
                cx + w, y + h, 1, 1, r, g, b, a
            };
            
            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
            
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
            glEnableVertexAttribArray(2);
            
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDeleteBuffers(1, &vbo);
            
            cx += w + 2;
        }
    }
};

// ================================================================
// RENDERER CLASS
// ================================================================
class Renderer {
private:
    static Renderer* instance;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    ANativeWindow* window = nullptr;
    int width = 0, height = 0;
    bool initialized = false;
    FontRenderer font;
    GLuint shader_program = 0;
    GLuint vbo = 0;
    float projection[16];
    std::mutex mtx;
    int screen_width = 1080;
    int screen_height = 2400;
    
    struct Vertex {
        float x, y;
        float r, g, b, a;
    };
    std::vector<Vertex> vertices;
    
    Renderer() {}
    
    bool init_egl() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) { RLOGE("EGL: No display"); return false; }
        
        EGLint major, minor;
        if (!eglInitialize(display, &major, &minor)) { RLOGE("EGL: Init failed"); return false; }
        
        EGLint config_attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };
        
        EGLConfig config;
        EGLint num_configs;
        if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
            RLOGE("EGL: Choose config failed");
            return false;
        }
        
        EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        if (context == EGL_NO_CONTEXT) { RLOGE("EGL: Create context failed"); return false; }
        
        surface = eglCreateWindowSurface(display, config, window, nullptr);
        if (surface == EGL_NO_SURFACE) { RLOGE("EGL: Create surface failed"); return false; }
        
        if (!eglMakeCurrent(display, surface, surface, context)) {
            RLOGE("EGL: Make current failed");
            return false;
        }
        
        RLOGI("EGL initialized: %dx%d", width, height);
        return true;
    }
    
    bool init_shaders() {
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertex_shader_source, nullptr);
        glCompileShader(vertex);
        
        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragment_shader_source, nullptr);
        glCompileShader(fragment);
        
        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex);
        glAttachShader(shader_program, fragment);
        glLinkProgram(shader_program);
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        
        glUseProgram(shader_program);
        
        float left = 0, right = screen_width;
        float bottom = screen_height, top = 0;
        projection[0] = 2.0f / (right - left);
        projection[1] = 0; projection[2] = 0; projection[3] = 0;
        projection[4] = 0;
        projection[5] = 2.0f / (top - bottom);
        projection[6] = 0; projection[7] = 0;
        projection[8] = 0; projection[9] = 0;
        projection[10] = -1; projection[11] = 0;
        projection[12] = -(right + left) / (right - left);
        projection[13] = -(top + bottom) / (top - bottom);
        projection[14] = 0; projection[15] = 1;
        
        GLuint proj_loc = glGetUniformLocation(shader_program, "uProjection");
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);
        
        glGenBuffers(1, &vbo);
        
        RLOGI("Shaders compiled");
        return true;
    }
    
public:
    static Renderer& get() {
        if (!instance) instance = new Renderer();
        return *instance;
    }
    
    bool init(ANativeWindow* native_window, int w, int h) {
        std::lock_guard<std::mutex> lock(mtx);
        if (initialized) return true;
        
        window = native_window;
        width = w;
        height = h;
        screen_width = w;
        screen_height = h;
        
        if (!init_egl()) return false;
        if (!init_shaders()) return false;
        if (!font.init(shader_program)) return false;
        
        glClearColor(0, 0, 0, 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        
        initialized = true;
        RLOGI("Renderer initialized");
        return true;
    }
    
    void begin_frame() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!initialized) return;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        vertices.clear();
    }
    
    void end_frame() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!initialized || vertices.empty()) return;
        
        glUseProgram(shader_program);
        glUniform1i(glGetUniformLocation(shader_program, "uUseTexture"), 0);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        
        eglSwapBuffers(display, surface);
    }
    
    void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
        if (w <= 0 || h <= 0) return;
        float x2 = x + w, y2 = y + h;
        Vertex v[] = {
            {x, y, r, g, b, a}, {x2, y, r, g, b, a}, {x, y2, r, g, b, a},
            {x2, y, r, g, b, a}, {x2, y2, r, g, b, a}, {x, y2, r, g, b, a}
        };
        vertices.insert(vertices.end(), v, v + 6);
    }
    
    void draw_rect_outline(float x, float y, float w, float h, float r, float g, float b, float a, float thickness = 1.0f) {
        draw_rect(x, y, w, thickness, r, g, b, a);
        draw_rect(x, y + h - thickness, w, thickness, r, g, b, a);
        draw_rect(x, y, thickness, h, r, g, b, a);
        draw_rect(x + w - thickness, y, thickness, h, r, g, b, a);
    }
    
    void draw_line(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float thickness = 1.0f) {
        float dx = x2 - x1, dy = y2 - y1;
        float len = sqrt(dx*dx + dy*dy);
        if (len < 0.001f) return;
        float nx = -dy / len * thickness / 2;
        float ny = dx / len * thickness / 2;
        Vertex v[] = {
            {x1 + nx, y1 + ny, r, g, b, a},
            {x1 - nx, y1 - ny, r, g, b, a},
            {x2 + nx, y2 + ny, r, g, b, a},
            {x2 - nx, y2 - ny, r, g, b, a},
            {x2 + nx, y2 + ny, r, g, b, a},
            {x1 - nx, y1 - ny, r, g, b, a}
        };
        vertices.insert(vertices.end(), v, v + 6);
    }
    
    void draw_circle(float cx, float cy, float radius, float r, float g, float b, float a, int segments = 32) {
        std::vector<Vertex> verts;
        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 6.2831853f;
            float angle2 = (float)(i + 1) / segments * 6.2831853f;
            verts.push_back({cx, cy, r, g, b, a});
            verts.push_back({cx + cos(angle1) * radius, cy + sin(angle1) * radius, r, g, b, a});
            verts.push_back({cx + cos(angle2) * radius, cy + sin(angle2) * radius, r, g, b, a});
        }
        vertices.insert(vertices.end(), verts.begin(), verts.end());
    }
    
    void draw_text(const std::string& text, float x, float y, float r, float g, float b, float a, float size = 14.0f, bool center = false) {
        if (text.empty()) return;
        float scale = size / 16.0f;
        if (center) {
            float w = font.get_text_width(text, scale);
            x -= w / 2;
        }
        font.draw_text(text, x, y, r, g, b, a, scale);
    }
    
    void set_screen_size(int w, int h) {
        screen_width = w;
        screen_height = h;
    }
};

Renderer* Renderer::instance = nullptr;

// ================================================================
// WRAPPER FUNCTIONS (for ESP)
// ================================================================
void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    Renderer::get().draw_rect(x, y, w, h, r, g, b, a);
}

void draw_rect_outline(float x, float y, float w, float h, float r, float g, float b, float a, float thickness) {
    Renderer::get().draw_rect_outline(x, y, w, h, r, g, b, a, thickness);
}

void draw_text(float x, float y, const std::string& text, float r, float g, float b, float a, float size, bool center) {
    Renderer::get().draw_text(text, x, y, r, g, b, a, size, center);
}

void draw_line(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float thickness) {
    Renderer::get().draw_line(x1, y1, x2, y2, r, g, b, a, thickness);
}

void draw_filled_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    Renderer::get().draw_rect(x, y, w, h, r, g, b, a);
}

void draw_circle(float x, float y, float radius, float r, float g, float b, float a, int segments) {
    Renderer::get().draw_circle(x, y, radius, r, g, b, a, segments);
}

void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, float r, float g, float b, float a) {
    Renderer::get().draw_line(x1, y1, x2, y2, r, g, b, a, 1);
    Renderer::get().draw_line(x2, y2, x3, y3, r, g, b, a, 1);
    Renderer::get().draw_line(x3, y3, x1, y1, r, g, b, a, 1);
}
