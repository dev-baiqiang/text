// Copyright (c) 2018 BaiQiang. All rights reserved.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

void screenshot(GLFWwindow *window, const char *path);

void saveScreenShot(GLFWwindow *window, const char *path);

#endif

#ifdef __cplusplus
}
#endif