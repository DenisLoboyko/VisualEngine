#pragma once
#include <map>
#include <cmath>
#include <cfloat>

#define IC_SELECT "\xEF\x89\x85"
#define IC_MOVE   "\xEF\x81\x87"
#define IC_ROTATE "\xEF\x8B\xB1"
#define IC_SCALE  "\xEF\x81\xA5"
#define IC_SKY    "\xEF\x83\x82"
#define IC_GRID   "\xEF\xA1\x8C"
#define IC_GIZMO  "\xEF\x86\xB2"
#define IC_BLOOM  "\xEF\x86\x85"
#define IC_OBJECT "\xEF\x86\xB2"
#define IC_PAINT  "\xEF\x87\xBC"
#define IC_LIGHT  "\xEF\x83\xAB"
#define IC_ERASE  "\xEF\x84\xAD"
#define IC_FILL   "\xEF\x96\xAA"
#define IC_HIER   "\xEF\x80\x8A"
#define IC_SETTINGS "\xEF\x80\x93"
#define IC_HELP   "\xEF\x81\x99"

static bool IconToolButton(const char* icon, bool selected, const char* tooltip = nullptr)
{
    static std::map<ImGuiID, float> anim;
    ImGui::PushID(tooltip ? tooltip : icon);
    float sz = 36.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton(icon, ImVec2(sz, sz));
    ImGuiID iid = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    float& t = anim[iid];
    t += ((hovered?1.f:0.f)-t)*(1.f-expf(-14.f*ImGui::GetIO().DeltaTime));
    if (selected)
        dl->AddRectFilled(p, ImVec2(p.x+sz,p.y+sz), ImGui::GetColorU32(ImVec4(0.22f,0.24f,0.27f,0.9f)), 6.f);
    else if (t > 0.01f)
        dl->AddRectFilled(p, ImVec2(p.x+sz,p.y+sz), ImGui::GetColorU32(ImVec4(0.65f,0.65f,0.70f,0.22f*t)), 6.f);
    float fs = 16.f*(1.f+0.10f*t);
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, icon);
    dl->AddText(ImGui::GetFont(), fs, ImVec2(p.x+(sz-ts.x)*.5f, p.y+(sz-ts.y)*.5f), ImGui::GetColorU32(ImGuiCol_Text), icon);
    if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
    return clicked;
}

static bool IconBtn(const char* icon, bool active, const char* tooltip)
{
    ImGui::PushID(tooltip ? tooltip : icon);
    ImVec2 sz(44, 40);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton(icon, sz);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (active)
        dl->AddRectFilled(p, ImVec2(p.x+sz.x, p.y+sz.y), ImGui::GetColorU32(ImVec4(0.18f,0.20f,0.24f,1.f)), 6.f);
    else if (hovered)
        dl->AddRectFilled(p, ImVec2(p.x+sz.x, p.y+sz.y), ImGui::GetColorU32(ImVec4(0.16f,0.18f,0.22f,1.f)), 6.f);
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(20.f, FLT_MAX, 0.f, icon);
    dl->AddText(ImGui::GetFont(), 20.f, ImVec2(p.x+(sz.x-ts.x)*0.5f, p.y+(sz.y-ts.y)*0.5f),
        ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled), icon);
    if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
    return clicked;
}

