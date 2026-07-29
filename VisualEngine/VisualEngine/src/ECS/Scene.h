#pragma once
#include "Registry.h"
#include "Components.h"

namespace VE {

    // =========================================================
    //  Scene — обёртка над Registry.
    //  Создаёт Entity и сразу навешивает нужные компоненты.
    //  Работает РЯДОМ со старым vector<SceneObject> —
    //  постепенно заменяем старый код новым.
    // =========================================================
    class Scene
    {
    public:
        Registry registry;

        // Создать пустую Entity с тегом
        EntityID CreateEntity(const std::string& name = "Entity")
        {
            EntityID id = registry.CreateEntity();
            registry.AddComponent<TagComponent>(id, name);
            registry.AddComponent<TransformComponent>(id);
            return id;
        }

        // Создать Entity с мешем (Cube/Sphere/Cylinder/Pyramid)
        EntityID CreateMeshEntity(const std::string& name,
                                   const Mesh& mesh,
                                   glm::vec3 position = {0,0,0},
                                   glm::vec3 color    = {1,1,1})
        {
            EntityID id = CreateEntity(name);
            registry.GetComponent<TransformComponent>(id).Position = position;
            registry.AddComponent<MeshComponent>(id, mesh, color);
            return id;
        }

        // Создать Entity с 3D моделью
        EntityID CreateModelEntity(const std::string& name,
                                    const std::string& modelPath,
                                    glm::vec3 position = {0,0,0})
        {
            EntityID id = CreateEntity(name);
            registry.GetComponent<TransformComponent>(id).Position = position;
            registry.AddComponent<ModelComponent>(id, modelPath);
            return id;
        }

        // Уничтожить Entity
        void DestroyEntity(EntityID id)
        {
            registry.DestroyEntity(id);
        }

        // Привязать Lua скрипт к Entity
        void AttachScript(EntityID id, const std::string& scriptPath)
        {
            if (!registry.HasComponent<ScriptComponent>(id))
                registry.AddComponent<ScriptComponent>(id, scriptPath);
            else
                registry.GetComponent<ScriptComponent>(id).ScriptPath = scriptPath;
        }

        void AttachScript(EntityID id, std::string&& scriptPath)
        {
            if (!registry.HasComponent<ScriptComponent>(id))
                registry.AddComponent<ScriptComponent>(id, std::move(scriptPath));
            else
                registry.GetComponent<ScriptComponent>(id).ScriptPath = std::move(scriptPath);
        }

        // Получить имя Entity
        std::string GetName(EntityID id)
        {
            if (registry.HasComponent<TagComponent>(id))
                return registry.GetComponent<TagComponent>(id).Name;
            return "Unknown";
        }

        // Получить Transform (удобный быстрый доступ)
        TransformComponent& GetTransform(EntityID id)
        {
            return registry.GetComponent<TransformComponent>(id);
        }

        // Проверить живёт ли Entity
        bool IsAlive(EntityID id) const
        {
            return registry.IsAlive(id);
        }

        // Количество живых Entity
        uint32_t EntityCount() const
        {
            return registry.EntityCount();
        }

        // Очистить сцену
        void Clear()
        {
            registry.Clear();
        }

        // -------------------------------------------------------
        //  Системы — вызывать каждый кадр в game loop
        // -------------------------------------------------------

        // Система скриптов: вернёт список {id, scriptPath}
        // для объектов у которых есть ScriptComponent
        void ForEachScript(const std::function<void(EntityID, ScriptComponent&, TransformComponent&)>& fn)
        {
            registry.Each<ScriptComponent, TransformComponent>(fn);
        }

        // Система рендера: вернёт список объектов для отрисовки
        void ForEachMesh(const std::function<void(EntityID, TransformComponent&, MeshComponent&)>& fn)
        {
            registry.Each<TransformComponent, MeshComponent>(fn);
        }

        void ForEachModel(const std::function<void(EntityID, TransformComponent&, ModelComponent&)>& fn)
        {
            registry.Each<TransformComponent, ModelComponent>(fn);
        }

        // Система освещения
        void ForEachLight(const std::function<void(EntityID, TransformComponent&, LightComponent&)>& fn)
        {
            registry.Each<TransformComponent, LightComponent>(fn);
        }
    };

} // namespace VE