#pragma once

#include "vk_types.hpp"
#include <glfw/glfw3.h>

struct Camera {
	double lastMousePositionX;
	double lastMousePositionY;

	glm::vec3 velocity;
	glm::vec3 position;

	float pitch;
	float yaw;

	glm::mat4 viewMatrix();
	glm::mat4 rotationMatrix();

	void update();

	static void configureGLFW(GLFWwindow* window);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
};