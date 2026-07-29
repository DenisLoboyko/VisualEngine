#pragma once
#include "Entity.h"
#include <array>
#include <unordered_map>
#include <cassert>

namespace VE {

    // =========================================================
    //  IComponentPool — базовый интерфейс (нужен Registry'ю
    //  чтобы хранить пулы разных типов в одном контейнере)
    // =========================================================
    struct IComponentPool
    {
        virtual ~IComponentPool() = default;
        virtual void OnEntityDestroyed(EntityID id) = 0;
    };

    // =========================================================
    //  ComponentPool<T> — dense array для одного типа компонента
    //
    //  Внутри два массива одинакового размера:
    //    m_Data[i]        — сам компонент
    //    m_IndexToEntity[i] — какой EntityID живёт в слоте i
    //  И обратная карта:
    //    m_EntityToIndex[id] — в каком слоте лежит компонент Entity
    //
    //  Удаление O(1): последний элемент переезжает на место удалённого.
    // =========================================================
    template<typename T>
    class ComponentPool : public IComponentPool
    {
    public:
        static constexpr uint32_t MAX_COMPONENTS = EntityManager::MAX_ENTITIES;

        // Добавить компонент к Entity (forward все аргументы в конструктор T)
        template<typename... Args>
        T& Add(EntityID id, Args&&... args)
        {
            assert(m_EntityToIndex.find(id) == m_EntityToIndex.end()
                   && "ComponentPool: у Entity уже есть этот компонент!");

            uint32_t slot = m_Size;
            m_Data[slot] = T{ std::forward<Args>(args)... };
            m_IndexToEntity[slot] = id;
            m_EntityToIndex[id]   = slot;
            ++m_Size;
            return m_Data[slot];
        }

        // Удалить компонент (swap-and-pop)
        void Remove(EntityID id)
        {
            auto it = m_EntityToIndex.find(id);
            assert(it != m_EntityToIndex.end()
                   && "ComponentPool: у Entity нет этого компонента!");

            uint32_t removedSlot = it->second;
            uint32_t lastSlot    = m_Size - 1;

            // Перемещаем последний элемент на место удалённого
            m_Data[removedSlot]         = std::move(m_Data[lastSlot]);
            EntityID movedEntity        = m_IndexToEntity[lastSlot];
            m_IndexToEntity[removedSlot]= movedEntity;
            m_EntityToIndex[movedEntity]= removedSlot;

            // Чистим последний слот
            m_EntityToIndex.erase(id);
            --m_Size;
        }

        // Получить компонент по ссылке
        T& Get(EntityID id)
        {
            auto it = m_EntityToIndex.find(id);
            assert(it != m_EntityToIndex.end()
                   && "ComponentPool: у Entity нет этого компонента!");
            return m_Data[it->second];
        }

        const T& Get(EntityID id) const
        {
            auto it = m_EntityToIndex.find(id);
            assert(it != m_EntityToIndex.end()
                   && "ComponentPool: у Entity нет этого компонента!");
            return m_Data[it->second];
        }

        // Проверить наличие компонента
        bool Has(EntityID id) const
        {
            return m_EntityToIndex.count(id) > 0;
        }

        // Вызывается автоматически при уничтожении Entity
        void OnEntityDestroyed(EntityID id) override
        {
            if (Has(id)) Remove(id);
        }

        // Итерация по всем компонентам (dense, cache-friendly)
        T*       Data()       { return m_Data.data(); }
        const T* Data() const { return m_Data.data(); }
        uint32_t Size() const { return m_Size; }

        EntityID EntityAt(uint32_t slot) const { return m_IndexToEntity[slot]; }

    private:
        std::array<T, MAX_COMPONENTS>        m_Data;
        std::array<EntityID, MAX_COMPONENTS> m_IndexToEntity{};
        std::unordered_map<EntityID, uint32_t> m_EntityToIndex;
        uint32_t m_Size = 0;
    };

} // namespace VE