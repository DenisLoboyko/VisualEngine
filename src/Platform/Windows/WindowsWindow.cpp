#include "WindowsWindow.h"
#include <iostream>

namespace VE {

    Window* Window::Create(const WindowProps& props)
    {
        return new WindowsWindow(props);
    }

    WindowsWindow::WindowsWindow(const WindowProps& props)
    {
        Init(props);
    }

    WindowsWindow::~WindowsWindow()
    {
        Shutdown();
    }

    void WindowsWindow::Init(const WindowProps& props)
    {
        m_Data.Title  = props.Title;
        m_Data.Width  = props.Width;
        m_Data.Height = props.Height;

        if (!glfwInit())
        {
            std::cerr << "Failed to init GLFW!\n";
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_Window = glfwCreateWindow(
            m_Data.Width,
            m_Data.Height,
            m_Data.Title.c_str(),
            nullptr, nullptr);

    
        if (!m_Window)
        {
            std::cerr << "Failed to create window!\n";
            return;
        }

        glfwMaximizeWindow(m_Window);
        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to init GLAD!\n";
            return;
        }

        std::cout << "Window created: " << m_Data.Title
                  << " (" << m_Data.Width << "x" << m_Data.Height << ")\n";
    }

    void WindowsWindow::Shutdown()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void WindowsWindow::OnUpdate()
    {
        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }

    bool WindowsWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }
}