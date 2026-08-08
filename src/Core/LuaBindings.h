#pragma once
/**
 * ════════════════════════════════════════════════════════════════════════════════════
 * LuaBindings.h — Дополнительные Lua-биндинги VisualEngine
 *
 * ВАЖНО: этот файл лежит в src/Core/ — РЯДОМ с LuaEngine.h. LuaEngine.h делает
 * #include "LuaBindings.h" в самом конце (после определения класса LuaEngine),
 * так что этот файл обязан оставаться в той же папке — иначе снова C1083.
 *
 * Здесь регистрируются ТОЛЬКО системы без привязки к конкретному объекту
 * ("self" не нужен): Vector (математика), Debug (отладочная отрисовка),
 * Particles (частицы). Вызывается один раз в LuaEngine::registerFunctions()
 * при создании каждого lua_State — то есть ещё до того, как известно, какому
 * SceneObject этот скрипт принадлежит.
 *
 * Биндинги, которым НУЖЕН "self" (своя Entity) — Physics.AddRigidbody,
 * Physics.AddCollider, Physics.RaycastAll, Animation.PlayAnimation/
 * StopAnimation/SetAnimationSpeed, Scene.SetCamera — по той же причине, что и
 * уже существующие Physics.AddForce / Animation.Play / Scene.GetPosition,
 * зарегистрированы в main.cpp внутри StartPlay(), а НЕ здесь.
 *
 * Audio.StopSound/SetListenerPosition, HUD.ShowImage/ShowInputBox/ShowSlider,
 * SceneManager.SaveScene, SaveGame/LoadGame — добавлены прямо в существующие
 * AudioEngine.h / HUD.h / SceneManager.h / SaveSystem.h, рядом с остальными
 * методами тех же систем.
 * ════════════════════════════════════════════════════════════════════════════════════
 */

#include <glm/glm.hpp>
#include <string>

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

#include "DebugDraw.h"
#include "ParticleSystem.h"

namespace VE {
namespace LuaBindings {

// ════════════════════════════════════════════════════════════════════════════════════
// VECTOR MATH — Normalize, Dot, Cross, Angle
// Векторы передаются/возвращаются как три отдельных числа (x,y,z), как и везде
// в остальном Lua API движка (Physics.AddForce(x,y,z), GetVelocity() -> x,y,z и т.д.) —
// в этом движке нет отдельного Lua-типа "vector3".
// ════════════════════════════════════════════════════════════════════════════════════
inline void register_Vector(lua_State* L)
{
    lua_newtable(L); // Vector table

    // Vector.Normalize(x,y,z) -> x,y,z
    lua_pushstring(L, "Normalize");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 v((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        float len = glm::length(v);
        glm::vec3 n = (len > 1e-6f) ? (v / len) : glm::vec3(0.f);
        lua_pushnumber(LS, n.x); lua_pushnumber(LS, n.y); lua_pushnumber(LS, n.z);
        return 3;
    }, 0);
    lua_settable(L, -3);

    // Vector.Length(x,y,z) -> number  (удобный бонус — часто нужен вместе с Normalize)
    lua_pushstring(L, "Length");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 v((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        lua_pushnumber(LS, glm::length(v));
        return 1;
    }, 0);
    lua_settable(L, -3);

    // Vector.Dot(x1,y1,z1, x2,y2,z2) -> number
    lua_pushstring(L, "Dot");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 a((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        glm::vec3 b((float)luaL_optnumber(LS,4,0), (float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,0));
        lua_pushnumber(LS, glm::dot(a,b));
        return 1;
    }, 0);
    lua_settable(L, -3);

    // Vector.Cross(x1,y1,z1, x2,y2,z2) -> x,y,z
    lua_pushstring(L, "Cross");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 a((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        glm::vec3 b((float)luaL_optnumber(LS,4,0), (float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,0));
        glm::vec3 c = glm::cross(a,b);
        lua_pushnumber(LS, c.x); lua_pushnumber(LS, c.y); lua_pushnumber(LS, c.z);
        return 3;
    }, 0);
    lua_settable(L, -3);

    // Vector.Angle(x1,y1,z1, x2,y2,z2) -> градусы (0..180)
    lua_pushstring(L, "Angle");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 a((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        glm::vec3 b((float)luaL_optnumber(LS,4,0), (float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,0));
        float la = glm::length(a), lb = glm::length(b);
        float result = 0.f;
        if (la > 1e-6f && lb > 1e-6f) {
            float c = glm::clamp(glm::dot(a,b) / (la*lb), -1.f, 1.f);
            result = glm::degrees(acosf(c));
        }
        lua_pushnumber(LS, result);
        return 1;
    }, 0);
    lua_settable(L, -3);

    // Vector.Distance(x1,y1,z1, x2,y2,z2) -> number  (бонус — расстояние между точками)
    lua_pushstring(L, "Distance");
    lua_pushcclosure(L, [](lua_State* LS) -> int {
        glm::vec3 a((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
        glm::vec3 b((float)luaL_optnumber(LS,4,0), (float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,0));
        lua_pushnumber(LS, glm::length(b-a));
        return 1;
    }, 0);
    lua_settable(L, -3);

    lua_setglobal(L, "Vector");
}

// ════════════════════════════════════════════════════════════════════════════════════
// register_all_bindings — вызывается из LuaEngine::registerFunctions()
// ════════════════════════════════════════════════════════════════════════════════════
inline void register_all_bindings(lua_State* L)
{
    register_Vector(L);                        // Vector.Normalize/Dot/Cross/Angle/Length/Distance
    VE::DebugDraw::Get().RegisterLua(L);        // Debug.DrawLine/DrawSphere/DrawBox/Clear
    VE::ParticleSystem::Get().RegisterLua(L);   // Particles.Spawn/Clear/GetCount
}

} // namespace LuaBindings
} // namespace VE
