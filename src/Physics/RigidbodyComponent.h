#pragma once
#include <glm/glm.hpp>

namespace VE {

    // =========================================================
    //  RigidbodyComponent — физическое тело Entity
    //
    //  Примеры:
    //
    //    // Обычный физический объект
    //    auto& rb = registry.AddComponent<RigidbodyComponent>(e);
    //    rb.Mass        = 5.0f;
    //    rb.UseGravity  = true;
    //    rb.LinearDrag  = 0.1f;
    //
    //    // Подбросить вверх
    //    rb.AddImpulse({ 0, 10, 0 });
    //
    //    // Кинематик (двигается кодом, не физикой — как двери, платформы)
    //    rb.IsKinematic = true;
    //
    //    // Невесомость (как в Source 2 gravity_scale)
    //    rb.GravityScale = 0.0f;
    //
    //    // Притяжение вверх (отрицательная гравитация)
    //    rb.GravityScale = -1.0f;
    //
    //    // Заморозить вращение (не крутится от столкновений)
    //    rb.FreezeRotationX = true;
    //    rb.FreezeRotationZ = true;
    // =========================================================

    struct RigidbodyComponent
    {
        // ----- Основные параметры ----------------------------
        float     Mass          = 1.0f;     // кг, 0 = статический (бесконечная масса)
        float     LinearDrag    = 0.02f;    // затухание линейной скорости (0 = нет)
        float     AngularDrag   = 0.05f;    // затухание угловой скорости
        float     GravityScale  = 1.0f;     // множитель гравитации (Source 2 style)
                                            // 0 = невесомость, -1 = обратная гравитация

        // ----- Режимы ----------------------------------------
        bool      UseGravity    = true;     // применять ли гравитацию
        bool      IsKinematic   = false;    // если true — физика не двигает объект,
                                            //   но он сам может двигаться и сталкивать
        bool      IsSleeping    = false;    // объект не двигается (оптимизация)

        // ----- Заморозка осей (Source 2 / Unity style) -------
        bool      FreezePositionX = false;
        bool      FreezePositionY = false;
        bool      FreezePositionZ = false;
        bool      FreezeRotationX = false;
        bool      FreezeRotationY = false;
        bool      FreezeRotationZ = false;

        // ----- Состояние (обновляется Physics-системой) ------
        glm::vec3 Velocity        = { 0.f, 0.f, 0.f };
        glm::vec3 AngularVelocity = { 0.f, 0.f, 0.f };

        // Накопленные силы (сбрасываются каждый тик)
        glm::vec3 _ForceAccum    = { 0.f, 0.f, 0.f };
        glm::vec3 _TorqueAccum   = { 0.f, 0.f, 0.f };
        int       _SleepCounter  = 0;

        // Порог скорости ниже которого объект засыпает
        float     SleepThreshold = 0.05f;

        // ----- API -------------------------------------------

        // Приложить постоянную силу (Newton, применяется каждый тик)
        void AddForce(const glm::vec3& force)
        {
            if (!IsKinematic)
                _ForceAccum += force;
        }

        // Мгновенный импульс (кг*м/с) — подбросить, выстрелить
        void AddImpulse(const glm::vec3& impulse)
        {
            if (!IsKinematic && Mass > 0.f)
                Velocity += impulse / Mass;
        }

        // Момент силы (крутящий)
        void AddTorque(const glm::vec3& torque)
        {
            if (!IsKinematic)
                _TorqueAccum += torque;
        }

        // Мгновенно остановить
        void Stop()
        {
            Velocity        = { 0.f, 0.f, 0.f };
            AngularVelocity = { 0.f, 0.f, 0.f };
            _ForceAccum     = { 0.f, 0.f, 0.f };
            _TorqueAccum    = { 0.f, 0.f, 0.f };
        }

        // Обратная масса (используется в солвере)
        float InverseMass() const
        {
            return (Mass > 0.f) ? (1.f / Mass) : 0.f;
        }

        RigidbodyComponent() = default;

        static RigidbodyComponent Static()
        {
            RigidbodyComponent rb;
            rb.Mass       = 0.f;   // статический = бесконечная масса
            rb.UseGravity = false;
            rb.IsKinematic = true;
            return rb;
        }

        static RigidbodyComponent Kinematic()
        {
            RigidbodyComponent rb;
            rb.IsKinematic = true;
            rb.UseGravity  = false;
            return rb;
        }
    };

} // namespace VE