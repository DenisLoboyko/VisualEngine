#pragma once
#include "Entity.h"
#include "ComponentPool.h"

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <functional>
#include <vector>

namespace VE {

    // =========================================================
    //  ComponentTypeID — каждый тип компонента получает
    //  уникальный числовой ID во время компиляции
    // =========================================================
    namespace Internal {
        inline uint32_t NextComponentTypeID()
        {
            static uint32_t id = 0;
            return id++;
        }

        template<typename T>
        inline uint32_t ComponentTypeID()
        {
            static uint32_t id = NextComponentTypeID();
            return id;
        }
    }

    // =========================================================
    //  Registry — главное ядро ECS
    //
    //  Пример использования:
    //
    //    VE::Registry registry;
    //
    //    // Создать entity
    //    EntityID e = registry.CreateEntity();
    //
    //    // Добавить компоненты
    //    registry.AddComponent<TransformComponent>(e);
    //    registry.AddComponent<TagComponent>(e, "Player");
    //
    //    // Получить компонент
    //    auto& t = registry.GetComponent<TransformComponent>(e);
    //    t.Position = {0, 1, 0};
    //
    //    // Итерация по всем entity с нужными компонентами
    //    registry.Each<TransformComponent, MeshComponent>(
    //        [](EntityID id, TransformComponent& t, MeshComponent& m) {
    //            // логика...
    //        });
    //
    //    // Уничтожить entity
    //    registry.DestroyEntity(e);
    // =========================================================

    class Registry
    {
    public:

        // ----- Entity ----------------------------------------

        EntityID CreateEntity()
        {
            return m_EntityManager.Create();
        }

        void DestroyEntity(EntityID id)
        {
            // Уведомляем все пулы — они сами удалят компонент если есть
            for (auto& [typeIdx, pool] : m_Pools)
                pool->OnEntityDestroyed(id);

            m_EntityManager.Destroy(id);
        }

        bool IsAlive(EntityID id) const
        {
            return m_EntityManager.IsAlive(id);
        }

        uint32_t EntityCount() const
        {
            return m_EntityManager.AliveCount();
        }

        // ----- Components ------------------------------------

        // Добавить компонент (аргументы пробрасываются в конструктор T)
        template<typename T, typename... Args>
        T& AddComponent(EntityID id, Args&&... args)
        {
            return GetPool<T>().Add(id, std::forward<Args>(args)...);
        }

        // Удалить компонент
        template<typename T>
        void RemoveComponent(EntityID id)
        {
            GetPool<T>().Remove(id);
        }

        // Получить компонент по ссылке
        template<typename T>
        T& GetComponent(EntityID id)
        {
            return GetPool<T>().Get(id);
        }

        template<typename T>
        const T& GetComponent(EntityID id) const
        {
            return GetPool<T>().Get(id);
        }

        // Проверить наличие компонента
        template<typename T>
        bool HasComponent(EntityID id) const
        {
            auto key = std::type_index(typeid(T));
            auto it  = m_Pools.find(key);
            if (it == m_Pools.end()) return false;
            return static_cast<ComponentPool<T>*>(it->second.get())->Has(id);
        }

        // ----- Итерация --------------------------------------

        // Each<A, B, C>(lambda) — вызывает lambda для каждой Entity
        // у которой есть ВСЕ перечисленные компоненты
        template<typename... Components, typename Func>
        void Each(Func&& func)
        {
            // Берём пул первого компонента как основу итерации
            auto& primaryPool = GetPool<std::tuple_element_t<0, std::tuple<Components...>>>();

            for (uint32_t i = 0; i < primaryPool.Size(); ++i)
            {
                EntityID id = primaryPool.EntityAt(i);

                // Проверяем что у entity есть ВСЕ остальные компоненты
                if (!HasAll<Components...>(id)) continue;

                // Вызываем lambda с нужными компонентами
                func(id, GetComponent<Components>(id)...);
            }
        }

        // Each только по одному компоненту (самый частый случай)
        template<typename T, typename Func>
        void Each(Func&& func)
        {
            auto& pool = GetPool<T>();
            for (uint32_t i = 0; i < pool.Size(); ++i)
            {
                EntityID id = pool.EntityAt(i);
                func(id, pool.Data()[i]);
            }
        }

        // Найти первую Entity с нужным компонентом
        template<typename T>
        EntityID FindFirst()
        {
            auto& pool = GetPool<T>();
            if (pool.Size() == 0) return NULL_ENTITY;
            return pool.EntityAt(0);
        }

        // Получить всех entity у которых есть компонент T
        template<typename T>
        std::vector<EntityID> GetEntitiesWith()
        {
            auto& pool = GetPool<T>();
            std::vector<EntityID> result;
            result.reserve(pool.Size());
            for (uint32_t i = 0; i < pool.Size(); ++i)
                result.push_back(pool.EntityAt(i));
            return result;
        }

        // Очистить всё
        void Clear()
        {
            m_Pools.clear();
            m_EntityManager = EntityManager{};
        }

    private:

        // Получить (или создать) пул для типа T
        template<typename T>
        ComponentPool<T>& GetPool()
        {
            auto key = std::type_index(typeid(T));
            auto it  = m_Pools.find(key);
            if (it == m_Pools.end())
            {
                m_Pools[key] = std::make_unique<ComponentPool<T>>();
                it = m_Pools.find(key);
            }
            return *static_cast<ComponentPool<T>*>(it->second.get());
        }

        // Проверка наличия всех компонентов (variadic)
        template<typename... Components>
        bool HasAll(EntityID id)
        {
            return (HasComponent<Components>(id) && ...);
        }

        EntityManager m_EntityManager;
        std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_Pools;
    };

} // namespace VE