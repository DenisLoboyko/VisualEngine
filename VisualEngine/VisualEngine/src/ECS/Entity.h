#pragma once
#include <cstdint>
#include <vector>
#include <cassert>

namespace VE {

    // =========================================================
    //  Entity — просто 32-битный ID
    //  Старшие 8 бит = версия (защита от «мёртвых» дескрипторов)
    //  Младшие 24 бита = индекс
    // =========================================================

    using EntityID = uint32_t;

    constexpr EntityID NULL_ENTITY      = 0;
    constexpr uint32_t ENTITY_INDEX_BITS = 24;
    constexpr uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;   // 0x00FFFFFF
    constexpr uint32_t ENTITY_VERSION_BITS = 8;
    constexpr uint32_t ENTITY_VERSION_MASK = (1u << ENTITY_VERSION_BITS) - 1u; // 0xFF

    inline uint32_t EntityIndex  (EntityID id) { return id & ENTITY_INDEX_MASK; }
    inline uint32_t EntityVersion(EntityID id) { return (id >> ENTITY_INDEX_BITS) & ENTITY_VERSION_MASK; }
    inline EntityID MakeEntityID (uint32_t index, uint32_t version)
    {
        return (version << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK);
    }

    // =========================================================
    //  EntityManager — выдаёт и отзывает ID
    // =========================================================
    class EntityManager
    {
    public:
        static constexpr uint32_t MAX_ENTITIES = 4096;

        EntityManager()
        {
            // Заполняем очередь свободных индексов (0 зарезервирован под NULL)
            m_FreeList.reserve(MAX_ENTITIES - 1);
            for (uint32_t i = 1; i < MAX_ENTITIES; ++i)
                m_FreeList.push_back(i);

            m_Versions.resize(MAX_ENTITIES, 0);
        }

        // Создать новую Entity
        EntityID Create()
        {
            assert(!m_FreeList.empty() && "EntityManager: превышен MAX_ENTITIES!");
            uint32_t idx = m_FreeList.back();
            m_FreeList.pop_back();
            ++m_AliveCount;
            return MakeEntityID(idx, m_Versions[idx]);
        }

        // Уничтожить Entity (увеличиваем версию — старые дескрипторы станут невалидными)
        void Destroy(EntityID id)
        {
            uint32_t idx = EntityIndex(id);
            assert(IsAlive(id) && "EntityManager: попытка уничтожить мёртвую Entity!");
            ++m_Versions[idx];          // инвалидируем старые дескрипторы
            m_FreeList.push_back(idx);
            --m_AliveCount;
        }

        // Проверить, жива ли Entity
        bool IsAlive(EntityID id) const
        {
            if (id == NULL_ENTITY) return false;
            uint32_t idx = EntityIndex(id);
            return m_Versions[idx] == EntityVersion(id);
        }

        uint32_t AliveCount() const { return m_AliveCount; }

    private:
        std::vector<uint32_t> m_Versions;   // версия для каждого слота
        std::vector<uint32_t> m_FreeList;   // свободные индексы (используем vector вместо queue для пула)
        uint32_t              m_AliveCount = 0;
    };

} // namespace VE