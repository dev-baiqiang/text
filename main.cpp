// Copyright (c) 2018 BaiQiang. All rights reserved.

#include <iostream>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "screenshot.h"
#include "shader.h"

#include <vector>
#include "hb-icu.h"
#include "hb-ft.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

const unsigned int WINDOW_WIDTH = 800;
const unsigned int WINDOW_HEIGHT = 600;

static void onSizeChange(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

static void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    } else if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        screenshot(window, "screenshot.tga");
        std::cout << "screenshot!!!" << std::endl;
        glfwSetWindowShouldClose(window, true);
    }
}

struct Point {
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
};

typedef struct {
    GLuint texture1;
    int w;
    int h;
    Point *vertices;
    GLuint *indices;
    int vc;
    int ic;
} Atlas;

typedef struct {
    std::string data;
    std::string language;
    hb_script_t script;
    hb_direction_t direction;
    float space;
} HBText;

static void force_ucs2_charmap(FT_Face ftf) {
    for (int i = 0; i < ftf->num_charmaps; i++) {
        if (((ftf->charmaps[i]->platform_id == 0)
             && (ftf->charmaps[i]->encoding_id == 3))
            || ((ftf->charmaps[i]->platform_id == 3)
                && (ftf->charmaps[i]->encoding_id == 1))) {
            FT_Set_Charmap(ftf, ftf->charmaps[i]);
        }
    }
}

static FT_Face face;
static GLuint program;
static hb_font_t *hb_font;
static GLuint VAO, VBO, IBO;
static const int MAX_WIDTH = 1024;

void *initHB() {

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        exit(1);
    }
    if (FT_New_Face(ft, "fonts/fzhtjt.ttf", 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load hb_font" << std::endl;
        exit(1);
    }
    force_ucs2_charmap(face);
    hb_font = hb_ft_font_create(face, nullptr);
    // hb_ft_font_set_funcs(hb_font);
};

static hb_position_t HBFloatToFixed(float v) {
    return scalbnf(v, +8);
}

Atlas *renderText(HBText text, unsigned int size, float x = 0, float y = 0) {
    auto atlas = new Atlas;
    FT_Set_Pixel_Sizes(face, 0, size);
    auto buffer = hb_buffer_create();
    hb_font_set_ppem(hb_font, size, size);
    hb_font_set_scale(hb_font, HBFloatToFixed(size),
                      HBFloatToFixed(size));
    hb_buffer_reset(buffer);
    hb_buffer_set_direction(buffer, text.direction);
    hb_buffer_set_script(buffer, text.script);
    hb_buffer_set_language(buffer, hb_language_from_string(text.language.c_str(), text.language.size()));
    size_t length = text.data.size();

    hb_buffer_add_utf8(buffer, text.data.c_str(), length, 0, length);
    hb_shape(hb_font, buffer, nullptr, 0);


    unsigned int glyphCount;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    unsigned int roww = 0;
    unsigned int rowh = 0;
    unsigned int tw = 0;
    unsigned int th = 0;

    for (unsigned int i = 0; i < glyphCount; ++i) {

        hb_glyph_info_t info = infos[i];
        FT_Load_Glyph(face,
                      info.codepoint,
                      FT_LOAD_DEFAULT
        );

        FT_GlyphSlot slot = face->glyph;
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        FT_Bitmap bitmap = slot->bitmap;

        if (roww + bitmap.width + 1 >= MAX_WIDTH) {
            tw = std::max(tw, roww);
            th += rowh;
            roww = 0;
            rowh = 0;
        }
        roww += bitmap.width + 1;
        rowh = std::max(rowh, bitmap.rows);
    }
    tw = std::max(tw, roww);
    th += rowh;

    atlas->w = tw;
    atlas->h = th;

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &atlas->texture1);
    glBindTexture(GL_TEXTURE_2D, atlas->texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tw, th, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    /* Clamping to edges is important to prevent artifacts when scaling */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* Linear filtering usually looks best for text */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int ox = 0;
    int oy = 0;
    rowh = 0;
    int ic = 0;
    int vc = 0;
    atlas->vertices = new Point[4 * glyphCount];
    atlas->indices = new GLuint[6 * glyphCount];
    float letterSpace = text.space;
    float letterSpaceHalfLeft = letterSpace * 0.5f;
    float letterSpaceHalfRight = letterSpace - letterSpaceHalfLeft;

    if (glyphCount) {
        x += letterSpaceHalfLeft;
    }
    for (unsigned int i = 0; i < glyphCount; ++i) {
        hb_glyph_info_t info = infos[i];
        hb_glyph_position_t pos = positions[i];

        FT_Load_Glyph(face,
                      info.codepoint,
                      FT_LOAD_DEFAULT
        );

        FT_GlyphSlot slot = face->glyph;
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
        FT_Bitmap bitmap = slot->bitmap;

        if (ox + bitmap.width + 1 >= MAX_WIDTH) {
            oy += rowh;
            rowh = 0;
            ox = 0;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, bitmap.width, bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, bitmap.buffer);
        float s0 = ox / (float) tw;
        float t0 = oy / (float) th;

        float xa = (float) pos.x_advance / 64;
        float ya = (float) pos.y_advance / 64;
        // float xo = (float) pos.x_offset / 64;
        // float yo = (float) pos.y_offset / 64;

        float s1 = s0 + float(bitmap.width) / atlas->w;
        float t1 = t0 + float(bitmap.rows) / atlas->h;
        float x0 = x + slot->bitmap_left;
        float y0 = floor(y + slot->bitmap_top);
        float x1 = x0 + bitmap.width;
        float y1 = floor(y0 - bitmap.rows);

        // printf("vertices: %f,%f,%f,%f,%f,%f,%f,%f,%d \n", x0, y0, x1, y1, s0, t0, s1, t1, bitmap.width);

        rowh = std::max(rowh, bitmap.rows);
        ox += bitmap.width + 1;
        x += xa + letterSpace;
        y += ya;
        GLuint index = i * 4;

        atlas->vertices[vc++] = {
                x0, y0, s0, t0
        };
        atlas->vertices[vc++] = {
                x0, y1, s0, t1
        };
        atlas->vertices[vc++] = {
                x1, y1, s1, t1
        };
        atlas->vertices[vc++] = {
                x1, y0, s1, t0
        };

        atlas->indices[ic++] = index;
        atlas->indices[ic++] = index + 1;
        atlas->indices[ic++] = index + 2;
        atlas->indices[ic++] = index;
        atlas->indices[ic++] = index + 2;
        atlas->indices[ic++] = index + 3;
    }
    if (glyphCount) {
        x += letterSpaceHalfRight;
    }
    atlas->vc = vc;
    atlas->ic = ic;

    printf("atlas: %d, %d, %d, %d, %f \n", atlas->w, atlas->h, atlas->vc, ic, x);

    glBindTexture(GL_TEXTURE_2D, 0);
    hb_buffer_destroy(buffer);
    return atlas;
}

