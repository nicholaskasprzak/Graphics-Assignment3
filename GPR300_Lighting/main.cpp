#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "EW/Shader.h"
#include "EW/EwMath.h"
#include "EW/Camera.h"
#include "EW/Mesh.h"
#include "EW/Transform.h"
#include "EW/ShapeGen.h"

void processInput(GLFWwindow* window);
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height);
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mousePosCallback(GLFWwindow* window, double xpos, double ypos);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

float lastFrameTime;
float deltaTime;

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 720;

double prevMouseX;
double prevMouseY;
bool firstMouseInput = false;

/* Button to lock / unlock mouse
* 1 = right, 2 = middle
* Mouse will start locked. Unlock it to use UI
* */
const int MOUSE_TOGGLE_BUTTON = 1;
const float MOUSE_SENSITIVITY = 0.1f;
const float CAMERA_MOVE_SPEED = 5.0f;
const float CAMERA_ZOOM_SPEED = 3.0f;

Camera camera((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT);

glm::vec3 bgColor = glm::vec3(0);
glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 lightPosition = glm::vec3(0.0f, 3.0f, 0.0f);

bool wireFrame = false;

struct Light
{
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
};

struct DirectionalLight
{
	glm::vec3 direction;
	Light light;
};

struct PointLight
{
	glm::vec3 position;
	Light light;

	float constK, linearK, quadraticK;
};

struct SpotLight
{
	glm::vec3 position;
	glm::vec3 direction;
	Light light;

	float range;
	float innerAngle;
	float outerAngle;
	float angleFalloff;
};

struct Material
{
	glm::vec3 color;
	float ambientK, diffuseK, specularK; // (0-1 range)
	float shininess = 1; // (1-512 range)
};

int numPointLights = 0;
glm::vec3 pointLightOrbitCenter;
float pointLightOrbitRange;
float pointLightOrbitSpeed;

DirectionalLight _DirectionalLight;
PointLight _PointLight;
SpotLight _SpotLight;
Material _Material;

int main() {
	if (!glfwInit()) {
		printf("glfw failed to init");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Lighting", 0, 0);
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		printf("glew failed to init");
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, resizeFrameBufferCallback);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetScrollCallback(window, mouseScrollCallback);
	glfwSetCursorPosCallback(window, mousePosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	//Hide cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Setup UI Platform/Renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	//Dark UI theme.
	ImGui::StyleColorsDark();

	//Used to draw shapes. This is the shader you will be completing.
	Shader litShader("shaders/defaultLit.vert", "shaders/defaultLit.frag");

	//Used to draw light sphere
	Shader unlitShader("shaders/defaultLit.vert", "shaders/unlit.frag");

	ew::MeshData cubeMeshData;
	ew::createCube(1.0f, 1.0f, 1.0f, cubeMeshData);
	ew::MeshData sphereMeshData;
	ew::createSphere(0.5f, 64, sphereMeshData);
	ew::MeshData cylinderMeshData;
	ew::createCylinder(1.0f, 0.5f, 64, cylinderMeshData);
	ew::MeshData planeMeshData;
	ew::createPlane(1.0f, 1.0f, planeMeshData);

	ew::Mesh cubeMesh(&cubeMeshData);
	ew::Mesh sphereMesh(&sphereMeshData);
	ew::Mesh planeMesh(&planeMeshData);
	ew::Mesh cylinderMesh(&cylinderMeshData);

	//Enable back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//Initialize shape transforms
	ew::Transform cubeTransform;
	ew::Transform sphereTransform;
	ew::Transform planeTransform;
	ew::Transform cylinderTransform;
	ew::Transform lightTransform;

	cubeTransform.position = glm::vec3(-2.0f, 0.0f, 0.0f);
	sphereTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);

	planeTransform.position = glm::vec3(0.0f, -1.0f, 0.0f);
	planeTransform.scale = glm::vec3(10.0f);

	cylinderTransform.position = glm::vec3(2.0f, 0.0f, 0.0f);

	lightTransform.scale = glm::vec3(0.5f);
	lightTransform.position = glm::vec3(0.0f, 5.0f, 0.0f);

	while (!glfwWindowShouldClose(window)) {
		processInput(window);
		glClearColor(bgColor.r,bgColor.g,bgColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float time = (float)glfwGetTime();
		deltaTime = time - lastFrameTime;
		lastFrameTime = time;

		litShader.use();
		litShader.setMat4("_Projection", camera.getProjectionMatrix());
		litShader.setMat4("_View", camera.getViewMatrix());

		litShader.setVec3("_DirectionalLight.direction", _DirectionalLight.direction);
		litShader.setFloat("_DirectionalLight.light.intensity", _DirectionalLight.light.intensity);
		litShader.setVec3("_DirectionalLight.light.color", _DirectionalLight.light.color);
		
		for (int i = 0; i < numPointLights; i++)
		{
			float angle = (360 / numPointLights) * i;
			angle = glm::radians(angle);
			glm::vec3 lightPosition = glm::vec3(cos(angle + (time * pointLightOrbitSpeed)) * pointLightOrbitRange, 0, sin(angle + (time * pointLightOrbitSpeed)) * pointLightOrbitRange);
			lightPosition += pointLightOrbitCenter;

			glm::vec3 color;
			if ((i + 1) % 3 == 0)
			{
				color = glm::vec3(1, 0, 0);
			}

			else if ((i + 1) % 2 == 0)
			{
				color = glm::vec3(0, 1, 0);
			}

			else
			{
				color = glm::vec3(0, 0, 1);
			}

			litShader.setVec3("_PointLights[" + std::to_string(i) + "].position", lightPosition);
			litShader.setVec3("_PointLights[" + std::to_string(i) + "].light.color", color);
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].light.intensity", _PointLight.light.intensity);
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].constK", _PointLight.constK);
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].linearK", _PointLight.linearK);
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].quadraticK", _PointLight.quadraticK);

			unlitShader.use();
			unlitShader.setMat4("_Projection", camera.getProjectionMatrix());
			unlitShader.setMat4("_View", camera.getViewMatrix());

			lightTransform.position = lightPosition;
			unlitShader.setMat4("_Model", lightTransform.getModelMatrix());
			unlitShader.setVec3("_Color", color);
			sphereMesh.draw();
		}
		litShader.use();
		litShader.setInt("lightCount", numPointLights);


		litShader.setVec3("_SpotLight.position", _SpotLight.position);
		litShader.setVec3("_SpotLight.direction", _SpotLight.direction);
		litShader.setFloat("_SpotLight.light.intensity", _SpotLight.light.intensity);
		litShader.setVec3("_SpotLight.light.color", _SpotLight.light.color);
		litShader.setFloat("_SpotLight.range", _SpotLight.range);
		litShader.setFloat("_SpotLight.innerAngle", _SpotLight.innerAngle);
		litShader.setFloat("_SpotLight.outerAngle", _SpotLight.outerAngle);
		litShader.setFloat("_SpotLight.angleFalloff", _SpotLight.angleFalloff);

		litShader.setVec3("_Material.color", _Material.color);
		litShader.setFloat("_Material.ambientK", _Material.ambientK);
		litShader.setFloat("_Material.diffuseK", _Material.diffuseK);
		litShader.setFloat("_Material.specularK", _Material.specularK);
		litShader.setFloat("_Material.shininess", _Material.shininess);

		litShader.setVec3("_CameraPosition", camera.getPosition());

		//Draw cube
		litShader.setMat4("_Model", cubeTransform.getModelMatrix());
		cubeMesh.draw();

		//Draw sphere
		litShader.setMat4("_Model", sphereTransform.getModelMatrix());
		sphereMesh.draw();

		//Draw cylinder
		litShader.setMat4("_Model", cylinderTransform.getModelMatrix());
		cylinderMesh.draw();

		//Draw plane
		litShader.setMat4("_Model", planeTransform.getModelMatrix());
		planeMesh.draw();

		//Draw UI

		ImGui::Begin("Directional Light");

		ImGui::DragFloat3("Direction", &_DirectionalLight.direction.x, 1, 0, 360);
		ImGui::DragFloat("Intensity", &_DirectionalLight.light.intensity, 0.01, 0, 1);
		ImGui::ColorEdit3("Color", &_DirectionalLight.light.color.r);
		ImGui::End();

		ImGui::Begin("Point Light");
		ImGui::DragInt("Number of Lights", &numPointLights, 1, 0, 8);
		ImGui::DragFloat("Intensity", &_PointLight.light.intensity, 0.01, 0, 1);
		ImGui::DragFloat("Constant Coefficient", &_PointLight.constK, 0.01, 0, 1);
		ImGui::DragFloat("Linear Coefficient", &_PointLight.linearK, 0.01, 0, 1);
		ImGui::DragFloat("Quadratic Coefficient", &_PointLight.quadraticK, 0.01, 0, 1);
		ImGui::DragFloat3("Orbit Center", &pointLightOrbitCenter.x);
		ImGui::DragFloat("Orbit Radius", &pointLightOrbitRange);
		ImGui::DragFloat("Orbit Speed", &pointLightOrbitSpeed);
		ImGui::End();

		ImGui::Begin("Spot Light");

		ImGui::DragFloat3("Position", &_SpotLight.position.x);
		ImGui::DragFloat3("Direction", &_SpotLight.direction.x, 0.01, -1, 1);
		ImGui::DragFloat("Intensity", &_SpotLight.light.intensity, 0.01, 0, 1);
		ImGui::ColorEdit3("Color", &_SpotLight.light.color.r);
		ImGui::DragFloat("Range", &_SpotLight.range, 1, 0, 30);
		ImGui::DragFloat("Inner Angle", &_SpotLight.innerAngle, 1, 0, 180);
		ImGui::DragFloat("Outer Angle", &_SpotLight.outerAngle, 1, 0, 180);
		ImGui::DragFloat("Angle Falloff", &_SpotLight.angleFalloff, 0.01, 0, 5);
		ImGui::End();

		ImGui::Begin("Material");

		ImGui::ColorEdit3("Color", &_Material.color.r);
		ImGui::DragFloat("Ambient", &_Material.ambientK, 0.01, 0, 1);
		ImGui::DragFloat("Diffuse", &_Material.diffuseK, 0.01, 0, 1);
		ImGui::DragFloat("Specular", &_Material.specularK, 0.01, 0, 1);
		ImGui::DragFloat("Shininess", &_Material.shininess, 1, 1, 512);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwPollEvents();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}
