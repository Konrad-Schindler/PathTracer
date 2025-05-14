#include "camera.hpp"
#include "engine.hpp"

#include <glm/gtc/quaternion.hpp>

glm::mat4 Camera::viewMatrix()
{
	glm::mat4 translation = glm::translate(glm::mat4(1.f), position);
	glm::mat4 rotation = rotationMatrix();
	return glm::inverse(translation * rotation);
}

glm::mat4 Camera::rotationMatrix()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });
	return glm::mat4_cast(yawRotation) * glm::mat4_cast(pitchRotation);
}

void Camera::update()
{
	if (pitch > 1) pitch = 1;
	if (pitch < -1) pitch = -1;

	glm::mat4 cameraRotation = rotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::configureGLFW(GLFWwindow* window)
{
	glfwSetKeyCallback(window, keyCallback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, cursorCallback);
}

void Camera::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Camera* camera = &reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window))->camera;
	
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_W) camera->velocity.z -= 1;
		if (key == GLFW_KEY_S) camera->velocity.z += 1;
		if (key == GLFW_KEY_A) camera->velocity.x -= 1;
		if (key == GLFW_KEY_D) camera->velocity.x += 1;
	}

	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_W) camera->velocity.z = 0;
		if (key == GLFW_KEY_S) camera->velocity.z = 0;
		if (key == GLFW_KEY_A) camera->velocity.x = 0;
		if (key == GLFW_KEY_D) camera->velocity.x = 0;
	}
}

void Camera::cursorCallback(GLFWwindow* window, double xpos, double ypos)
{
	static bool firstMouse = true;
	Camera* camera = &reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window))->camera;

	if (firstMouse) {
		camera->lastMousePositionX = xpos;
		camera->lastMousePositionY = ypos;
		firstMouse = false;
	}

	double xDiff = xpos - camera->lastMousePositionX;
	double yDiff = ypos - camera->lastMousePositionY;

	camera->yaw += (float)xDiff / 50.f;
	camera->pitch += (float)-yDiff / 50.f;

	camera->lastMousePositionX = xpos;
	camera->lastMousePositionY = ypos;
}
