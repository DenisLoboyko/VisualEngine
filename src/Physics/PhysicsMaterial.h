#pragma once

namespace VE {

    // =========================================================
    //  PhysicsMaterial — настройки поверхности
    //
    //  Пример:
    //    PhysicsMaterial ice;
    //    ice.Friction    = 0.05f;
    //    ice.Bounciness  = 0.0f;
    //
    //    PhysicsMaterial rubber;
    //    rubber.Friction   = 0.8f;
    //    rubber.Bounciness = 0.7f;
    //    rubber.Combine    = PhysicsMaterial::CombineMode::Max;
    // =========================================================

    struct PhysicsMaterial
    {
        // Как смешивать два материала при контакте (как в Source 2 / Unity)
        enum class CombineMode
        {
            Average,    // (a + b) / 2  — стандарт
            Multiply,   // a * b        — усиливает трение/отскок
            Min,        // min(a, b)    — скользкий побеждает
            Max         // max(a, b)    — упругий побеждает
        };

        float       Friction        = 0.5f;                 // 0 = лёд, 1 = резина
        float       Bounciness      = 0.0f;                 // 0 = нет отскока, 1 = идеальный
        CombineMode FrictionCombine = CombineMode::Average;
        CombineMode BounceCombine   = CombineMode::Average;

        // Дефолтные пресеты
        static PhysicsMaterial Default()  { return { 0.5f,  0.0f }; }
        static PhysicsMaterial Ice()      { return { 0.05f, 0.0f }; }
        static PhysicsMaterial Rubber()   { return { 0.8f,  0.7f, CombineMode::Multiply, CombineMode::Max }; }
        static PhysicsMaterial Metal()    { return { 0.3f,  0.1f }; }
        static PhysicsMaterial Bouncy()   { return { 0.4f,  0.9f, CombineMode::Average,  CombineMode::Max }; }
        static PhysicsMaterial NoFriction(){ return { 0.0f, 0.0f }; }

        // Смешать два материала по CombineMode
        static float Combine(float a, float b, CombineMode mode)
        {
            switch (mode)
            {
                case CombineMode::Average:  return (a + b) * 0.5f;
                case CombineMode::Multiply: return a * b;
                case CombineMode::Min:      return a < b ? a : b;
                case CombineMode::Max:      return a > b ? a : b;
            }
            return (a + b) * 0.5f;
        }

        // Итоговые значения при контакте двух материалов
        float ResolveFriction(const PhysicsMaterial& other) const
        {
            CombineMode mode = (static_cast<int>(FrictionCombine) >= static_cast<int>(other.FrictionCombine))
                               ? FrictionCombine : other.FrictionCombine;
            return Combine(Friction, other.Friction, mode);
        }

        float ResolveBounciness(const PhysicsMaterial& other) const
        {
            CombineMode mode = (static_cast<int>(BounceCombine) >= static_cast<int>(other.BounceCombine))
                               ? BounceCombine : other.BounceCombine;
            return Combine(Bounciness, other.Bounciness, mode);
        }
    };

} // namespace VE