//Author: Eric Winebrenner
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height)
{
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
	camera.setAspectRatio((float)SCREEN_WIDTH / SCREEN_HEIGHT);
	glViewport(0, 0, width, height);
}
//Author: Eric Winebrenner
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
	if (keycode == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
	//Reset camera
	if (keycode == GLFW_KEY_R && action == GLFW_PRESS) {
		camera.setPosition(glm::vec3(0, 0, 5));
		camera.setYaw(-90.0f);
		camera.setPitch(0.0f);
		firstMouseInput = false;
	}
	if (keycode == GLFW_KEY_1 && action == GLFW_PRESS) {
		wireFrame = !wireFrame;
		glPolygonMode(GL_FRONT_AND_BACK, wireFrame ? GL_LINE : GL_FILL);
	}
}
//Author: Eric Winebrenner
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (abs(yoffset) > 0) {
		float fov = camera.getFov() - (float)yoffset * CAMERA_ZOOM_SPEED;
		camera.setFov(fov);
	}
}
//Author: Eric Winebrenner
void mousePosCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		return;
	}
	if (!firstMouseInput) {
		prevMouseX = xpos;
		prevMouseY = ypos;
		firstMouseInput = true;
	}
	float yaw = camera.getYaw() + (float)(xpos - prevMouseX) * MOUSE_SENSITIVITY;
	camera.setYaw(yaw);
	float pitch = camera.getPitch() - (float)(ypos - prevMouseY) * MOUSE_SENSITIVITY;
	pitch = glm::clamp(pitch, -89.9f, 89.9f);
	camera.setPitch(pitch);
	prevMouseX = xpos;
	prevMouseY = ypos;
}
//Author: Eric Winebrenner
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	//Toggle cursor lock
	if (button == MOUSE_TOGGLE_BUTTON && action == GLFW_PRESS) {
		int inputMode = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
		glfwSetInputMode(window, GLFW_CURSOR, inputMode);
		glfwGetCursorPos(window, &prevMouseX, &prevMouseY);
	}
}

//Author: Eric Winebrenner
//Returns -1, 0, or 1 depending on keys held
float getAxis(GLFWwindow* window, int positiveKey, int negativeKey) {
	float axis = 0.0f;
	if (glfwGetKey(window, positiveKey)) {
		axis++;
	}
	if (glfwGetKey(window, negativeKey)) {
		axis--;
	}
	return axis;
}

//Author: Eric Winebrenner
//Get input every frame
void processInput(GLFWwindow* window) {

	float moveAmnt = CAMERA_MOVE_SPEED * deltaTime;

	//Get camera vectors
	glm::vec3 forward = camera.getForward();
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
	glm::vec3 up = glm::normalize(glm::cross(forward, right));

	glm::vec3 position = camera.getPosition();
	position += forward * getAxis(window, GLFW_KEY_W, GLFW_KEY_S) * moveAmnt;
	position += right * getAxis(window, GLFW_KEY_D, GLFW_KEY_A) * moveAmnt;
	position += up * getAxis(window, GLFW_KEY_Q, GLFW_KEY_E) * moveAmnt;
	camera.setPosition(position);
}
