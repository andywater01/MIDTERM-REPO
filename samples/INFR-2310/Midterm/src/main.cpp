#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "VertexArrayObject.h"
#include "Shader.h"
#include "Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "InputHelpers.h"
#include "MeshBuilder.h"
#include "MeshFactory.h"
#include "Entity.h"
//Includes the obj loader header
#include "ObjLoader.h"
//Includes the not-obj header
#include "NotObjLoader.h"
#include "VertexTypes.h"

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
	window = glfwCreateWindow(800, 800, "INFR1350U", nullptr, nullptr);
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



void setBricks(std::vector <glm::mat4> transform)
{
	glm::vec3 brickPos = glm::vec3(3.0f, -9.0f, 0.0f);

	int brickCounter = 0;

	for (int y = 0; y < 3; y++)
	{
		brickPos += glm::vec3(0.0f, 3.5f, 0.0f);

		for (int x = 0; x < 3; x++)
		{
			brickPos += glm::vec3(3.5f, 0.0f, 0.0f);

			transform[brickCounter] = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));

			transform[brickCounter] = glm::translate(transform[brickCounter], brickPos);

			brickCounter++;
		}
	}
}



void createBricks(Shader::sptr shader, VertexArrayObject::sptr VAO, std::vector <glm::mat4> transform)
{
	for (int i = 0; i < 54; i++)
	{
		shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform[i]);
		shader->SetUniformMatrix("u_Model", transform[i]);
		shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform[i]));
		VAO->Render();
	}
}

//Ball movement function
void ballMovement(glm::mat4 transform, float dt)
{
	glm::vec3 moveDir = glm::vec3(-1.0f, 0.0f, 0.0f);
	//glm::vec3 ballPos = glm::vec3();

	transform = glm::translate(transform, moveDir * dt);
}



/*
function intersect(sphere, box) {
	// get box closest point to sphere center by clamping
	var x = Math.max(box.minX, Math.min(sphere.x, box.maxX));
	var y = Math.max(box.minY, Math.min(sphere.y, box.maxY));
	var z = Math.max(box.minZ, Math.min(sphere.z, box.maxZ));

	// this is the same as isPointInsideSphere
	var distance = Math.sqrt((x - sphere.x) * (x - sphere.x) +
		(y - sphere.y) * (y - sphere.y) +
		(z - sphere.z) * (z - sphere.z));

	return distance < sphere.radius;
}
*/

