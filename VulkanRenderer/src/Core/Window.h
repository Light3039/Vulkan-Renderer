#pragma once

#include "Base.h"

struct GLFWwindow;

class Window
{
private:
	GLFWwindow* m_WindowHandle;

	class Renderer* m_Pipeline;
public:
	Window();
	~Window();

	inline void RegisterPipeline(class Renderer* pipeline) { m_Pipeline = pipeline; }

	inline GLFWwindow* GetHandle() { return m_WindowHandle; }

	// #todo
	bool IsClosed() const;

private:
	void BindGlfwEvents();
};