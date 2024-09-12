#pragma once

#include <GLFW/glfw3.h>


class Window
{
public:
	Window(uint32_t _width, uint32_t _height, const std::string& title);
	~Window();
	bool shouldClose();
	void setTitle(const std::string& _title);
	void setSize(const uint32_t _width, const uint32_t _height);
	void setPos(const int32_t _x, const int32_t _y);
	const void* getHandle() const { return m_handle; };
	const uint32_t getWidth() const { return m_width; };
	const uint32_t getHeight() const { return m_height; };
	//void update();
	void setSizeCallback(std::function<void(int32_t, int32_t)> func);
	//void setKeyCallback(std::function<void(Key::Enum, Action::Enum)> func);
	void setScrollCallback(std::function<void(double dx, double dy)> func);
	void setMouseButtonCallback(std::function<void(int32_t, int32_t, int32_t, double, double)> func);
	void setMouseMoveCallback(std::function<void(double, double)> func);
	//void setDropCallback(std::function<void(const char** files, int numFiles)> func);

private:
	void* m_handle{ nullptr };
	//void* m_context{ nullptr };
	uint32_t    m_width{ 0 };
	uint32_t    m_height{ 0 };

public:

	class Impl;
	Impl* m_impl{ nullptr };
};

class Window::Impl final
{
public:
	std::function<void(int32_t, int32_t)> m_sizeCallback{ nullptr };
	//std::function<void(UI::Key::Enum, UI::Action::Enum)> m_keyCallback{ nullptr };
	std::function<void(double dx, double dy)> m_scrollCallback{ nullptr };
	std::function<void(int32_t, int32_t, int32_t, double, double)> m_mouseButtonCallback{ nullptr };
	std::function<void(double, double)> m_mouseMoveCallback{ nullptr };
	//std::function<void(const char**, int)> m_dropCallback{ nullptr };
	void windowSizeCb(GLFWwindow* window, int32_t _w, int32_t _h);
	//void keyCb(GLFWwindow* window, int32_t _key, int32_t _scancode, int32_t _action, int32_t _mods);
	void scrollCb(GLFWwindow* window, double _dx, double _dy);
	void mouseButtonCb(GLFWwindow* window, int32_t _button, int32_t _action, int32_t _mods);
	void mouseMoveCb(GLFWwindow* window, double mx, double my);
//	void dropCb(GLFWwindow* window, const char** files, int numFiles);
};

void Window::Impl::windowSizeCb(GLFWwindow* window, int32_t _w, int32_t _h) {
	if (m_sizeCallback)
		m_sizeCallback(_w, _h);
}


void Window::Impl::scrollCb(GLFWwindow* window, double _dx, double _dy) {
	if (m_scrollCallback)
		m_scrollCallback(_dx, _dy);
}

void Window::Impl::mouseButtonCb(GLFWwindow* window, int32_t _button, int32_t _action, int32_t _mods) {
	if (m_mouseButtonCallback)
	{
		double mouse_x;
		double mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		m_mouseButtonCallback(_button, _action, _mods, mouse_x, mouse_y);
	}
}

void Window::Impl::mouseMoveCb(GLFWwindow* window, double mx, double my) {
	if (m_mouseMoveCallback)
		m_mouseMoveCallback(mx, my);
}

Window::Window(uint32_t _width, uint32_t _height, const std::string& _title) :
	m_width(_width),
	m_height(_height)
{
	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_handle = glfwCreateWindow(_width, _height, _title.c_str(), NULL, NULL);


	if (!m_handle)
	{
		std::cerr << "Could not open window!" << std::endl;
	}

	m_impl = new Impl();
}

Window::~Window() {

	delete m_impl;

	glfwDestroyWindow((GLFWwindow*)m_handle);
}

bool Window::shouldClose()
{
	return !(NULL != m_handle
		&& !glfwWindowShouldClose((GLFWwindow*)m_handle));
}

void Window::setTitle(const std::string& title) {
	glfwSetWindowTitle((GLFWwindow*)m_handle, title.c_str());
}

void Window::setSize(const uint32_t width, const uint32_t height) {
	glfwSetWindowSize((GLFWwindow*)m_handle, width, height);
}

void Window::setPos(const int32_t x, const int32_t y) {
	glfwSetWindowPos((GLFWwindow*)m_handle, x, y);
}

void Window::setSizeCallback(std::function<void(int32_t, int32_t)> func) {

	glfwSetWindowUserPointer((GLFWwindow*)m_handle, m_impl);

	auto callback = [](GLFWwindow* window, int w, int h)
	{
		static_cast<Impl*>(glfwGetWindowUserPointer(window))->windowSizeCb(window, w, h);
	};

	glfwSetWindowSizeCallback((GLFWwindow*)m_handle, callback);

	m_impl->m_sizeCallback = func;
}

void Window::setScrollCallback(std::function<void(double dx, double dy)> func) {

	glfwSetWindowUserPointer((GLFWwindow*)m_handle, m_impl);

	auto callback = [](GLFWwindow* window, double _dx, double _dy)
	{
		static_cast<Impl*>(glfwGetWindowUserPointer(window))->scrollCb(window, _dx, _dy);
	};

	glfwSetScrollCallback((GLFWwindow*)m_handle, callback);

	m_impl->m_scrollCallback = func;

}

void Window::setMouseButtonCallback(std::function<void(int32_t, int32_t, int32_t, double, double)> func) {

	glfwSetWindowUserPointer((GLFWwindow*)m_handle, m_impl);

	auto callback = [](GLFWwindow* window, int32_t _button, int32_t _action, int32_t _mods)
	{
		static_cast<Impl*>(glfwGetWindowUserPointer(window))->mouseButtonCb(window, _button, _action, _mods);
	};

	glfwSetMouseButtonCallback((GLFWwindow*)m_handle, callback);

	m_impl->m_mouseButtonCallback = func;
}

void Window::setMouseMoveCallback(std::function<void(double, double)> func) {

	glfwSetWindowUserPointer((GLFWwindow*)m_handle, m_impl);

	auto callback = [](GLFWwindow* window, double mx, double my)
	{
		static_cast<Impl*>(glfwGetWindowUserPointer(window))->mouseMoveCb(window, mx, my);
	};

	glfwSetCursorPosCallback((GLFWwindow*)m_handle, callback);

	m_impl->m_mouseMoveCallback = func;
}


class App
{
public:
	App() = default;
	~App() = default;
	virtual void init() = 0;
	virtual void update() = 0;
	virtual void finish() = 0;
	virtual bool isRunning() = 0;

protected:
	std::string m_dataPath = DATA_DIR;
	Window* m_mainWindow;
	int m_winWidth = 1280;
	int m_winHeight = 720;
};