void checkCollision(Entity &ball, Entity &gameObject, float length, float width)
{
	if ((ball.transform.m_pos.x >= gameObject.transform.m_pos.x - length && ball.transform.m_pos.x <= gameObject.transform.m_pos.x + length) 
		&& (ball.transform.m_pos.y <= gameObject.transform.m_pos.y - width && ball.transform.m_pos.y >= gameObject.transform.m_pos.y + width))
	{
		printf("HIT!");
	}
	/*if (ball.transform.m_pos.y <= 9.5)
	{
		std::cout << (ball.transform.m_pos.y) << std::endl;
	}*/
}

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


	
	

	static const float points[] = {
		-0.5f, -0.5f, 0.1f,
		 0.5f, -0.5f, 0.1f,
		-0.5f,  0.5f, 0.1f
	};

	static const float colors[] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	};

	//VBO - Vertex buffer object
	VertexBuffer::sptr posVbo = VertexBuffer::Create();
	posVbo->LoadData(points, 9);

	VertexBuffer::sptr color_vbo = VertexBuffer::Create();
	color_vbo->LoadData(colors, 9);

	VertexArrayObject::sptr vao = VertexArrayObject::Create();
	vao->AddVertexBuffer(posVbo, {
		BufferAttribute(0, 3, GL_FLOAT, false, 0, NULL)
	});
	vao->AddVertexBuffer(color_vbo, {
		BufferAttribute(1, 3, GL_FLOAT, false, 0, NULL)
	});

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

	/////////////// VertexPosNormTex Stuff ///////////////

	/*std::vector<VertexPosNormTex> verticies;
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> uvs;*/

	// f p1/t1/n1 p2/t2/n2 p3/t3/n3
	/*VertexPosNormTex v1 = VertexPosNormTex(positions[vertices[i] - 1], uvs [t1 - 1], normals[n1 - 1]);
	VertexPosNormTex v2 = VertexPosNormTex(positions[p2 - 1], uvs [t2 - 1], normals[n2 - 1]);
	VertexPosNormTex v3 = VertexPosNormTex(positions[p3 - 1], uvs [t3 - 1], normals[n3 - 1]);*/

	/*verticies.push_back(v1);
	verticies.push_back(v2);
	verticies.push_back(v3);*/



	////////////// NEW STUFF
	
	// We'll use the provided mesh builder to build a new mesh with a few elements
	MeshBuilder<VertexPosNormTexCol> builder = MeshBuilder<VertexPosNormTexCol>();
	MeshFactory::AddPlane(builder, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0, 0.0f), glm::vec2(100.0f, 100.0f), glm::vec4(1.0f));
	MeshFactory::AddCube(builder, glm::vec3(-2.0f, 0.0f, 0.5f), glm::vec3(1.0f, 2.0f, 1.0f), glm::vec3(0.0f, 0.0f, 45.0f), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
	MeshFactory::AddIcoSphere(builder, glm::vec3(0.0f, 0.f, 1.0f), 0.5f, 2, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	MeshFactory::AddUvSphere(builder, glm::vec3(1.0f, 0.f, 1.0f), 0.5f, 2, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
	VertexArrayObject::sptr vao3 = builder.Bake();

	// We'll be implementing a loader that works a bit like an OBJ loader to learn how to read files, we'll
	// load an exact copy of the mesh created above
	VertexArrayObject::sptr vao4 = NotObjLoader::LoadFromFile("Sample.notobj");

	//Testobj.txt
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> uvs;

	VertexArrayObject::sptr paddleVAO = nullptr;
	bool loader = ObjLoader::LoadFromFile("Paddle.obj", positions, uvs, normals);

	paddleVAO = VertexArrayObject::Create();
	VertexBuffer::sptr vertices = VertexBuffer::Create();
	vertices->LoadData(positions.data(), positions.size());

	VertexBuffer::sptr _normals = VertexBuffer::Create();
	_normals->LoadData(normals.data(), normals.size());

	paddleVAO = VertexArrayObject::Create();

	paddleVAO->AddVertexBuffer(vertices, {
		BufferAttribute(0, 3, GL_FLOAT, false, 0, NULL)
		});
	paddleVAO->AddVertexBuffer(_normals, {
		BufferAttribute(2, 3, GL_FLOAT, false, 0, NULL)
		});


	//Ball.obj
	std::vector<glm::vec3> positions_ball;
	std::vector<glm::vec3> normals_ball;
	std::vector<glm::vec2> uvs_ball;

	VertexArrayObject::sptr ballVAO = nullptr;
	bool ball_loader = ObjLoader::LoadFromFile("Ball.obj", positions_ball, uvs_ball, normals_ball);

	ballVAO = VertexArrayObject::Create();
	VertexBuffer::sptr ball_vertices = VertexBuffer::Create();
	ball_vertices->LoadData(positions_ball.data(), positions_ball.size());

	VertexBuffer::sptr ball_normals = VertexBuffer::Create();
	ball_normals->LoadData(normals_ball.data(), normals_ball.size());

	ballVAO = VertexArrayObject::Create();

	ballVAO->AddVertexBuffer(ball_vertices, {
		BufferAttribute(0, 3, GL_FLOAT, false, 0, NULL)
		});
	ballVAO->AddVertexBuffer(ball_normals, {
		BufferAttribute(2, 3, GL_FLOAT, false, 0, NULL)
		});

	
	//brick.obj
	std::vector<glm::vec3> positions_brick;
	std::vector<glm::vec3> normals_brick;
	std::vector<glm::vec2> uvs_brick;

	VertexArrayObject::sptr brickVAO = nullptr;
	bool brick_loader = ObjLoader::LoadFromFile("brick1.obj", positions_brick, uvs_brick, normals_brick);

	brickVAO = VertexArrayObject::Create();
	VertexBuffer::sptr brick_vertices = VertexBuffer::Create();
	brick_vertices->LoadData(positions_brick.data(), positions_brick.size());

	VertexBuffer::sptr brick_normals = VertexBuffer::Create();
	brick_normals->LoadData(normals_brick.data(), normals_brick.size());

	brickVAO = VertexArrayObject::Create();

	brickVAO->AddVertexBuffer(brick_vertices, {
		BufferAttribute(0, 3, GL_FLOAT, false, 0, NULL)
		});
	brickVAO->AddVertexBuffer(brick_normals, {
		BufferAttribute(2, 3, GL_FLOAT, false, 0, NULL)
		});
	


	// Load our shaders
	Shader::sptr shader = Shader::Create();
	shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
	shader->LoadShaderPartFromFile("shaders/frag_blinn_phong.glsl", GL_FRAGMENT_SHADER);  
	shader->Link();  

	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
	glm::vec3 lightCol = glm::vec3(1.0f, 30.0f, 1.0f);
	float     lightAmbientPow = 0.05f;
	float     lightSpecularPow = 10.0f;
	glm::vec3 ambientCol = glm::vec3(30.0f);
	float     ambientPow = 1.0f;
	float     shininess = 1.2f;
	// These are our application / scene level uniforms that don't necessarily update
	// every frame
	shader->SetUniform("u_LightPos", lightPos);
	shader->SetUniform("inColor", glm::vec3(1.0f));
	shader->SetUniform("u_LightCol", lightCol);
	shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
	shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
	shader->SetUniform("u_AmbientCol", ambientCol);
	shader->SetUniform("u_AmbientStrength", ambientPow);
	shader->SetUniform("u_Shininess", shininess);

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
			if (ImGui::SliderFloat3("Light Pos", glm::value_ptr(lightPos), -10.0f, 10.0f)) {
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

	glm::mat4 transform = glm::mat4(1.0f);
	glm::mat4 transform2 = glm::mat4(1.0f);
	glm::mat4 transform3 = glm::mat4(1.0f);
	glm::mat4 transform4 = glm::mat4(1.2f);

	std::vector <glm::mat4> brickTransform;

	//Creating the bricks
	for (int i = 0; i < 54; i++)
	{
		brickTransform.push_back(glm::mat4(1.0f));
	}

	//setBricks(brickTransform);

	//Paddle Entity
	auto paddleEntity = Entity::Create();

	paddleEntity.transform.m_rotation = glm::rotate(paddleEntity.transform.RecomputeGlobal(), glm::radians(90.0f), glm::vec3(0, 1, 0));
	paddleEntity.transform.m_pos = glm::vec3(0.0f, 9.5f, 0.0f);

	transform = paddleEntity.transform.RecomputeGlobal();


	//Ball Entity
	auto ballEntity = Entity::Create();
	ballEntity.transform.m_pos = glm::vec3(0.0f, -2.0f, 0.0f);
	transform2 = ballEntity.transform.RecomputeGlobal();


	
	//paddleEntity.Get<Transform>().m_pos.x
	
	//Rotations
	//transform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));

	//Translations
	//transform = glm::translate(transform, glm::vec3(0.0f, 9.5f, 0.0f));
	//transform2 = glm::translate(transform2, glm::vec3(0.0f, 1.0f, 0.0f));

	transform3 = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));
	transform3 = glm::translate(transform3, glm::vec3(3.0f, -9.0f, 0.0f));

	//brickTransform[4] = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.0f, 0.0f));

	int brickCounter = 0;
	float xMove = 0.0f;
	float yMove = -2.0f;

	std::vector<float> brickMinX, brickMinY, brickMaxX, brickMaxY;

	
	
	for (int y = 0; y < 6; y++)
	{
		yMove += 2.0f;
		for (int x = 0; x < 9; x++)
		{
			brickTransform[brickCounter] = glm::translate(brickTransform[brickCounter], glm::vec3(14.0f + xMove, -16.0f + yMove, 0.0f));
			xMove += -3.5f;
			brickCounter++;
		}
		xMove = 0.0f;
	}
	
	/*
	for (int y = 0; y < 6; y++)
	{
		yMove += 2.0f;
		for (int x = 0; x < 9; x++)
		{
			brickTransform[brickCounter] = glm::translate(brickTransform[brickCounter], glm::vec3(14.0f + xMove, -16.0f + yMove, 0.0f));
			xMove += -3.5f;
			brickCounter++;
		}
		xMove = 0.0f;
	}
	*/
	

	for (int i = 0; i < 54; i++)
	{
		brickTransform[i] = glm::rotate(brickTransform[i], glm::radians(90.0f), glm::vec3(0, 1, 0));
	}

	

	camera = Camera::Create();



	camera->SetPosition(glm::vec3(0, 1, 10)); // Set initial position
	camera->SetUp(glm::vec3(0, 0, 1)); // Use a z-up coordinate system
	camera->LookAt(glm::vec3(0.0f)); // Look at center of the screen
	camera->SetFovDegrees(120.0f); // Set an initial FOV

	//Ball Variables
	float ballSpeed = 1.0f;
	glm::vec3 moveDir = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 ballPos = glm::vec3();

	// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
	// how this is implemented. Note that the ampersand here is capturing the variables within
	// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
	// use std::bind
	bool is_wireframe = false;
	KeyPressWatcher tKeyWatcher = KeyPressWatcher(GLFW_KEY_T, [&]() {
		is_wireframe = !is_wireframe;
		glPolygonMode(GL_FRONT, is_wireframe ? GL_LINE : GL_FILL);
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

		// We need to poll our key watchers so they can do their logic with the GLFW state
		tKeyWatcher.Poll(window);

		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
			transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 10.0f) * dt);
			paddleEntity.transform.m_pos.x += (0.0f, 0.0f, 10.0f) * dt;
		}
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
			transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, -10.0f) * dt);
			paddleEntity.transform.m_pos.x += (0.0f, 0.0f, -10.0f) * dt;
			//brickTransform[2] = glm::translate(brickTransform[2], glm::vec3(0.0f, 0.0f, -10.0f) * dt);
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
		else if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
		{
			
		}
				
		//transform = glm::rotate_slow(glm::mat4(1.0f), static_cast<float>(thisFrame), glm::vec3(0, 1, 0));
		//transform2 = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.0f, glm::sin(static_cast<float>(thisFrame))));

		//transform4 =  glm::translate(glm::mat4(1.0f), glm::vec3(3, 0.0f, glm::sin(static_cast<float>(thisFrame))));
		//transform4 = glm::rotate_slow(glm::mat4(1.0f), static_cast<float>(thisFrame), glm::vec3(0, -1, 0));
		
		//transform4 = transform4 * glm::translate(glm::mat4(1.0f), glm::vec3(3, 0.0f, glm::sin(static_cast<float>(thisFrame))));
		
		glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		

		shader->Bind();
		// These are the uniforms that update only once per frame
		//shader->SetUniformMatrix("u_View", camera->GetView());
		shader->SetUniform("u_CamPos", camera->GetPosition());

		//paddleEntity.transform.RecomputeGlobal();
		// These uniforms update for every object we want to draw
		shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform);
		shader->SetUniformMatrix("u_Model", transform);
		shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform));
		paddleVAO->Render();

		

		//transform2 = glm::translate(transform2, moveDir * ballSpeed * dt);
		transform2 = glm::translate(transform2, moveDir * dt);
		ballEntity.transform.m_pos.y += moveDir.y * dt;
		
		// Checks balls distance to paddle height
		if (ballEntity.transform.m_pos.y >= (paddleEntity.transform.m_pos.y - 1.0f) && (ballEntity.transform.m_pos.x >= (paddleEntity.transform.m_pos.x) - 2 && (ballEntity.transform.m_pos.x <= (paddleEntity.transform.m_pos.x + 2))))
		{
			moveDir = moveDir * (-1.0f);
		}

		shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform2);
		shader->SetUniformMatrix("u_Model", transform2);
		shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform2));
		//ballMovement(transform2, dt);
		ballVAO->Render();
		

		//shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform3);
		//shader->SetUniformMatrix("u_Model", transform3);
		//shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform3));
		//brickVAO->Render();

		//shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * brickTransform[1]);
		//shader->SetUniformMatrix("u_Model", brickTransform[1]);
		//shader->SetUniformMatrix("u_ModelRotation", glm::mat3(brickTransform[1]));
		//brickVAO->Render();
		
		createBricks(shader, brickVAO, brickTransform);

		
		checkCollision(ballEntity, paddleEntity, 10.0f, 2.0f);

		RenderImGui();

		glfwSwapBuffers(window);
		lastFrame = thisFrame;
	}

	ShutdownImGui();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}