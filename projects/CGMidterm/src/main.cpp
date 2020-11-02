#include <Logging.h>
#include <iostream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <string>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Gameplay/Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Gameplay/Transform.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Texture2DData.h"
#include "Utilities/InputHelpers.h"
#include "Utilities/MeshBuilder.h"
#include "Utilities/MeshFactory.h"
#include "Utilities/NotObjLoader.h"
#include "Utilities/ObjLoader.h"
#include "Utilities/VertexTypes.h"


#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
		#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
		#endif
	default: break;
	}
}

GLFWwindow* window;
Camera::sptr camera = nullptr;

bool perspective = true;
bool isPressed = false;
int lives = 3;
int BrickHealth[54];
int brickHits[54];
bool isHit = false;
int firstDigitScore = 0;
int secondDigitScore = 0;
std::string currentFirstScore = "number" + std::to_string(firstDigitScore) + "Vao";
std::string currentSecondScore = "number" + std::to_string(secondDigitScore) + "Vao";



void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	camera->ResizeWindow(width, height);
}

bool initGLFW() {
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
	
	//Create a new GLFW window
	window = glfwCreateWindow(800, 800, "Brick Breaker", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

void InitImGui() {
	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output 
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	// Allow our viewports to use transparent backbuffers
	io.ConfigFlags |= ImGuiConfigFlags_TransparentBackbuffers;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}
}

void ShutdownImGui()
{
	// Cleanup the ImGui implementation
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// Destroy our ImGui context
	ImGui::DestroyContext();
}

std::vector<std::function<void()>> imGuiCallbacks;
void RenderImGui() {
	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

	if (ImGui::Begin("Debug")) {
		// Render our GUI stuff
		for (auto& func : imGuiCallbacks) {
			func();
		}
		ImGui::End();
	}
	
	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(window);
	}
}

void RenderVAO(
	const Shader::sptr& shader,
	const VertexArrayObject::sptr& vao,
	const Camera::sptr& camera,
	const Transform::sptr& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform->LocalTransform());
	shader->SetUniformMatrix("u_Model", transform->LocalTransform());
	shader->SetUniformMatrix("u_NormalMatrix", transform->NormalMatrix());
	vao->Render();
}