GLFWwindow *initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 1);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    }
    int width, height, nrChannels;
    unsigned char *data = stbi_load("res/icon.png", &width, &height, &nrChannels, 0);
    const GLFWimage icon = {
            width,
            height,
            data
    };
    glfwSetWindowIcon(window, 1, &icon);
    stbi_image_free(data);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, onSizeChange);
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
    }
    glfwSwapInterval(1);
    return window;
}

void initGL() {
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(1, 1, 1, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(WINDOW_WIDTH), 0.0f,
                                      static_cast<GLfloat>(WINDOW_HEIGHT));

    program = shader_load("res/vs_texture.glsl",
                          "res/fs_texture.glsl");
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &IBO);
    glBindVertexArray(0);
}

void drawText(Atlas *atlas) {
    glBindTexture(GL_TEXTURE_2D, atlas->texture1);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glBufferData(GL_ARRAY_BUFFER, atlas->vc * 4 * sizeof(GLfloat), atlas->vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, atlas->ic * sizeof(GLfloat), atlas->indices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, atlas->ic, GL_UNSIGNED_INT, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

void destroyAtlas(Atlas *a) {
    glDeleteTextures(1, &a->texture1);
    delete[] a->indices;
    delete[] a->vertices;
    delete a;
}

int main() {
    GLFWwindow *window = initWindow();
    initGL();
    initHB();
    HBText text1 = {
            "Single Texture",
            "zh",
            HB_SCRIPT_HAN,
            HB_DIRECTION_LTR
    };

    HBText text2 = {
            "How to render text",
            "en",
            HB_SCRIPT_LATIN,
            HB_DIRECTION_LTR,
            8.0f
    };

    HBText text3 = {
            "现代文本渲染：FreeType",
            "zh",
            HB_SCRIPT_HAN,
            HB_DIRECTION_LTR,
            1.2f
    };

    auto a1 = renderText(text1, 30, 20, WINDOW_HEIGHT - 50);
    auto a2 = renderText(text2, 40, 20, WINDOW_HEIGHT - 200);
    auto a3 = renderText(text3, 50, 20, WINDOW_HEIGHT - 350);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float timeValue = glfwGetTime();
        float value = sin(timeValue);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);

        glUniform3f(glGetUniformLocation(program, "textColor"), 0, 1.0, value);
        drawText(a1);

        glUniform3f(glGetUniformLocation(program, "textColor"), 0, value, value);
        drawText(a2);

        glUniform3f(glGetUniformLocation(program, "textColor"), 0.5, 0, value);
        drawText(a3);

        glUseProgram(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    destroyAtlas(a1);
    destroyAtlas(a2);
    destroyAtlas(a3);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &IBO);
    glDeleteProgram(program);
    FT_Done_Face(face);
    hb_font_destroy(hb_font);
    glfwTerminate();
    return 0;
}
