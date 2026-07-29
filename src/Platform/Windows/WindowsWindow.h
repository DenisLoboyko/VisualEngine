#pragma once
#include "Core/Window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace VE {

    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& props);
        virtual ~WindowsWindow();

        void OnUpdate() override;
        unsigned int GetWidth()  const override { return m_Data.Width; }
        unsigned int GetHeight() const override { return m_Data.Height; }
        void* GetNativeWindow()  const override { return m_Window; }
        bool  ShouldClose()      const override;

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        GLFWwindow* m_Window;

        struct WindowData
        {
            std::string  Title;
            unsigned int Width  = 1280;
            unsigned int Height = 720;
        };

        WindowData m_Data;
    };
}