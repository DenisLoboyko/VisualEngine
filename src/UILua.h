#pragma once
#include "UI2D.h"
#include "Core/AudioEngine.h"
#include <string>
#include <vector>
#include <filesystem>
#include <GLFW/glfw3.h>
extern "C" {
#include "../external/Lua/include/lua.h"
#include "../external/Lua/include/lauxlib.h"
}
extern std::vector<UIElement> uiElements;

namespace UILua {
inline UIElement* find(const char* nm){ if(!nm) return nullptr; for(auto& u:uiElements) if(u.name==nm) return &u; return nullptr; }

inline std::string ResolvePath(const char* p){
    std::string s=p; namespace fsx=std::filesystem;
    if (fsx::exists(s)) return s;
    if (fsx::exists("project/"+s)) return "project/"+s;
    if (fsx::exists("Assets/"+s)) return "Assets/"+s;
    return s;
}
inline float g_PopupEndTime = 0.f;
inline std::string g_PopupName;
inline void TickPopup(){
    if (g_PopupEndTime > 0.f && (float)glfwGetTime() >= g_PopupEndTime){
        auto* u = find(g_PopupName.c_str());
        if (u) u->visible = false;
        g_PopupEndTime = 0.f;
        g_PopupName.clear();
    }
}


inline void Register(lua_State* L){
    if(!L) return;
    lua_newtable(L);
    lua_newtable(L); lua_setfield(L,-2,"_callbacks");
    auto reg=[&](const char* n, lua_CFunction f){ lua_pushcfunction(L,f); lua_setfield(L,-2,n); };
    reg("create",[](lua_State* LS)->int{
        const char* t=lua_tostring(LS,1); const char* nm=lua_tostring(LS,2);
        UIElement u; u.name=nm?nm:("UI_"+std::to_string((int)uiElements.size()+1));
        std::string s=t?t:"Frame";
        if(s=="Button")u.type=UIElement::Type::Button;
        else if(s=="Text")u.type=UIElement::Type::Text;
        else if(s=="Image")u.type=UIElement::Type::Image;
        else u.type=UIElement::Type::Frame;
        u.fx=true; uiElements.push_back(u); return 0; });
    reg("destroy",[](lua_State* LS)->int{
        const char* nm=lua_tostring(LS,1); if(!nm) return 0;
        for(size_t i=0;i<uiElements.size();i++) if(uiElements[i].name==nm){ uiElements.erase(uiElements.begin()+i); break; }
        return 0; });
    reg("setImage",[](lua_State* LS)->int{
        auto* u=find(lua_tostring(LS,1)); const char* p=lua_tostring(LS,2);
        if(u&&p){ u->texPath=p; GLuint t=VE::LoadTextureRaw(ResolvePath(p)); if(t)u->tex=t; } return 0; });
    reg("setParent",[](lua_State* LS)->int{
        auto* u=find(lua_tostring(LS,1)); auto* p=find(lua_tostring(LS,2));
        if(u){ int idx=-1; if(p) for(size_t i=0;i<uiElements.size();i++) if(&uiElements[i]==p){idx=(int)i;break;} u->parentIndex=idx; } return 0; });
    reg("setText",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); const char* t=lua_tostring(LS,2); if(u&&t) u->text=t; return 0; });
    reg("getText",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u){lua_pushstring(LS,u->text.c_str()); return 1;} lua_pushnil(LS); return 1; });
    reg("setPos",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->anchor=glm::vec2((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3)); return 0; });
    reg("setOffset",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->posOffset=glm::vec2((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3)); return 0; });
    reg("setSize",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->size=glm::vec2((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3)); return 0; });
    reg("setSizeScale",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->sizeScale=glm::vec2((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3)); return 0; });
    reg("setPivot",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->anchorPoint=glm::vec2((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3)); return 0; });
    reg("setZ",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->z=(int)lua_tonumber(LS,2); return 0; });
    reg("setRounding",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->cornerRadius=(float)lua_tonumber(LS,2); return 0; });
    reg("setTransparency",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->transparency=(float)lua_tonumber(LS,2); return 0; });
    reg("setColor",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->color=glm::vec4((float)lua_tonumber(LS,2),(float)lua_tonumber(LS,3),(float)lua_tonumber(LS,4),(float)luaL_optnumber(LS,5,1)); return 0; });
    reg("setVisible",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->visible=lua_toboolean(LS,2)!=0; return 0; });
    reg("getVisible",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u){lua_pushboolean(LS,u->visible); return 1;} return 0; });
    reg("setFontSize",[](lua_State* LS)->int{ auto* u=find(lua_tostring(LS,1)); if(u) u->fontSize=(float)lua_tonumber(LS,2); return 0; });
    reg("onClicked",[](lua_State* LS)->int{
        const char* nm=lua_tostring(LS,1); if(!nm||!lua_isfunction(LS,2)) return 0;
        lua_getglobal(LS,"UI"); lua_getfield(LS,-1,"_callbacks");
        lua_getfield(LS,-1,nm);
        if(!lua_istable(LS,-1)){ lua_pop(LS,1); lua_newtable(LS); }
        lua_pushvalue(LS,2);
        lua_rawseti(LS,-2,(int)lua_rawlen(LS,-2)+1);
        lua_setfield(LS,-2,nm);
        lua_pop(LS,2); return 0; });
    reg("Popup",[](lua_State* LS)->int{
        const char* tex = luaL_checkstring(LS,1);
        const char* snd = luaL_checkstring(LS,2);
        float dur = (float)luaL_optnumber(LS,3,3.0);
        const char* name = "__popup_auto__";
        UIElement* u = find(name);
        if (!u){
            UIElement nu; nu.name=name;
            nu.type=UIElement::Type::Image;
            nu.sizeScale=glm::vec2(1.f,1.f);
            nu.size=glm::vec2(0.f,0.f);
            nu.anchor=glm::vec2(0.5f,0.5f);
            nu.anchorPoint=glm::vec2(0.5f,0.5f);
            nu.z=9999; nu.visible=false; nu.fx=true;
            uiElements.push_back(nu);
            u = &uiElements.back();
        }
        std::string tpath = ResolvePath(tex);
        GLuint t = VE::LoadTextureRaw(tpath);
        if (t) u->tex = t;
        u->texPath = tpath;
        u->visible = true;
        VE::AudioEngine::Get().PlaySound(ResolvePath(snd));
        g_PopupEndTime = (float)glfwGetTime() + dur;
        g_PopupName = name;
        logInfo("UI.Popup activated");
        return 0; });
    lua_setglobal(L,"UI");

    // ── Audio: проигрывание mp3/wav прямо из Lua ──
    lua_newtable(L);
    lua_pushcfunction(L,[](lua_State* LS)->int{
        const char* p=lua_tostring(LS,1); if(!p) return 0;
        VE::AudioEngine::Get().PlaySound(ResolvePath(p));
        return 0; });
    lua_setfield(L,-2,"play");
    lua_setglobal(L,"Audio");
    lua_newtable(L);
    lua_pushcfunction(L,[](lua_State* LS)->int{
        lua_pushnumber(LS,(double)glfwGetTime());
        return 1; });
    lua_setfield(L,-2,"time");
    lua_setglobal(L,"Time");
}
}

