#pragma once
// =========================================================
//  HUD.h  —  UI/HUD система для VisualEngine
//
//  Рисует текст и кнопки поверх Game View через ImGui.
//  Работает только в Play режиме.
//
//  Положи файл: src/Core/HUD.h
//
//  Lua API:
//    HUD.Text("Score: 100", 20, 20)
//    HUD.Text("Score: 100", 20, 20, 1, 1, 0, 1)   -- rgba
//    HUD.Text("Score: 100", 20, 20, 1, 1, 0, 1, 24)  -- rgba + размер шрифта (пока только default)
//    HUD.Button("Pause", 400, 300, 120, 40)  -> bool
//    HUD.Rect(x, y, w, h, r, g, b, a)
//    HUD.SetVisible(true/false)
//    HUD.Clear()   -- очистить все элементы (вызывается автоматически каждый кадр)
//
//  C++ — вызови HUD::Get().BeginFrame() в начале кадра (до Draw)
//         и    HUD::Get().Draw(gameViewPos, gameViewSize) после рендера сцены
// =========================================================

extern "C" {
#include "../../external/Lua/include/lua.h"
#include "../../external/Lua/include/lualib.h"
#include "../../external/Lua/include/lauxlib.h"
}

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace VE {

    // ── Типы HUD элементов ──────────────────────────────────
    enum class HUDElementType { Text, Button, Rect };

    struct HUDElement {
        HUDElementType type;
        std::string    text;
        float          x, y, w, h;
        float          r = 1, g = 1, b = 1, a = 1;
        bool           clicked = false; // результат кнопки
    };

    // =========================================================
    //  HUD — синглтон
    // =========================================================
    class HUD
    {
    public:
        static HUD& Get()
        {
            static HUD instance;
            return instance;
        }
        HUD(const HUD&) = delete;
        HUD& operator=(const HUD&) = delete;

        // ── Вызвать в начале каждого кадра ──────────────────
        void BeginFrame()
        {
            m_Elements.clear();
            m_ButtonResults.clear();
        }

        // ── API для C++ и Lua ────────────────────────────────

        void AddText(const std::string& text, float x, float y,
            float r = 1, float g = 1, float b = 1, float a = 1)
        {
            if (!m_Visible) return;
            m_Elements.push_back({ HUDElementType::Text, text, x, y, 0, 0, r, g, b, a });
        }

        // Возвращает true если кнопка нажата в этом кадре
        bool AddButton(const std::string& label, float x, float y, float w, float h,
            float r = 0.22f, float g = 0.14f, float b = 0.38f, float a = 1.f)
        {
            if (!m_Visible) return false;
            HUDElement el{ HUDElementType::Button, label, x, y, w, h, r, g, b, a };
            m_Elements.push_back(el);
            // Результат кнопки заполняется во время Draw()
            m_ButtonResults.push_back(false);
            return m_ButtonResults.back();
        }

        void AddRect(float x, float y, float w, float h,
            float r = 1, float g = 1, float b = 1, float a = 0.5f)
        {
            if (!m_Visible) return;
            m_Elements.push_back({ HUDElementType::Rect, "", x, y, w, h, r, g, b, a });
        }

        void SetVisible(bool v) { m_Visible = v; }
        bool IsVisible()  const { return m_Visible; }

        // ── Draw — вызывать внутри ImGui Game View после Image ──
        // gamePos  — позиция левого верхнего угла Game View в пикселях экрана
        // gameSize — размер Game View
        // Возвращает вектор результатов кнопок (индекс = порядок AddButton)
        void Draw(ImVec2 gamePos, ImVec2 gameSize)
        {
            if (!m_Visible) return;

            // Рисуем поверх Game View через ImDrawList
            ImDrawList* dl = ImGui::GetWindowDrawList();

            int btnIdx = 0;

            for (auto& el : m_Elements) {
                float px = gamePos.x + el.x;
                float py = gamePos.y + el.y;

                switch (el.type) {

                case HUDElementType::Text: {
                    ImU32 col = IM_COL32(
                        (int)(el.r * 255), (int)(el.g * 255),
                        (int)(el.b * 255), (int)(el.a * 255));
                    // Тень для читаемости
                    dl->AddText(ImVec2(px + 1, py + 1), IM_COL32(0, 0, 0, 180), el.text.c_str());
                    dl->AddText(ImVec2(px, py), col, el.text.c_str());
                    break;
                }

                case HUDElementType::Rect: {
                    ImU32 col = IM_COL32(
                        (int)(el.r * 255), (int)(el.g * 255),
                        (int)(el.b * 255), (int)(el.a * 255));
                    dl->AddRectFilled(
                        ImVec2(px, py),
                        ImVec2(px + el.w, py + el.h),
                        col, 4.f);
                    break;
                }

                case HUDElementType::Button: {
                    ImVec2 bMin(px, py);
                    ImVec2 bMax(px + el.w, py + el.h);

                    ImVec2 mp = ImGui::GetIO().MousePos;
                    bool hovered = mp.x >= bMin.x && mp.x <= bMax.x &&
                        mp.y >= bMin.y && mp.y <= bMax.y;
                    bool pressed = hovered && ImGui::GetIO().MouseClicked[0];

                    // Фон кнопки
                    float br = el.r, bg_ = el.g, bb = el.b;
                    if (hovered) {
                        br = br * 1.4f > 1.f ? 1.f : br * 1.4f;
                        bg_ = bg_ * 1.4f > 1.f ? 1.f : bg_ * 1.4f;
                        bb = bb * 1.4f > 1.f ? 1.f : bb * 1.4f;
                    }
                    ImU32 bgCol = IM_COL32((int)(br * 255), (int)(bg_ * 255), (int)(bb * 255), (int)(el.a * 255));
                    ImU32 bdCol = IM_COL32(200, 180, 255, 180);

                    dl->AddRectFilled(bMin, bMax, bgCol, 5.f);
                    dl->AddRect(bMin, bMax, bdCol, 5.f, 0, 1.5f);

                    // Текст по центру кнопки
                    ImVec2 ts = ImGui::CalcTextSize(el.text.c_str());
                    float tx = px + (el.w - ts.x) * 0.5f;
                    float ty = py + (el.h - ts.y) * 0.5f;
                    dl->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 160), el.text.c_str());
                    dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), el.text.c_str());

                    if (btnIdx < (int)m_ButtonResults.size())
                        m_ButtonResults[btnIdx] = pressed;
                    btnIdx++;
                    break;
                }
                }
            }
        }

        // Получить результат кнопки по индексу (после Draw)
        bool GetButtonResult(int idx) const
        {
            if (idx < 0 || idx >= (int)m_ButtonResults.size()) return false;
            return m_ButtonResults[idx];
        }

        // ── Lua биндинги ─────────────────────────────────────
        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // HUD.Text(text, x, y, r?, g?, b?, a?)
            lua_pushstring(L, "Text");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                const char* txt = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float r = lua_isnumber(LS, 4) ? (float)lua_tonumber(LS, 4) : 1.f;
                float g = lua_isnumber(LS, 5) ? (float)lua_tonumber(LS, 5) : 1.f;
                float b = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 1.f;
                float a = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 1.f;
                HUD::Get().AddText(txt, x, y, r, g, b, a);
                return 0;
                });
            lua_settable(L, -3);

            // HUD.Button(label, x, y, w, h) -> bool
            lua_pushstring(L, "Button");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                const char* lbl = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float w = (float)luaL_checknumber(LS, 4);
                float h = (float)luaL_checknumber(LS, 5);
                float r = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 0.22f;
                float g = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 0.14f;
                float b = lua_isnumber(LS, 8) ? (float)lua_tonumber(LS, 8) : 0.38f;
                bool res = HUD::Get().AddButton(lbl, x, y, w, h, r, g, b);
                lua_pushboolean(LS, res ? 1 : 0);
                return 1;
                });
            lua_settable(L, -3);

            // HUD.Rect(x, y, w, h, r?, g?, b?, a?)
            lua_pushstring(L, "Rect");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                float x = (float)luaL_checknumber(LS, 1);
                float y = (float)luaL_checknumber(LS, 2);
                float w = (float)luaL_checknumber(LS, 3);
                float h = (float)luaL_checknumber(LS, 4);
                float r = lua_isnumber(LS, 5) ? (float)lua_tonumber(LS, 5) : 1.f;
                float g = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 1.f;
                float b = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 1.f;
                float a = lua_isnumber(LS, 8) ? (float)lua_tonumber(LS, 8) : 0.5f;
                HUD::Get().AddRect(x, y, w, h, r, g, b, a);
                return 0;
                });
            lua_settable(L, -3);

            // HUD.SetVisible(bool)
            lua_pushstring(L, "SetVisible");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                HUD::Get().SetVisible(lua_toboolean(LS, 1) != 0);
                return 0;
                });
            lua_settable(L, -3);

            // HUD.IsVisible() -> bool
            lua_pushstring(L, "IsVisible");
            lua_pushcfunction(L, [](lua_State* LS) -> int {
                lua_pushboolean(LS, HUD::Get().IsVisible() ? 1 : 0);
                return 1;
                });
            lua_settable(L, -3);

            lua_setglobal(L, "HUD");
            std::cout << "[HUD] Lua API registered (HUD.*)\n";
        }

    private:
        HUD() = default;

        std::vector<HUDElement> m_Elements;
        std::vector<bool>       m_ButtonResults;
        bool                    m_Visible = true;
    };

} // namespace VE