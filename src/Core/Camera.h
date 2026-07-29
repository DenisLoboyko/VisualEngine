#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VE {

    class Camera
    {
    public:
        glm::vec3 Position;
        glm::vec3 Front;
        glm::vec3 Up;

        float Yaw   = -90.0f;
        float Pitch =   0.0f;
        float Speed =   5.0f;
        float Sensitivity = 0.1f;
        float Fov   =  45.0f;

        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f))
            : Position(position),
              Front(glm::vec3(0.0f, 0.0f, -1.0f)),
              Up(glm::vec3(0.0f, 1.0f, 0.0f))
        {}

        glm::mat4 GetViewMatrix() const
        {
            return glm::lookAt(Position, Position + Front, Up);
        }

        glm::mat4 GetProjectionMatrix(float aspect) const
        {
            return glm::perspective(glm::radians(Fov), aspect, 0.1f, 100.0f);
        }

        void ProcessKeyboard(int key, float delta)
        {
            float v = Speed * delta;
            if (key == 0) Position += v * Front;           // W
            if (key == 1) Position -= v * Front;           // S
            if (key == 2) Position -= glm::normalize(glm::cross(Front, Up)) * v; // A
            if (key == 3) Position += glm::normalize(glm::cross(Front, Up)) * v; // D
        }

        void ProcessMouse(float xoff, float yoff)
        {
            Yaw   += xoff * Sensitivity;
            Pitch += yoff * Sensitivity;
            if (Pitch >  89.0f) Pitch =  89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
            UpdateVectors();
        }

        // Пересчитать Front из текущих Yaw/Pitch — вызывать после
        // ручного изменения Yaw/Pitch (например, follow-камера на игроке)
        void UpdateVectors()
        {
            glm::vec3 front;
            front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
            front.y = sin(glm::radians(Pitch));
            front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
            Front = glm::normalize(front);
        }
    };
}