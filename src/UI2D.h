#pragma once
#include "imgui.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cfloat>
#include <cmath>

struct UIElement {
    enum class Type { Image, Text, Button, Frame };
    Type type = Type::Image;

    std::string name = "UI";
    int parentIndex = -1;

    glm::vec2 anchor    = glm::vec2(0.5f);
    glm::vec2 posOffset = glm::vec2(0.f);
    glm::vec2 sizeScale = glm::vec2(0.f);
    glm::vec2 size      = glm::vec2(160,50);
    glm::vec2 anchorPoint = glm::vec2(0.5f);

    glm::vec4 color = glm::vec4(1.f);
    float transparency = 0.f;
    float cornerRadius = 6.f;

    std::string text = "Text";
    float fontSize = 22.f;
    GLuint tex = 0;
    std::string texPath;
    bool visible = true;
    int z = 0;
    bool fx = false;
    float hoverT = 0.f;
    float pressT = 0.f;
};

namespace VEUI {
    inline int clickedThisFrame = -1;
    inline int hoveredIndex = -1;
    inline bool clickedElement = false;
    inline ImU32 col4(const glm::vec4& c){ return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r,c.g,c.b,c.a)); }

    inline void Draw(std::vector<UIElement>& els, ImVec2 vpPos, ImVec2 vpSize, bool editMode, int* selIdx) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float dt = ImGui::GetIO().DeltaTime;
        clickedThisFrame=-1; hoveredIndex=-1; clickedElement=false;
        static int dragIdx=-1, resizeIdx=-1;
        ImVec2 mouse = ImGui::GetMousePos();
        bool inVp = mouse.x>=vpPos.x && mouse.y>=vpPos.y && mouse.x<=vpPos.x+vpSize.x && mouse.y<=vpPos.y+vpSize.y;

        int n=(int)els.size();
        std::vector<ImVec2> MN(n), MX(n);
        std::vector<int> order; order.reserve(n);

        std::function<void(int,ImVec2,ImVec2)> compute = [&](int idx, ImVec2 pPos, ImVec2 pSize){
            UIElement& e = els[idx];
            ImVec2 center(pPos.x + e.anchor.x*pSize.x + e.posOffset.x,
                          pPos.y + e.anchor.y*pSize.y + e.posOffset.y);
            ImVec2 sz(std::max(2.f, e.sizeScale.x*pSize.x + e.size.x),
                      std::max(2.f, e.sizeScale.y*pSize.y + e.size.y));
            MN[idx] = ImVec2(center.x - sz.x*e.anchorPoint.x, center.y - sz.y*e.anchorPoint.y);
            MX[idx] = ImVec2(MN[idx].x + sz.x, MN[idx].y + sz.y);
            order.push_back(idx);
            for (int c=0;c<n;c++) if (c!=idx && els[c].parentIndex==idx) compute(c, MN[idx], sz);
        };
        std::vector<int> roots;
        for (int i=0;i<n;i++) if (els[i].parentIndex<0 || els[i].parentIndex>=n) roots.push_back(i);
        std::stable_sort(roots.begin(), roots.end(), [&](int a,int b){ return els[a].z < els[b].z; });
        for (int r : roots) compute(r, vpPos, vpSize);
        for (int i=0;i<n;i++) { bool f=false; for(int o:order) if(o==i){f=true;break;} if(!f) compute(i, vpPos, vpSize); }

        if (editMode) {
            if (resizeIdx>=0) {
                if (ImGui::IsMouseDown(0) && resizeIdx<n) {
                    UIElement& se = els[resizeIdx];
                    ImVec2 center(vpPos.x + se.anchor.x*vpSize.x + se.posOffset.x,
                                  vpPos.y + se.anchor.y*vpSize.y + se.posOffset.y);
                    se.size.x = std::max(8.f, fabsf(mouse.x-center.x)*2.f);
                    se.size.y = std::max(8.f, fabsf(mouse.y-center.y)*2.f);
                } else resizeIdx=-1;
            }
            if (dragIdx>=0) {
                if (ImGui::IsMouseDown(0) && dragIdx<n) {
                    els[dragIdx].anchor.x = std::clamp((mouse.x-vpPos.x)/vpSize.x, 0.f, 1.f);
                    els[dragIdx].anchor.y = std::clamp((mouse.y-vpPos.y)/vpSize.y, 0.f, 1.f);
                } else dragIdx=-1;
            }
            if (dragIdx<0 && resizeIdx<0 && inVp && ImGui::IsMouseClicked(0)) {
                if (selIdx && *selIdx>=0 && *selIdx<n) {
                    ImVec2 hcs[4] = { ImVec2(MN[*selIdx].x,MN[*selIdx].y), ImVec2(MX[*selIdx].x,MN[*selIdx].y),
                                      ImVec2(MN[*selIdx].x,MX[*selIdx].y), ImVec2(MX[*selIdx].x,MX[*selIdx].y) };
                    for (int h=0;h<4;h++) if (fabsf(mouse.x-hcs[h].x)<=10 && fabsf(mouse.y-hcs[h].y)<=10) { resizeIdx=*selIdx; break; }
                }
                if (resizeIdx<0) {
                    for (int k=(int)order.size()-1;k>=0;k--) {
                        int i=order[k]; UIElement& e=els[i]; if(!e.visible) continue;
                        if (mouse.x>=MN[i].x && mouse.y>=MN[i].y && mouse.x<=MX[i].x && mouse.y<=MX[i].y) {
                            if (selIdx) *selIdx=i; clickedElement=true; dragIdx=i; break;
                        }
                    }
                }
            }
        }

        for (int idx : order) {
            UIElement& e = els[idx];
            if (!e.visible) continue;
            ImVec2 mn=MN[idx], mx=MX[idx];
            ImVec2 sz(mx.x-mn.x, mx.y-mn.y);
            ImVec2 c2((mn.x+mx.x)*0.5f,(mn.y+mx.y)*0.5f);
            bool hov = inVp && mouse.x>=mn.x && mouse.y>=mn.y && mouse.x<=mx.x && mouse.y<=mx.y;
            if (!editMode && e.fx) {
                e.hoverT += ((hov?1.f:0.f)-e.hoverT)*std::min(1.f, dt*12.f);
                if (e.type==UIElement::Type::Button && hov && ImGui::IsMouseClicked(0)) { e.pressT=1.f; clickedThisFrame=idx; }
                e.pressT = std::max(0.f, e.pressT - dt*5.f);
            }
            float anim = 1.f + 0.05f*e.hoverT - 0.08f*e.pressT;
            ImVec2 amn(c2.x - sz.x*anim*0.5f, c2.y - sz.y*anim*0.5f);
            ImVec2 amx(c2.x + sz.x*anim*0.5f, c2.y + sz.y*anim*0.5f);
            float alpha = 1.f - e.transparency;
            ImU32 base = ImGui::ColorConvertFloat4ToU32(ImVec4(e.color.r,e.color.g,e.color.b,e.color.a*alpha));

            if (e.type==UIElement::Type::Image) {
                dl->AddImage((ImTextureID)(intptr_t)(e.tex?e.tex:VE2D::whiteTex), amn, amx, ImVec2(0,1), ImVec2(1,0), base);
            } else if (e.type==UIElement::Type::Text) {
                dl->AddText(ImGui::GetFont(), e.fontSize, amn, base, e.text.c_str());
            } else if (e.type==UIElement::Type::Frame) {
                dl->AddRectFilled(amn, amx, base, e.cornerRadius);
                if (editMode) dl->AddRect(amn, amx, IM_COL32(120,140,180,120), e.cornerRadius);
            } else {
                float hb = 0.15f + 0.10f*e.hoverT - 0.05f*e.pressT;
                ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(hb, hb+0.02f, hb+0.05f, 0.92f*alpha));
                if (!editMode && hov) hoveredIndex = idx;
                dl->AddRectFilled(amn, amx, bg, e.cornerRadius);
                if (e.tex) dl->AddImage((ImTextureID)(intptr_t)e.tex, ImVec2(amn.x+4,amn.y+4), ImVec2(amx.x-4,amx.y-4), ImVec2(0,1), ImVec2(1,0), base);
                ImU32 edge = ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f+0.3f*e.hoverT, 0.45f+0.3f*e.hoverT, 0.7f+0.3f*e.hoverT, alpha));
                dl->AddRect(amn, amx, edge, e.cornerRadius, 0, 1.f + 1.5f*e.hoverT);
                if (!e.tex) {
                if (!e.tex) {
                float fs = e.fontSize * (1.f + 0.03f*e.hoverT);
                ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.f, e.text.c_str(), nullptr);
                dl->AddText(ImGui::GetFont(), fs, ImVec2(c2.x - ts.x*0.5f, c2.y - ts.y*0.5f),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1,1,1,alpha)), e.text.c_str());
                }
                }
            }
            if (editMode) {
                bool isSel = selIdx && (*selIdx==idx);
                if (isSel) {
                    dl->AddRect(ImVec2(amn.x-2,amn.y-2), ImVec2(amx.x+2,amx.y+2), IM_COL32(60,180,255,255), 0.f, 0, 2.f);
                    dl->AddRectFilled(ImVec2(amn.x-5,amn.y-5), ImVec2(amn.x+5,amn.y+5), IM_COL32(60,180,255,255));
                    dl->AddRectFilled(ImVec2(amx.x-5,amn.y-5), ImVec2(amx.x+5,amn.y+5), IM_COL32(60,180,255,255));
                    dl->AddRectFilled(ImVec2(amn.x-5,amx.y-5), ImVec2(amn.x+5,amx.y+5), IM_COL32(60,180,255,255));
                    dl->AddRectFilled(ImVec2(amx.x-5,amx.y-5), ImVec2(amx.x+5,amx.y+5), IM_COL32(60,180,255,255));
                } else dl->AddRect(amn, amx, IM_COL32(255,255,255,50));
                dl->AddText(ImGui::GetFont(), 11.f, ImVec2(amn.x, amn.y-15.f), IM_COL32(60,180,255,220), e.name.c_str());
            }
        }
    }
}



