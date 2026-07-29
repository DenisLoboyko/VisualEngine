#pragma once
#include <glm/glm.hpp>
#include <functional>
#include "PhysicsMaterial.h"
#include "../ECS/Entity.h"

namespace VE {

    // =========================================================
    //  ColliderComponent — форма коллизий для Entity
    //
    //  Примеры:
    //
    //    // Твёрдый Box-коллайдер
    //    auto& col = registry.AddComponent<ColliderComponent>(e);
    //    col.Shape    = ColliderComponent::ShapeType::Box;
    //    col.HalfSize = { 0.5f, 0.5f, 0.5f };  // полуразмер ящика
    //
    //    // Триггер-сфера (объекты проходят сквозь, но событие срабатывает)
    //    auto& col = registry.AddComponent<ColliderComponent>(e);
    //    col.Shape     = ColliderComponent::ShapeType::Sphere;
    //    col.Radius    = 2.0f;
    //    col.IsTrigger = true;
    //    col.OnTriggerEnter = [](EntityID other) {
    //        // что-то произошло
    //    };
    //
    //    // Призрак (полная проходимость без событий)
    //    col.IsSolid   = false;
    //    col.IsTrigger = false;
    //
    //    // Layer mask — объект на слое 1, сталкивается только со слоями 0 и 2
    //    col.Layer      = 1;
    //    col.LayerMask  = (1 << 0) | (1 << 2);
    // =========================================================

    struct ColliderComponent
    {
        // ----- Форма -----------------------------------------
        enum class ShapeType
        {
            Box,        // AABB или OBB по Rotation из TransformComponent
            Sphere,     // сфера по Radius
            Capsule,    // капсула (Radius + Height)
        };

        ShapeType Shape    = ShapeType::Box;
        glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };  // для Box  (полуразмер)
        float     Radius   = 0.5f;                    // для Sphere / Capsule
        float     Height   = 1.0f;                    // для Capsule (без куполов)
        glm::vec3 Offset   = { 0.0f, 0.0f, 0.0f };   // смещение центра коллайдера

        // ----- Поведение -------------------------------------

        // IsSolid = true  → физически твёрдый, объекты отталкиваются
        // IsSolid = false → полная проходимость, коллбеки не вызываются
        bool IsSolid   = true;

        // IsTrigger = true → объекты проходят сквозь, НО вызывают OnTrigger*
        // IsTrigger работает только если IsSolid = true
        bool IsTrigger = false;

        // ----- Материал --------------------------------------
        PhysicsMaterial Material = PhysicsMaterial::Default();

        // ----- Слои ------------------------------------------
        // Layer     — на каком слое находится этот объект (0-31)
        // LayerMask — с какими слоями он сталкивается
        uint32_t Layer     = 0;
        uint32_t LayerMask = 0xFFFFFFFF;  // по умолчанию сталкивается со всеми

        bool CanCollideWith(const ColliderComponent& other) const
        {
            return (LayerMask & (1u << other.Layer)) != 0
                && (other.LayerMask & (1u << Layer)) != 0;
        }

        // ----- Коллбеки (заполняются пользователем) ----------

        // Вызывается когда объект ВХОДИТ в контакт
        std::function<void(EntityID other)> OnCollisionEnter;
        // Вызывается каждый тик пока контакт продолжается
        std::function<void(EntityID other)> OnCollisionStay;
        // Вызывается когда объект ВЫХОДИТ из контакта
        std::function<void(EntityID other)> OnCollisionExit;

        // Триггер-события (только когда IsTrigger = true)
        std::function<void(EntityID other)> OnTriggerEnter;
        std::function<void(EntityID other)> OnTriggerExit;

        ColliderComponent() = default;

        // Быстрые конструкторы
        static ColliderComponent Box(glm::vec3 halfSize = { 0.5f, 0.5f, 0.5f },
                                     bool isTrigger = false)
        {
            ColliderComponent c;
            c.Shape    = ShapeType::Box;
            c.HalfSize = halfSize;
            c.IsTrigger = isTrigger;
            return c;
        }

        static ColliderComponent Sphere(float radius = 0.5f, bool isTrigger = false)
        {
            ColliderComponent c;
            c.Shape     = ShapeType::Sphere;
            c.Radius    = radius;
            c.IsTrigger = isTrigger;
            return c;
        }

        static ColliderComponent Capsule(float radius = 0.5f, float height = 1.0f)
        {
            ColliderComponent c;
            c.Shape  = ShapeType::Capsule;
            c.Radius = radius;
            c.Height = height;
            return c;
        }

        // Призрак — полностью проходимый
        static ColliderComponent Ghost()
        {
            ColliderComponent c;
            c.IsSolid   = false;
            c.IsTrigger = false;
            return c;
        }
    };

} // namespace VE