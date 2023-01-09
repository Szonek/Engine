#include <iostream>
#include <format>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#include <GLFW/glfw3.h>


int main()
{

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	auto window = glfwCreateWindow(800, 600, "LearnOpenGL", nullptr, nullptr);
	if (!window)
	{
		std::cout << "Failed to create GLFW window\n";
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	const auto gl_version = gladLoadGL(glfwGetProcAddress);
	if (gl_version == 0)
	{
		std::cout << "Failed to initialize GLAD\n";
		return -1;
	}
	else
	{
		std::cout << std::format("Sucesfully loaded Opengl ver: {0}, {1}.\n",
			GLAD_VERSION_MAJOR(gl_version), GLAD_VERSION_MINOR(gl_version));
	}
	return 0;
}