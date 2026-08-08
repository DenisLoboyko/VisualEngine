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
//    HUD.ShowImage(path, x, y, w, h, r?, g?, b?, a?)
//    HUD.ShowInputBox(id, x, y, w, h, defaultText?) -> string
//    HUD.ShowSlider(id, x, y, w, value, min, max) -> number
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
#include <unordered_map>
#include <cstring>
#include "TextureLoader.h"

namespace VE {

    // ── Типы HUD элементов ──────────────────────────────────
    enum class HUDElementType { Text, Button, Rect, Image, InputBox, Slider };

    struct HUDElement {
        HUDElementType type;
        std::string    text;
        float          x, y, w, h;
        float          r = 1, g = 1, b = 1, a = 1;
        bool           clicked = false;       // результат кнопки
        unsigned int   textureID = 0;         // для Image
        std::string    widgetId;              // для InputBox/Slider — стабильный ID между кадрами
        float          sliderMin = 0.f, sliderMax = 1.f; // для Slider
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
            // m_ButtonPressed / m_InputValues / m_SliderValues НЕ чистим —
            // это персистентное состояние виджетов между кадрами (иначе Lua
            // всегда бы читал значение "до" реального клика/ввода этого кадра).
        }

        // ── API для C++ и Lua ────────────────────────────────

        void AddText(const std::string& text, float x, float y,
            float r = 1, float g = 1, float b = 1, float a = 1)
        {
            if (!m_Visible) return;
            m_Elements.push_back({ HUDElementType::Text, text, x, y, 0, 0, r, g, b, a });
        }

        // Возвращает true если кнопка нажата в этом кадре.
        // Примечание: результат клика вычисляется во время Draw() — то есть
        // отражает клик из ПРЕДЫДУЩЕГО кадра (обычная задержка в 1 кадр для
        // immediate-mode HUD поверх FBO; так же работают InputBox/Slider ниже).
        bool AddButton(const std::string& label, float x, float y, float w, float h,
            float r = 0.22f, float g = 0.14f, float b = 0.38f, float a = 1.f)
        {
            if (!m_Visible) return false;
            HUDElement el{ HUDElementType::Button, label, x, y, w, h, r, g, b, a };
            el.widgetId = label; // ключ для m_ButtonPressed (кнопки с одинаковым label делят состояние)
            m_Elements.push_back(el);
            return m_ButtonPressed[label]; // значение из Draw() прошлого кадра
        }

        void AddRect(float x, float y, float w, float h,
            float r = 1, float g = 1, float b = 1, float a = 0.5f)
        {
            if (!m_Visible) return;
            m_Elements.push_back({ HUDElementType::Rect, "", x, y, w, h, r, g, b, a });
        }

        // Показать текстуру (например, иконку/спрайт) в заданном прямоугольнике HUD.
        // path — путь к файлу изображения; текстура кэшируется движком (VE::LoadTexture).
        void AddImage(const std::string& path, float x, float y, float w, float h,
            float r = 1, float g = 1, float b = 1, float a = 1)
        {
            if (!m_Visible) return;
            unsigned int tex = VE::LoadTexture(path);
            HUDElement el{ HUDElementType::Image, path, x, y, w, h, r, g, b, a };
            el.textureID = tex;
            m_Elements.push_back(el);
        }

        // Однострочное текстовое поле. id — стабильный идентификатор поля между кадрами
        // (не то же самое, что отображаемый текст). Возвращает ТЕКУЩЕЕ содержимое поля
        // (с задержкой в 1 кадр, как и AddButton — см. примечание выше).
        std::string AddInputBox(const std::string& id, float x, float y, float w, float h,
            const std::string& defaultText = "")
        {
            if (m_InputValues.find(id) == m_InputValues.end())
                m_InputValues[id] = defaultText; // инициализируем только один раз
            if (!m_Visible) return m_InputValues[id];
            HUDElement el{ HUDElementType::InputBox, "", x, y, w, h };
            el.widgetId = id;
            m_Elements.push_back(el);
            return m_InputValues[id];
        }