void ManipulateTransformWithInput(const Transform::sptr& transform, float dt) {
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		transform->MoveLocal(0.0f, -1.0f * dt, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { 
		transform->MoveLocal(0.0f,  1.0f * dt, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		transform->MoveLocal(-1.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		transform->MoveLocal( 1.0f * dt, 0.0f,  0.0f); 
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		transform->MoveLocal(0.0f, 0.0f,  1.0f * dt);
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		transform->MoveLocal(0.0f, 0.0f, -1.0f * dt);
	}

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) { 
		transform->RotateLocal(0.0f, -45.0f * dt, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		transform->RotateLocal(0.0f,  45.0f * dt,0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		transform->RotateLocal( 45.0f * dt, 0.0f,0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		transform->RotateLocal(-45.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, 0.0f, 45.0f * dt);
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, 0.0f, -45.0f * dt);
	}
}

void PaddleInput(const Transform::sptr& transform, float dt) {
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {

		transform->MoveLocal(0.0f, 0.0f, 3.0f * dt);

		if (transform->GetLocalPosition().x >= 3.93)
		{
			transform->SetLocalPosition(3.93f, transform->GetLocalPosition().y, transform->GetLocalPosition().z);
		}
		
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {

		transform->MoveLocal(0.0f, 0.0f, -3.0f * dt);

		if (transform->GetLocalPosition().x <= -4.2)
		{
			transform->SetLocalPosition(-4.2f, transform->GetLocalPosition().y, transform->GetLocalPosition().z);
		}
		
	}
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		transform->MoveLocal(-1.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		transform->MoveLocal(1.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
	{
		firstDigitScore = 4;
		secondDigitScore = 5;
	}
	/*
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		transform->MoveLocal(0.0f, 0.0f, 1.0f * dt);
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		transform->MoveLocal(0.0f, 0.0f, -1.0f * dt);
	}*/
}

struct Material
{
	Texture2D::sptr Albedo;
	Texture2D::sptr Specular;
	Texture2D::sptr NewTexture;

	float           Shininess;
	float			TextureMix;
};

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	 
	
	VertexArrayObject::sptr paddleVao = ObjLoader::LoadFromFile("models/Paddle.obj");

	VertexArrayObject::sptr brickVao = ObjLoader::LoadFromFile("models/Brick.obj");

	VertexArrayObject::sptr ballVao = ObjLoader::LoadFromFile("models/Ball.obj");

	VertexArrayObject::sptr borderVao = ObjLoader::LoadFromFile("models/Border.obj");

	VertexArrayObject::sptr number0Vao = ObjLoader::LoadFromFile("models/0.obj");

	VertexArrayObject::sptr number1Vao = ObjLoader::LoadFromFile("models/1.obj");

	VertexArrayObject::sptr number2Vao = ObjLoader::LoadFromFile("models/2.obj");

	VertexArrayObject::sptr number3Vao = ObjLoader::LoadFromFile("models/3.obj");

	VertexArrayObject::sptr number4Vao = ObjLoader::LoadFromFile("models/4.obj");

	VertexArrayObject::sptr number5Vao = ObjLoader::LoadFromFile("models/5.obj");

	VertexArrayObject::sptr number6Vao = ObjLoader::LoadFromFile("models/6.obj");

	VertexArrayObject::sptr number7Vao = ObjLoader::LoadFromFile("models/7.obj");

	VertexArrayObject::sptr number8Vao = ObjLoader::LoadFromFile("models/8.obj");

	VertexArrayObject::sptr number9Vao = ObjLoader::LoadFromFile("models/9.obj");

	VertexArrayObject::sptr winVao = ObjLoader::LoadFromFile("models/You Won.obj");

	VertexArrayObject::sptr loseVao = ObjLoader::LoadFromFile("models/Game Over.obj");

	VertexArrayObject::sptr backVao = ObjLoader::LoadFromFile("models/Background.obj");

	static const VertexPosCol interleaved[] = {
    //     X      Y     Z       R     G    B
		{{ 0.5f, -0.5f, 0.0f},   {0.0f, 0.0f, 0.0f, 1.0f}},
		{{ 0.5f,  0.5f, 0.0f},  {0.3f, 0.2f, 0.5f, 1.0f}},
	    {{-0.5f,  0.5f, 0.0f},  {1.0f, 1.0f, 0.0f, 1.0f}},
		{{ 0.5f,  1.0f, 0.0f},  {1.0f, 1.0f, 1.0f, 1.0f}}
	};

	VertexBuffer::sptr interleaved_vbo = VertexBuffer::Create();
	interleaved_vbo->LoadData(interleaved, 4);

	static const uint16_t indices[] = {
		0, 1, 2,
		1, 3, 2
	};
	IndexBuffer::sptr interleaved_ibo = IndexBuffer::Create();
	interleaved_ibo->LoadData(indices, 3 * 2);

	size_t stride = sizeof(VertexPosCol);
	VertexArrayObject::sptr vao2 = VertexArrayObject::Create();
	vao2->AddVertexBuffer(interleaved_vbo, VertexPosCol::V_DECL);
	vao2->SetIndexBuffer(interleaved_ibo);
		
	// We'll use the provided mesh builder to build a new mesh with a few elements
	MeshBuilder<VertexPosNormTexCol> builder = MeshBuilder<VertexPosNormTexCol>();
	MeshFactory::AddCube(builder, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
	VertexArrayObject::sptr vao3 = builder.Bake();

	// We'll be implementing a loader that works a bit like an OBJ loader to learn how to read files, we'll
	// load an exact copy of the mesh created above
	VertexArrayObject::sptr sceneVao = NotObjLoader::LoadFromFile("Sample.notobj");
		
	// Load our shaders
	Shader::sptr shader = Shader::Create();
	shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
	shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);  
	shader->Link();  

	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
	glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
	float     lightAmbientPow = 0.05f;
	float     lightSpecularPow = 1.0f;
	glm::vec3 ambientCol = glm::vec3(1.0f);
	float     ambientPow = 1.0f;
	float     textureMix = 0.2f;
	float     shininess = 4.0f;
	float     lightLinearFalloff = 0.09f;
	float     lightQuadraticFalloff = 0.032f;
	
	// These are our application / scene level uniforms that don't necessarily update
	// every frame
	shader->SetUniform("u_LightPos", lightPos);
	shader->SetUniform("u_LightCol", lightCol);
	shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
	shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
	shader->SetUniform("u_AmbientCol", ambientCol);
	shader->SetUniform("u_AmbientStrength", ambientPow);
	shader->SetUniform("u_TextureMix", textureMix);
	shader->SetUniform("u_Shininess", shininess);
	shader->SetUniform("u_LightAttenuationConstant", 1.0f);
	shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
	shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);

	// We'll add some ImGui controls to control our shader
	imGuiCallbacks.push_back([&]() {
		if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
		{
			if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
				shader->SetUniform("u_AmbientCol", ambientCol);
			}
			if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
				shader->SetUniform("u_AmbientStrength", ambientPow); 
			}
		}
		if (ImGui::CollapsingHeader("Light Level Lighting Settings")) 
		{
			if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
				shader->SetUniform("u_LightPos", lightPos);
			}
			if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
				shader->SetUniform("u_LightCol", lightCol);
			}
			if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
				shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
			}
			if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
				shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
			}
			if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
				shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
			}
			if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
				shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
			}
		}
		if (ImGui::CollapsingHeader("Material Level Lighting Settings"))
		{
			if (ImGui::SliderFloat("Shininess", &shininess, 0.1f, 128.0f)) {
				shader->SetUniform("u_Shininess", shininess);
			}
		}
	});

	// GL states
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// NEW STUFF

	// Create some transforms and initialize them
	Transform::sptr transforms[4];
	Transform::sptr paddleTransform;
	Transform::sptr ballTransform;
	Transform::sptr ball2Transform;
	Transform::sptr ball3Transform;
	Transform::sptr ball4Transform;
	Transform::sptr borderTransform;
	Transform::sptr brickTransform[54];
	Transform::sptr firstDigitTransform;
	Transform::sptr secondDigitTransform;
	Transform::sptr winTransform;
	Transform::sptr loseTransform;
	Transform::sptr backTransform;

	paddleTransform = Transform::Create();
	ballTransform = Transform::Create();
	ball2Transform = Transform::Create();
	ball3Transform = Transform::Create();
	ball4Transform = Transform::Create();
	borderTransform = Transform::Create();
	firstDigitTransform = Transform::Create();
	secondDigitTransform = Transform::Create();
	winTransform = Transform::Create();
	loseTransform = Transform::Create();
	backTransform = Transform::Create();

	for (int i = 0; i < 54; i++)
	{
		brickTransform[i] = Transform::Create();

		brickHits[i] = 0;
	}

	transforms[0] = Transform::Create();
	transforms[1] = Transform::Create();
	transforms[2] = Transform::Create();
	transforms[3] = Transform::Create();

	//IMPORTANT NOTES:
	//X = Left and Right
	//Y = In and Out
	//Z = Up and Down

	// We can use operator chaining, since our Set* methods return a pointer to the instance, neat!
	paddleTransform->SetLocalPosition(0.0f, 0.0f, -4.0f)->SetLocalRotation(0.0f, 90.0f, 0.0f)->SetLocalScale(0.2f,0.5f, 0.3f);
	ballTransform->SetLocalPosition(0.0f, 0.0f, -2.0f)->SetLocalScale(0.3f, 0.3f, 0.3f);
	ball2Transform->SetLocalPosition(-4.0f, 0.0f, -5.5f)->SetLocalScale(0.3f, 0.3f, 0.3f);
	ball3Transform->SetLocalPosition(-4.4f, 0.0f, -5.5f)->SetLocalScale(0.3f, 0.3f, 0.3f);
	ball4Transform->SetLocalPosition(-4.8f, 0.0f, -5.5f)->SetLocalScale(0.3f, 0.3f, 0.3f);
	borderTransform->SetLocalPosition(-0.14f, 0.0f, 0.4f)->SetLocalRotation(0.0f, 0.0f, 90.0f)->SetLocalScale(0.05f, 0.18f, 0.15f);
	firstDigitTransform->SetLocalPosition(4.0f, 0.0f, -5.5f)->SetLocalRotation(90.0f, 0.0f, 90.0f)->SetLocalScale(1.0f, 1.0f, 1.0f);
	secondDigitTransform->SetLocalPosition(4.4f, 0.0f, -5.5f)->SetLocalRotation(90.0f, 0.0f, 90.0f)->SetLocalScale(1.0f, 1.0f, 1.0f);
	winTransform->SetLocalPosition(2.6f, 0.0f, 0.0f)->SetLocalRotation(90.0f, 0.0f, 90.0f)->SetLocalScale(2.0f, 2.0f, 2.0f);
	loseTransform->SetLocalPosition(2.85f, 0.0f, 0.0f)->SetLocalRotation(90.0f, 0.0f, 90.0f)->SetLocalScale(2.0f, 2.0f, 2.0f);
	backTransform->SetLocalPosition(0.0f, -3.0f, 0.5f)->SetLocalRotation(0.0f, 0.0f, 90.0f)->SetLocalScale(2.0f, 2.0f, 1.9f);

	//Ball Variables
	float ballSpeed = 2.0f;
	glm::vec3 moveDir = glm::vec3(-1.0f, 0.0f, -1.0f);

	int brickCounter = 0;
	float xMove = 0.0f;
	float yMove = 0.5f;

	for (int y = 0; y < 6; y++)
	{
		yMove -= 0.5f;
		for (int x = 0; x < 9; x++)
		{
			brickTransform[brickCounter]->SetLocalPosition(3.65f + xMove, 0.0f, 4.2f + yMove)->SetLocalRotation(0.0f, 90.0f, 0.0f)->SetLocalScale(0.3f, 0.5f, 0.4f);
			xMove += -0.95f;

			if (brickTransform[brickCounter]->GetLocalPosition().z >= 3.4f)
			{
				BrickHealth[brickCounter] = 2;
			}
			else
			{
				BrickHealth[brickCounter] = 1;
			}

			brickCounter++;
		}
		xMove = 0.0f;
	}

	transforms[1]->SetLocalPosition(2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 45.0f);
	transforms[2]->SetLocalPosition(-2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, -45.0f);
	transforms[3]->SetLocalPosition(0.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, -45.0f);

	// We'll store all our VAOs into a nice array for easy access
	VertexArrayObject::sptr vaos[4];
	vaos[0] = sceneVao;
	vaos[1] = paddleVao;
	vaos[2] = vao2;
	vaos[3] = vao3;

	// TODO: load textures
	
	// Load our texture data from a file
	Texture2DData::sptr diffuseMap = Texture2DData::LoadFromFile("images/Stone_001_Diffuse.png");
	Texture2DData::sptr diffuseMap2 = Texture2DData::LoadFromFile("images/box.bmp");
	Texture2DData::sptr specularMap = Texture2DData::LoadFromFile("images/Stone_001_Specular.png");

	Texture2DData::sptr paddleMap = Texture2DData::LoadFromFile("images/RedPaddle.png");
	Texture2DData::sptr ballMap = Texture2DData::LoadFromFile("images/BallColour.png");
	Texture2DData::sptr baseBrickMap = Texture2DData::LoadFromFile("images/GreenBrick.png");
	Texture2DData::sptr hitBrickMap = Texture2DData::LoadFromFile("images/YellowBrick.png");
	Texture2DData::sptr boundaryMap = Texture2DData::LoadFromFile("images/BoundaryColour.png");
	Texture2DData::sptr backMap = Texture2DData::LoadFromFile("images/Background.png");

	// Create a texture from the data
	Texture2D::sptr diffuse = Texture2D::Create();
	diffuse->LoadData(diffuseMap);

	// Create a texture from the data
	Texture2D::sptr diffuse2 = Texture2D::Create();
	diffuse2->LoadData(diffuseMap2);

	Texture2D::sptr specular = Texture2D::Create();
	specular->LoadData(specularMap);

	Texture2D::sptr paddleDiffuse = Texture2D::Create();
	paddleDiffuse->LoadData(paddleMap);

	Texture2D::sptr ballDiffuse = Texture2D::Create();
	ballDiffuse->LoadData(ballMap);

	Texture2D::sptr baseBrickDiffuse = Texture2D::Create();
	baseBrickDiffuse->LoadData(baseBrickMap);

	Texture2D::sptr boundaryDiffuse = Texture2D::Create();
	boundaryDiffuse->LoadData(boundaryMap);

	Texture2D::sptr hitBrickDiffuse = Texture2D::Create();
	hitBrickDiffuse->LoadData(hitBrickMap);

	Texture2D::sptr BackDiffuse = Texture2D::Create();
	BackDiffuse->LoadData(backMap);

	// Creating an empty texture
	Texture2DDescription desc = Texture2DDescription();
	desc.Width = 1;
	desc.Height = 1;
	desc.Format = InternalFormat::RGB8;
	Texture2D::sptr texture2 = Texture2D::Create(desc);
	texture2->Clear();


	// TODO: store some info about our materials for each object

	// We'll use a temporary lil structure to store some info about our material (we'll expand this later)
	Material materials[4];
	Material paddleMaterial;
	Material ballMaterial;
	Material baseBrickMaterial;
	Material hitBrickMaterial;
	Material boundaryMaterial;
	Material backMaterial;

	paddleMaterial.Albedo = paddleDiffuse;
	paddleMaterial.Specular = specular;
	paddleMaterial.Shininess = 16.0f;

	ballMaterial.Albedo = ballDiffuse;
	ballMaterial.Specular = specular;
	ballMaterial.Shininess = 16.0f;

	baseBrickMaterial.Albedo = baseBrickDiffuse;
	baseBrickMaterial.Specular = specular;
	baseBrickMaterial.Shininess = 16.0f;

	hitBrickMaterial.Albedo = hitBrickDiffuse;
	hitBrickMaterial.Specular = specular;
	hitBrickMaterial.Shininess = 16.0f;

	boundaryMaterial.Albedo = boundaryDiffuse;
	boundaryMaterial.Specular = specular;
	boundaryMaterial.Shininess = 16.0f;

	backMaterial.Albedo = BackDiffuse;
	backMaterial.Specular = specular;
	backMaterial.Shininess = 16.0f;
	
	materials[0].Albedo    = diffuse;
	materials[0].NewTexture = diffuse2;
	materials[0].Specular  = specular;
	materials[0].Shininess = 4.0f;
	materials[0].TextureMix = 0.4f;

	materials[1].Albedo    = diffuse;
	materials[1].NewTexture = diffuse2;
	materials[1].Specular  = specular;
	materials[1].Shininess = 16.0f;
	materials[1].TextureMix = 0.5f;

	materials[2].Albedo    = diffuse;
	materials[2].NewTexture = diffuse2;
	materials[2].Specular  = specular;
	materials[2].Shininess = 32.0f;
	materials[2].TextureMix = 0.8f;
	
	materials[3].Albedo    = diffuse;
	materials[3].NewTexture = diffuse2;
	materials[3].Specular  = specular;
	materials[3].Shininess = 64.0f;
	materials[3].TextureMix = 0.6f;
	
	camera = Camera::Create();
	camera->SetPosition(glm::vec3(0, 5, 0)); // Set initial position
	camera->SetUp(glm::vec3(0, 0, 1)); // Use a z-up coordinate system
	camera->LookAt(glm::vec3(0.0f)); // Look at center of the screen
	camera->SetFovDegrees(90.0f); // Set an initial FOV
	camera->SetOrthoHeight(3.0f);


	// We'll use a vector to store all our key press events for now
	std::vector<KeyPressWatcher> keyToggles;
	// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
	// how this is implemented. Note that the ampersand here is capturing the variables within
	// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
	// use std::bind
	keyToggles.emplace_back(GLFW_KEY_T, [&](){ camera->ToggleOrtho(); });

	int selectedVao = 0; // select cube by default
	keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
		selectedVao++;
		if (selectedVao >= 4)
			selectedVao = 1;
	});
	keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
		selectedVao--;
		if (selectedVao <= 0)
			selectedVao = 3;
	});

	InitImGui();
		
	// Our high-precision timer
	double lastFrame = glfwGetTime();
	
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		// We'll make sure our UI isn't focused before we start handling input for our game
		if (!ImGui::IsAnyWindowFocused()) {
			// We need to poll our key watchers so they can do their logic with the GLFW state
			// Note that since we want to make sure we don't copy our key handlers, we need a const
			// reference!
			for (const KeyPressWatcher& watcher : keyToggles) {
				watcher.Poll(window);
			}

			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isPressed == false)
			{
				isPressed = true;
				perspective = !perspective;
				camera->SetProjectionType(perspective);
				//camera->GetViewProjection();
			}
			else if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE && isPressed == true)
			{
				isPressed = false;

			}



			// We'll run some basic input to move our transform around
			//ManipulateTransformWithInput(transforms[selectedVao], dt);
			PaddleInput(paddleTransform, dt);

			//PaddleInput(brickTransform[2], dt);
		}
						
		glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader->Bind();
		// These are the uniforms that update only once per frame
		shader->SetUniformMatrix("u_View", camera->GetView());
		shader->SetUniform("u_CamPos", camera->GetPosition());
		
		// Tell OpenGL that slot 0 will hold the diffuse, and slot 1 will hold the specular
		shader->SetUniform("s_Diffuse",  0);
		shader->SetUniform("s_Specular", 1); 
		shader->SetUniform("s_Diffuse2", 2);
		
		// Render all VAOs in our scene
		/*
		for(int ix = 0; ix < 4; ix++) {
			// TODO: Apply materials
			
			// Apply material properties for each instance
			materials[ix].Albedo->Bind(0);
			materials[ix].Specular->Bind(1);
			materials[ix].NewTexture->Bind(2);
			shader->SetUniform("u_Shininess", materials[ix].Shininess);
			shader->SetUniform("u_TextureMix", materials[ix].TextureMix);
			
			RenderVAO(shader, vaos[ix], camera, transforms[ix]);			
		}
		*/

		//materials[0].Albedo->Bind(0);
		//materials[0].Specular->Bind(1);
		//materials[0].NewTexture->Bind(2);
		//shader->SetUniform("u_Shininess", materials[0].Shininess);


		ballTransform->MoveLocal(moveDir * ballSpeed * dt);
		//ballTransform->SetLocalPosition(ballTransform->GetLocalPosition().x, moveDir.y * ballSpeed * dt, ballTransform->GetLocalPosition().z);
		//ballTransform->SetLocalPosition(moveDir.x * ballSpeed * dt, ballTransform->GetLocalPosition().y, ballTransform->GetLocalPosition().z);
		//ballEntity.transform.m_pos.y += moveDir.y * Speed * dt;
		//ballEntity.transform.m_pos.x += moveDir.x * Speed * dt;


		paddleMaterial.Albedo->Bind(0);
		paddleMaterial.Specular->Bind(1);
		shader->SetUniform("u_Shininess", paddleMaterial.Shininess);

		RenderVAO(shader, paddleVao, camera, paddleTransform);

		
		

		if (firstDigitScore == 0)
		{
			RenderVAO(shader, number0Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 1)
		{
			RenderVAO(shader, number1Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 2)
		{
			RenderVAO(shader, number2Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 3)
		{
			RenderVAO(shader, number3Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 4)
		{
			RenderVAO(shader, number4Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 5)
		{
			RenderVAO(shader, number5Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 6)
		{
			RenderVAO(shader, number6Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 7)
		{
			RenderVAO(shader, number7Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 8)
		{
			RenderVAO(shader, number8Vao, camera, firstDigitTransform);
		}
		else if (firstDigitScore == 9)
		{
			RenderVAO(shader, number9Vao, camera, firstDigitTransform);
		}

		if (secondDigitScore == 0)
		{
			RenderVAO(shader, number0Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 1)
		{
			RenderVAO(shader, number1Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 2)
		{
			RenderVAO(shader, number2Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 3)
		{
			RenderVAO(shader, number3Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 4)
		{
			RenderVAO(shader, number4Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 5)
		{
			RenderVAO(shader, number5Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 6)
		{
			RenderVAO(shader, number6Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 7)
		{
			RenderVAO(shader, number7Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 8)
		{
			RenderVAO(shader, number8Vao, camera, secondDigitTransform);
		}
		else if (secondDigitScore == 9)
		{
			RenderVAO(shader, number9Vao, camera, secondDigitTransform);
		}


		ballMaterial.Albedo->Bind(0);
		ballMaterial.Specular->Bind(1);
		shader->SetUniform("u_Shininess", ballMaterial.Shininess);

		RenderVAO(shader, ballVao, camera, ballTransform);

		

		if (lives == 3)
		{
			RenderVAO(shader, ballVao, camera, ball2Transform);
			RenderVAO(shader, ballVao, camera, ball3Transform);
			RenderVAO(shader, ballVao, camera, ball4Transform);
		}
		else if (lives == 2)
		{
			RenderVAO(shader, ballVao, camera, ball2Transform);
			RenderVAO(shader, ballVao, camera, ball3Transform);
		}
		else if (lives == 1)
		{
			RenderVAO(shader, ballVao, camera, ball2Transform);
		}
		else
		{
			RenderVAO(shader, loseVao, camera, loseTransform);
			moveDir = glm::vec3(0.0f, 0.0f, 0.0f);
			ballTransform->SetLocalPosition(1000.0f, 0.0f, 0.0f);
		}


		if (secondDigitScore == 5 && firstDigitScore == 4)
		{
			RenderVAO(shader, winVao, camera, winTransform);
			moveDir = glm::vec3(0.0f, 0.0f, 0.0f);
			ballTransform->SetLocalPosition(1000.0f, 0.0f, 0.0f);
		}



		boundaryMaterial.Albedo->Bind(0);
		boundaryMaterial.Specular->Bind(1);
		shader->SetUniform("u_Shininess", boundaryMaterial.Shininess);

		RenderVAO(shader, borderVao, camera, borderTransform);

		
		//Background Render
		backMaterial.Albedo->Bind(0);
		backMaterial.Specular->Bind(1);
		RenderVAO(shader, backVao, camera, backTransform);


		for (int i = 0; i < 54; i++)
		{
			if (BrickHealth[i] == 2)
			{
				baseBrickMaterial.Albedo->Bind(0);
				baseBrickMaterial.Specular->Bind(1);
				shader->SetUniform("u_Shininess", baseBrickMaterial.Shininess);

				RenderVAO(shader, brickVao, camera, brickTransform[i]);
			}
			else if (BrickHealth[i] < 2)
			{
				hitBrickMaterial.Albedo->Bind(0);
				hitBrickMaterial.Specular->Bind(1);
				shader->SetUniform("u_Shininess", hitBrickMaterial.Shininess);

				RenderVAO(shader, brickVao, camera, brickTransform[i]);
			}
		}

		/*
		if (ballTransform->GetLocalPosition().x <= paddleTransform->GetLocalPosition().x + 1.0f && ballTransform->GetLocalPosition().x >
			paddleTransform->GetLocalPosition().x&& ballTransform->GetLocalPosition().z >= (paddleTransform->GetLocalPosition().z - 1.0f) &&
			(ballTransform->GetLocalPosition().z <= paddleTransform->GetLocalPosition().z + 1.0f))
		{
			moveDir.z = moveDir.z * (-1.0f);
			moveDir.x = (1.0f);
		}

		if (ballTransform->GetLocalPosition().x >= paddleTransform->GetLocalPosition().x - 1.0f && ballTransform->GetLocalPosition().x <=
			paddleTransform->GetLocalPosition().x && ballTransform->GetLocalPosition().z >= (paddleTransform->GetLocalPosition().z - 1.0f) &&
			(ballTransform->GetLocalPosition().z <= paddleTransform->GetLocalPosition().z + 1.0f))
		{
			moveDir.z = moveDir.z * (-1.0f);
			moveDir.x = (-1.0f);
		}
		*/

		//Collisions with Ball and Paddle
		if (ballTransform->GetLocalPosition().x <= paddleTransform->GetLocalPosition().x + 0.95f && ballTransform->GetLocalPosition().x >
			paddleTransform->GetLocalPosition().x && ballTransform->GetLocalPosition().z >= (paddleTransform->GetLocalPosition().z - 0.2f) &&
			(ballTransform->GetLocalPosition().z <= paddleTransform->GetLocalPosition().z + 0.2f))
		{
			moveDir.z = moveDir.z * (-1.0f);
			moveDir.x = (1.0f);
		}
		if (ballTransform->GetLocalPosition().x >= paddleTransform->GetLocalPosition().x - 0.95f && ballTransform->GetLocalPosition().x <=
			paddleTransform->GetLocalPosition().x && ballTransform->GetLocalPosition().z >= (paddleTransform->GetLocalPosition().z - 0.2f) &&
			(ballTransform->GetLocalPosition().z <= paddleTransform->GetLocalPosition().z + 0.2f))
		{
			moveDir.z = moveDir.z * (-1.0f);
			moveDir.x = (-1.0f);
		}

		
		//Collision with Bricks
		for (int i = 0; i < 54; i++)
		{
			if (ballTransform->GetLocalPosition().x - 0.1f < brickTransform[i]->GetLocalPosition().x + 0.34f &&
				ballTransform->GetLocalPosition().x + 0.1f >= brickTransform[i]->GetLocalPosition().x - 0.44f &&
				ballTransform->GetLocalPosition().z - 0.1f < brickTransform[i]->GetLocalPosition().z + 0.2f &&
				ballTransform->GetLocalPosition().z + 0.1f >= brickTransform[i]->GetLocalPosition().z - 0.1f)
			{
				isHit = true;


				//Check Move Direction to see if positive or negative on the z axis and decide whehter or not to send the ball up/down/left/right
				if (isHit == true)
				{
					/*
					if (ballTransform->GetLocalPosition().x - 0.1f < brickTransform[i]->GetLocalPosition().x + 0.34f &&
						ballTransform->GetLocalPosition().z - 0.1f < brickTransform[i]->GetLocalPosition().z + 0.2f &&
						ballTransform->GetLocalPosition().z + 0.1f >= brickTransform[i]->GetLocalPosition().z - 0.1f)
					{
						moveDir.x = moveDir.x * (-1.0f);
					}

					if (ballTransform->GetLocalPosition().x + 0.1f > brickTransform[i]->GetLocalPosition().x - 0.44f &&
						ballTransform->GetLocalPosition().z - 0.1f < brickTransform[i]->GetLocalPosition().z + 0.2f &&
						ballTransform->GetLocalPosition().z + 0.1f >= brickTransform[i]->GetLocalPosition().z - 0.1f)
					{
						moveDir.x = moveDir.x * (-1.0f);
					}
					*/

					BrickHealth[i] --;


					if (BrickHealth[i] == 0)
					{
						moveDir.z = moveDir.z * (-1.0f);
						brickTransform[i]->SetLocalPosition(1000.f, 0.0f, 0.0f);
						firstDigitScore++;

						if (firstDigitScore == 10)
						{
							firstDigitScore = 0;
							secondDigitScore++;
						}
					}
					else
					{
						moveDir.z = moveDir.z * (-1.0f);
					}
					isHit = false;
				}
				



				//moveDir.z = moveDir.z * (-1.0f);
				//brickHits[i]++;

				//if (brickHits[i] == 2)
				//{
					//brickTransform[i]->SetLocalPosition(1000.f, 0.0f, 0.0f);

				//}

				//moveDir.z = moveDir.z * (-1.0f);
				//brickTransform[i]->SetLocalPosition(1000.f, 0.0f, 0.0f);
			}
			

		}






		//Collisions with Ball and Borders
		if (ballTransform->GetLocalPosition().x >= 4.4f || ballTransform->GetLocalPosition().x <= -4.7f)
		{
			moveDir.x = moveDir.x * (-1.0f);
		}
		if (ballTransform->GetLocalPosition().z >= 4.7f)
		{
			moveDir.z = moveDir.z * (-1.0f);
		}
		if (ballTransform->GetLocalPosition().z <= -4.0f)
		{
			ballTransform->SetLocalPosition(0.0f, 0.0f, 0.0f);

			// Lose life
			lives--;
		}


		



		//std::cout << "Brick Position X: " << brickTransform[2]->GetLocalPosition().x << std::endl;
		//std::cout << "Brick Position Y: " << brickTransform[2]->GetLocalPosition().y << std::endl;
		//std::cout << "Brick Position Z: " << brickTransform[2]->GetLocalPosition().z << "\n" << std::endl;

		//std::cout << "Paddle Position X: " << paddleTransform->GetLocalPosition().x << std::endl;
		//std::cout << "Paddle Position Y: " << paddleTransform->GetLocalPosition().y << std::endl;
		//std::cout << "Paddle Position Z: " << paddleTransform->GetLocalPosition().z << "\n" << std::endl;

		//std::cout << "Y: " << ballTransform->GetLocalPosition().y << std::endl;
		//std::cout << "Z: " << ballTransform->GetLocalPosition().z << std::endl;

		//std::cout << currentScore << std::endl;

		RenderImGui();

		glfwSwapBuffers(window);
		lastFrame = thisFrame;
	}

	ShutdownImGui();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}