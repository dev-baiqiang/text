// Copyright (c) 2018 BaiQiang. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include "screenshot.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

void screenshot(GLFWwindow *window, const char *path) {
    int width, height;

    FILE *out = fopen(path, "wb");

    glfwGetWindowSize(window, &width, &height);

    uint8_t *buffer = (uint8_t *) calloc(width * height * 3, sizeof(uint8_t));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, buffer);

    uint8_t tga_header[18] = {0};
    // Data code type -- 2 - uncompressed RGB image
    tga_header[2] = 2;
    tga_header[12] = width & 0xFF;
    tga_header[13] = (width >> 8) & 0xFF;
    tga_header[14] = height & 0xFF;
    tga_header[15] = (height >> 8) & 0xFF;
    tga_header[16] = 24;

    fwrite(tga_header, sizeof(uint8_t), 18, out);
    fwrite(buffer, sizeof(uint8_t), width * height * 3, out);

    fclose(out);
    free(buffer);
}

void saveScreenShot(GLFWwindow *window, const char *path) {
    int width, height;
    stbi_flip_vertically_on_write(1);
    glfwGetWindowSize(window, &width, &height);
    uint8_t *buffer = (uint8_t *) calloc(width * height * 3, sizeof(uint8_t));
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
    stbi_write_bmp(path, width, height, 3, buffer);
    free(buffer);
}
