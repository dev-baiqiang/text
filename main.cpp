// Copyright (c) 2018 BaiQiang. All rights reserved.

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "screenshot.h"
#include "shader.h"

#include <vector>
#include "harfbuzz/hb-icu.h"
#include "harfbuzz/hb-ft.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

typedef struct {
    unsigned char *buffer;
    unsigned int width;
    unsigned int height;
    float bearing_x;
    float bearing_y;
} Glyph;

typedef struct {
    GLuint textureId;
    GLuint vertexBuffer;
    GLuint indexBuffer;
    GLuint vertexArray;
} Character;

typedef struct {
    std::string data;
    std::string language;
    hb_script_t script;
    hb_direction_t direction;

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

static Glyph *getGlyph(FT_Face *face, uint32_t glyphIndex) {
    Glyph *g = new Glyph;

    FT_Int32 flags = FT_LOAD_DEFAULT;

    FT_Load_Glyph(*face,
                  glyphIndex, // the glyph_index in the font file
                  flags
    );

    FT_GlyphSlot slot = (*face)->glyph;
    FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

    FT_Bitmap bitmap = slot->bitmap;
    g->buffer = bitmap.buffer;
    g->width = bitmap.width;
    g->height = bitmap.rows;
    g->bearing_x = slot->bitmap_left;
    g->bearing_y = slot->bitmap_top;

    return g;
}

static FT_Face face;
static GLuint program;
static hb_font_t *hb_font;

hb_buffer_t *initHB() {

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
    FT_Set_Char_Size(face, 0, 50 * 64, 72, 72);

    hb_font = hb_ft_font_create(face, NULL);
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_allocation_successful(buffer);
    return buffer;
};

std::vector<Character *> renderText(HBText text, hb_buffer_t *buffer, float x = 0, float y = 0) {
    std::vector<Character *> cs;

    hb_buffer_reset(buffer);
    hb_buffer_set_direction(buffer, text.direction);
    hb_buffer_set_script(buffer, text.script);
    hb_buffer_set_language(buffer, hb_language_from_string(text.language.c_str(), text.language.size()));
    size_t length = text.data.size();

    hb_buffer_add_utf8(buffer, text.data.c_str(), length, 0, length);

    hb_shape(hb_font, buffer, NULL, 0);

    unsigned int glyphCount;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    for (unsigned int i = 0; i < glyphCount; ++i) {
        hb_glyph_info_t info = infos[i];
        hb_glyph_position_t pos = positions[i];

//        static float HBFixedToFloat(hb_position_t v) {
//            return scalbnf(v, -8);
//        }
//        printf("DoLayout %u, %f; %f; %d, %d \n", info.codepoint,
//               HBFixedToFloat(positions[i].x_advance), HBFixedToFloat(positions[i].y_advance),
//               positions[i].x_offset,
//               positions[i].y_offset);

        Glyph *glyph = getGlyph(&face, info.codepoint);

        int tw = pow(2, ceil(log(glyph->width) / log(2)));
        int th = pow(2, ceil(log(glyph->height) / log(2)));

        auto data = new unsigned char[tw * th]();
        for (unsigned int j = 0; j < glyph->height; ++j) {
            memcpy(data + j * tw, glyph->buffer + j * glyph->width, glyph->width);
        }

        float s0 = 0.0;
        float t0 = 0.0;
        float s1 = (float) glyph->width / tw;
        float t1 = (float) glyph->height / th;
        float xa = (float) pos.x_advance / 64;
        float ya = (float) pos.y_advance / 64;
        float xo = (float) pos.x_offset / 64;
        float yo = (float) pos.y_offset / 64;
        float x0 = x + xo + glyph->bearing_x;
        float y0 = floor(y + yo + glyph->bearing_y);
        float x1 = x0 + glyph->width;
        float y1 = floor(y0 - glyph->height);
        // printf("vertices: %f,%f,%f,%f,%f,%f,%f,%f \n", x0, y0, s0, t0, x1, y1, s1, t1);

        GLfloat vertices[] = {
                x0, y0, s0, t0,
                x0, y1, s0, t1,
                x1, y1, s1, t1,
                x1, y0, s1, t0
        };
        GLuint indices[] = {
                0, 1, 2,
                0, 2, 3
        };

        Character *c = new Character();
        glGenVertexArrays(1, &c->vertexArray);
        glBindVertexArray(c->vertexArray);
        glGenTextures(1, &c->textureId);
        glBindTexture(GL_TEXTURE_2D, c->textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tw, th, 0, GL_RED, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glGenBuffers(1, &c->vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, c->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glGenBuffers(1, &c->indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        cs.push_back(c);

        x += xa;
        y += ya;
        delete glyph;
    }
    return cs;
}

GLFWwindow *initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 16);
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
}

void drawText(std::vector<Character *> cs) {
    for (auto c: cs) {
        glBindVertexArray(c->vertexArray);
        glBindTexture(GL_TEXTURE_2D, c->textureId);
        glBindBuffer(GL_ARRAY_BUFFER, c->vertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->indexBuffer);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void destroyCharacters(std::vector<Character *> cs) {
    for (auto c: cs) {
        glDeleteTextures(1, &c->textureId);
        glDeleteBuffers(1, &c->indexBuffer);
        glDeleteBuffers(1, &c->vertexBuffer);
        glDeleteVertexArrays(1, &c->vertexArray);
        delete c;
    }
}

int main() {
    GLFWwindow *window = initWindow();
    initGL();
    hb_buffer_t *buffer = initHB();
    HBText text = {
            "如何渲染文字",
            "zh",
            HB_SCRIPT_HAN,
            HB_DIRECTION_LTR
    };

    HBText text2 = {
            "How to render text",
            "en",
            HB_SCRIPT_LATIN,
            HB_DIRECTION_LTR
    };

    auto cs1 = renderText(text, buffer, 20, 50);
    auto cs2 = renderText(text2, buffer, 40, 120);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float timeValue = glfwGetTime();
        float value = sin(timeValue);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);

        glUniform3f(glGetUniformLocation(program, "textColor"), 0, 1.0, value);
        drawText(cs1);

        glUniform3f(glGetUniformLocation(program, "textColor"), 0, value, value);
        drawText(cs2);

        glUseProgram(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(program);
    destroyCharacters(cs1);
    destroyCharacters(cs2);
    FT_Done_Face(face);
    hb_buffer_destroy(buffer);
    hb_font_destroy(hb_font);
    glfwTerminate();
    return 0;
}