        // Горизонтальный слайдер. id — стабильный идентификатор между кадрами.
        // value — используется как начальное значение при ПЕРВОМ вызове с этим id.
        // Возвращает ТЕКУЩЕЕ значение (с задержкой в 1 кадр — см. примечание выше).
        float AddSlider(const std::string& id, float x, float y, float w,
            float value, float minV, float maxV)
        {
            if (m_SliderValues.find(id) == m_SliderValues.end())
                m_SliderValues[id] = value; // инициализируем только один раз
            if (!m_Visible) return m_SliderValues[id];
            HUDElement el{ HUDElementType::Slider, "", x, y, w, 0 };
            el.widgetId = id; el.sliderMin = minV; el.sliderMax = maxV;
            m_Elements.push_back(el);
            return m_SliderValues[id];
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

                case HUDElementType::Image: {
                    if (el.textureID == 0) break;
                    ImU32 tint = IM_COL32(
                        (int)(el.r * 255), (int)(el.g * 255),
                        (int)(el.b * 255), (int)(el.a * 255));
                    // UV перевёрнут по Y, т.к. TextureLoader грузит текстуры с
                    // stbi_set_flip_vertically_on_load(true) — та же конвенция, что и
                    // у остальных ImGui::Image(...) вызовов в движке (Scene/Game View).
                    dl->AddImage((ImTextureID)(intptr_t)el.textureID,
                        ImVec2(px, py), ImVec2(px + el.w, py + el.h),
                        ImVec2(0, 1), ImVec2(1, 0), tint);
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

                    m_ButtonPressed[el.widgetId] = pressed;
                    break;
                }

                case HUDElementType::InputBox: {
                    // Настоящий ImGui-виджет, позиционированный поверх Game View —
                    // работает, т.к. Draw() вызывается внутри активного ImGui::Begin/End
                    // окна Game View (см. main.cpp), значит клавиатура/мышь корректно
                    // перехватываются самим ImGui.
                    ImGui::SetCursorScreenPos(ImVec2(px, py));
                    ImGui::SetNextItemWidth(el.w);
                    std::string& val = m_InputValues[el.widgetId];
                    char buf[256];
                    strncpy_s(buf, val.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                    ImGui::PushID(el.widgetId.c_str());
                    if (ImGui::InputText("##hud_input", buf, sizeof(buf))) {
                        val = buf;
                    }
                    ImGui::PopID();
                    break;
                }

                case HUDElementType::Slider: {
                    ImGui::SetCursorScreenPos(ImVec2(px, py));
                    ImGui::SetNextItemWidth(el.w);
                    float& val = m_SliderValues[el.widgetId];
                    ImGui::PushID(el.widgetId.c_str());
                    ImGui::SliderFloat("##hud_slider", &val, el.sliderMin, el.sliderMax);
                    ImGui::PopID();
                    break;
                }
                }
            }
        }

        // Получить результат кнопки по label (после Draw)
        bool GetButtonResult(const std::string& label) const
        {
            auto it = m_ButtonPressed.find(label);
            return it != m_ButtonPressed.end() ? it->second : false;
        }

        // ── Lua биндинги ─────────────────────────────────────
        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // HUD.Text(text, x, y, r?, g?, b?, a?)
            lua_pushstring(L, "Text");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* txt = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float r = lua_isnumber(LS, 4) ? (float)lua_tonumber(LS, 4) : 1.f;
                float g = lua_isnumber(LS, 5) ? (float)lua_tonumber(LS, 5) : 1.f;
                float b = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 1.f;
                float a = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 1.f;
                HUD::Get().AddText(txt, x, y, r, g, b, a);
                return 0;
                }, 0);
            lua_settable(L, -3);

            // HUD.Button(label, x, y, w, h) -> bool
            lua_pushstring(L, "Button");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
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
                }, 0);
            lua_settable(L, -3);

            // HUD.Rect(x, y, w, h, r?, g?, b?, a?)
            lua_pushstring(L, "Rect");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
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
                }, 0);
            lua_settable(L, -3);

            // HUD.ShowImage(path, x, y, w, h, r?, g?, b?, a?)
            lua_pushstring(L, "ShowImage");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* path = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float w = (float)luaL_checknumber(LS, 4);
                float h = (float)luaL_checknumber(LS, 5);
                float r = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 1.f;
                float g = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 1.f;
                float b = lua_isnumber(LS, 8) ? (float)lua_tonumber(LS, 8) : 1.f;
                float a = lua_isnumber(LS, 9) ? (float)lua_tonumber(LS, 9) : 1.f;
                HUD::Get().AddImage(path, x, y, w, h, r, g, b, a);
                return 0;
                }, 0);
            lua_settable(L, -3);

            // HUD.ShowInputBox(id, x, y, w, h, defaultText?) -> string (текущее содержимое)
            lua_pushstring(L, "ShowInputBox");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* id = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float w = (float)luaL_checknumber(LS, 4);
                float h = (float)luaL_checknumber(LS, 5);
                const char* def = lua_isstring(LS, 6) ? lua_tostring(LS, 6) : "";
                std::string val = HUD::Get().AddInputBox(id, x, y, w, h, def);
                lua_pushstring(LS, val.c_str());
                return 1;
                }, 0);
            lua_settable(L, -3);

            // HUD.ShowSlider(id, x, y, w, value, min, max) -> number (текущее значение)
            lua_pushstring(L, "ShowSlider");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                const char* id = luaL_checkstring(LS, 1);
                float x = (float)luaL_checknumber(LS, 2);
                float y = (float)luaL_checknumber(LS, 3);
                float w = (float)luaL_checknumber(LS, 4);
                float value = (float)luaL_checknumber(LS, 5);
                float minV = lua_isnumber(LS, 6) ? (float)lua_tonumber(LS, 6) : 0.f;
                float maxV = lua_isnumber(LS, 7) ? (float)lua_tonumber(LS, 7) : 1.f;
                float res = HUD::Get().AddSlider(id, x, y, w, value, minV, maxV);
                lua_pushnumber(LS, res);
                return 1;
                }, 0);
            lua_settable(L, -3);

            // HUD.SetVisible(bool)
            lua_pushstring(L, "SetVisible");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                HUD::Get().SetVisible(lua_toboolean(LS, 1) != 0);
                return 0;
                }, 0);
            lua_settable(L, -3);

            // HUD.IsVisible() -> bool
            lua_pushstring(L, "IsVisible");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                lua_pushboolean(LS, HUD::Get().IsVisible() ? 1 : 0);
                return 1;
                }, 0);
            lua_settable(L, -3);

            lua_setglobal(L, "HUD");
            std::cout << "[HUD] Lua API registered (HUD.*)\n";
        }

    private:
        HUD() = default;

        std::vector<HUDElement> m_Elements;
        std::unordered_map<std::string, bool>        m_ButtonPressed;
        std::unordered_map<std::string, std::string> m_InputValues;
        std::unordered_map<std::string, float>       m_SliderValues;
        bool                    m_Visible = true;
    };

} // namespace VE