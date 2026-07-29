#pragma once
#include <string>
#include <functional>

namespace VE {

    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(
            const std::string& title = "VisualEngine",
            unsigned int width  = 1280,
            unsigned int height = 720)
            : Title(title), Width(width), Height(height) {}
    };

    class Window
    {
    public:
        virtual ~Window() = default;

        virtual void OnUpdate() = 0;
        virtual unsigned int GetWidth()  const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual void* GetNativeWindow()  const = 0;
        virtual bool  ShouldClose()      const = 0;

        static Window* Create(const WindowProps& props = WindowProps());
    };
}