#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Подключаем существующие классы движка
#include "../Core/Primitives.h"   // Mesh
#include "../Core/Shader.h"       // Shader
#include "../Core/Camera.h"       // Camera

namespace VE {

    // =========================================================
    //  TagComponent — имя объекта
    //  registry.AddComponent<TagComponent>(e, "Player");
    // =========================================================
    struct TagComponent
    {
        std::string Name = "Entity";
        bool        Active = true;

        TagComponent() = default;
        TagComponent(const std::string& name) : Name(name) {}
    };

    // =========================================================
    //  TransformComponent — позиция, поворот, масштаб
    //
    //  Пример:
    //    auto& t = registry.GetComponent<TransformComponent>(e);
    //    t.Position = { 0.0f, 1.0f, 0.0f };
    //    glm::mat4 model = t.GetMatrix();
    // =========================================================
    struct TransformComponent
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // Euler углы в градусах
        glm::vec3 Scale    = { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(glm::vec3 pos) : Position(pos) {}
        TransformComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
            : Position(pos), Rotation(rot), Scale(scale) {}

        // Собрать матрицу модели (TRS)
        glm::mat4 GetMatrix() const
        {
            glm::mat4 mat = glm::mat4(1.0f);
            mat = glm::translate(mat, Position);
            mat = glm::rotate(mat, glm::radians(Rotation.x), { 1, 0, 0 });
            mat = glm::rotate(mat, glm::radians(Rotation.y), { 0, 1, 0 });
            mat = glm::rotate(mat, glm::radians(Rotation.z), { 0, 0, 1 });
            mat = glm::scale(mat, Scale);
            return mat;
        }
    };

    // =========================================================
    //  MeshComponent — меш + цвет для рендера
    //
    //  Пример:
    //    auto& mc = registry.AddComponent<MeshComponent>(e);
    //    mc.MeshData = VE::CreateSphere(1.0f, 32, 32);
    //    mc.Color    = { 1.0f, 0.5f, 0.2f };
    // =========================================================
    struct MeshComponent
    {
        Mesh      MeshData;
        glm::vec3 Color     = { 1.0f, 1.0f, 1.0f };
        bool      Wireframe = false;
        bool      Visible   = true;

        MeshComponent() = default;
        MeshComponent(const Mesh& mesh, glm::vec3 color = { 1, 1, 1 })
            : MeshData(mesh), Color(color) {}
    };

    // =========================================================
    //  ModelComponent — загруженная 3D модель (assimp)
    //
    //  Пример:
    //    registry.AddComponent<ModelComponent>(e, "assets/models/car.obj");
    // =========================================================
    struct ModelComponent
    {
        std::string ModelPath;
        glm::vec3   Color   = { 1.0f, 1.0f, 1.0f };
        bool        Visible = true;

        ModelComponent() = default;
        ModelComponent(const std::string& path) : ModelPath(path) {}
    };

    // =========================================================
    //  ScriptComponent — Lua скрипт привязанный к объекту
    //
    //  Пример:
    //    registry.AddComponent<ScriptComponent>(e, "assets/scripts/player.lua");
    // =========================================================
    struct ScriptComponent
    {
        std::string ScriptPath;
        bool        Running = false;

        ScriptComponent() = default;
        ScriptComponent(const std::string& path) : ScriptPath(path) {}
    };

    // =========================================================
    //  LightComponent — источник света
    //
    //  Type: 0 = Point, 1 = Directional, 2 = Spot
    //
    //  Пример:
    //    auto& light = registry.AddComponent<LightComponent>(e);
    //    light.Color     = { 1.0f, 0.9f, 0.8f };
    //    light.Intensity = 2.0f;
    //    light.Type      = 0; // Point
    // =========================================================
    struct LightComponent
    {
        glm::vec3 Color     = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;
        float     Range     = 10.0f;
        int       Type      = 0; // 0=Point, 1=Directional, 2=Spot

        LightComponent() = default;
        LightComponent(glm::vec3 color, float intensity = 1.0f, int type = 0)
            : Color(color), Intensity(intensity), Type(type) {}
    };

    // =========================================================
    //  CameraComponent — камера привязанная к entity
    //
    //  Пример:
    //    registry.AddComponent<CameraComponent>(e, true); // isPrimary = true
    // =========================================================
    struct CameraComponent
    {
        float FOV       = 45.0f;
        float NearClip  = 0.1f;
        float FarClip   = 1000.0f;
        bool  IsPrimary = false; // Главная камера сцены

        CameraComponent() = default;
        CameraComponent(bool primary) : IsPrimary(primary) {}
    };

    // =========================================================
    //  Физика вынесена в отдельные файлы:
    //    src/Physics/PhysicsMaterial.h
    //    src/Physics/RigidbodyComponent.h
    //    src/Physics/ColliderComponent.h
    //    src/Physics/Physics.h          <- главный синглтон
    //
    //  Пример:
    //    #include "../Physics/Physics.h"
    //    #include "../Physics/RigidbodyComponent.h"
    //    #include "../Physics/ColliderComponent.h"
    //
    //    auto& rb  = registry.AddComponent<RigidbodyComponent>(e);
    //    auto& col = registry.AddComponent<ColliderComponent>(e,
    //                    ColliderComponent::Box({ 0.5f, 0.5f, 0.5f }));
    //
    //    // В game loop:
    //    Physics::Get().Step(registry, deltaTime);
    // =========================================================

} // namespace VE