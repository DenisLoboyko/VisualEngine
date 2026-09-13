#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

namespace VE {

    // Forward-declaration вЂ” СЃР°РјР° СЂРµР°Р»РёР·Р°С†РёСЏ РїРѕРґРєР»СЋС‡Р°РµС‚СЃСЏ С‡РµСЂРµР· LuaBindings.h
    // РџРћРЎР›Р• РєР»Р°СЃСЃР° LuaEngine (СЃРј. #include РІРЅРёР·Сѓ С„Р°Р№Р»Р°). Р‘РµР· СЌС‚РѕР№ СЃС‚СЂРѕРєРё
    // РІС‹Р·РѕРІ VE::LuaBindings::register_all_bindings(L) РІРЅСѓС‚СЂРё registerFunctions()
    // РЅРµ РєРѕРјРїРёР»РёСЂСѓРµС‚СЃСЏ: С‚РµР»Рѕ inline-РјРµС‚РѕРґР° РєР»Р°СЃСЃР° СЂР°Р·РІРѕСЂР°С‡РёРІР°РµС‚СЃСЏ РєРѕРјРїРёР»СЏС‚РѕСЂРѕРј
    // "РєР°Рє Р±СѓРґС‚Рѕ СЃСЂР°Р·Сѓ РїРѕСЃР»Рµ Р·Р°РєСЂС‹РІР°СЋС‰РµР№ } РєР»Р°СЃСЃР°" вЂ” С‚Рѕ РµСЃС‚СЊ Р”Рћ С‚РѕРіРѕ РјРµСЃС‚Р°,
    // РіРґРµ РѕР±С‹С‡РЅС‹Р№ #include РІРЅРёР·Сѓ С„Р°Р№Р»Р° СѓСЃРїРµР» Р±С‹ РѕР±СЉСЏРІРёС‚СЊ namespace LuaBindings.
    namespace LuaBindings { void register_all_bindings(lua_State* L); }

    class LuaEngine
    {
    public:
        lua_State* L = nullptr;
        bool started = false;       // РІС‹Р·РІР°РЅ Р»Рё onStart() СѓР¶Рµ (РїРѕ РѕРґРЅРѕРјСѓ РЅР° СЃРєСЂРёРїС‚)
        std::string scriptPath;     // РєР°РєРѕРјСѓ С„Р°Р№Р»Сѓ РїСЂРёРЅР°РґР»РµР¶РёС‚ СЌС‚РѕС‚ РёРЅСЃС‚Р°РЅСЃ (РґР»СЏ РѕС‚Р»Р°РґРєРё/РїРѕРєР°Р·Р°)

        // Object transform
        float objX=0,objY=0,objZ=0;
        float objRotX=0,objRotY=0,objRotZ=0;
        float objScaleX=1,objScaleY=1,objScaleZ=1;
        // Object color
        float objR=1,objG=1,objB=1;
        // РРјСЏ СЃРІРѕРµРіРѕ РѕР±СЉРµРєС‚Р° вЂ” read-only РґР»СЏ СЃРєСЂРёРїС‚Р° (this.name), РЅСѓР¶РЅРѕ РґР»СЏ
        // Scene.Destroy(this.name) / РїРѕРёСЃРєР° СЃРµР±СЏ Р¶Рµ С‡РµСЂРµР· Scene.GetPosition Рё С‚.Рї.
        std::string objName;
        // РћС‚РґРµР»СЊРЅС‹Р№ "РІР·РіР»СЏРґ РІРІРµСЂС…/РІРЅРёР·" РґР»СЏ FPS-РєР°РјРµСЂС‹ вЂ” РќР• РІСЂР°С‰Р°РµС‚ СЃР°РјСѓ РјРѕРґРµР»СЊ,
        // РёСЃРїРѕР»СЊР·СѓРµС‚СЃСЏ С‚РѕР»СЊРєРѕ РґР»СЏ follow-РєР°РјРµСЂС‹ (СЃРј. lookPitch Сѓ SceneObject)
        float objLookPitch=0;

        std::string printOutput;

        LuaEngine()
        {
            L = luaL_newstate();
            luaL_openlibs(L);
            registerFunctions();
        }

        ~LuaEngine() { if(L) lua_close(L); }

        // РЈСЃС‚Р°РЅР°РІР»РёРІР°РµРј GLFW РѕРєРЅРѕ РґР»СЏ Input
        void setWindow(GLFWwindow* w)
        {
            lua_pushlightuserdata(L, w);
            lua_setglobal(L, "__glfwWindow");
        }

        bool loadScript(const std::string& code)
        {
            printOutput.clear();
            int result = luaL_dostring(L, code.c_str());
            if(result != LUA_OK){
                printOutput = "[Error] " + std::string(lua_tostring(L,-1));
                lua_pop(L,1);
                return false;
            }
            return true;
        }

        bool loadFile(const std::string& path)
        {
            std::ifstream f(path);
            if(!f.is_open()){ printOutput="[Error] Cannot open: "+path; return false; }
            std::stringstream ss; ss<<f.rdbuf();
            return loadScript(ss.str());
        }

        void callOnStart()  { callFunction("onStart"); }

        // other.name, other.id РїРµСЂРµРґР°СЋС‚СЃСЏ РІ Lua РєР°Рє С‚Р°Р±Р»РёС†Р° "other"
        void callOnCollisionEnter(const std::string& otherName, int otherID) { callCollisionFn("onCollisionEnter", otherName, otherID); }
        void callOnCollisionExit (const std::string& otherName, int otherID) { callCollisionFn("onCollisionExit",  otherName, otherID); }
        void callOnTriggerEnter  (const std::string& otherName, int otherID) { callCollisionFn("onTriggerEnter",   otherName, otherID); }
        void callOnTriggerExit   (const std::string& otherName, int otherID) { callCollisionFn("onTriggerExit",    otherName, otherID); }

        void callOnUpdate(float dt)
        {
            // в”Ђв”Ђ РћР±РЅРѕРІР»СЏРµРј Time.deltaTime СЂРµР°Р»СЊРЅС‹Рј Р·РЅР°С‡РµРЅРёРµРј СЌС‚РѕРіРѕ РєР°РґСЂР° в”Ђв”Ђ
            // (СЂР°РЅСЊС€Рµ Р±С‹Р»Рѕ Р·Р°С…Р°СЂРґРєРѕР¶РµРЅРѕ 0.016 Рё РЅРёРєРѕРіРґР° РЅРµ РјРµРЅСЏР»РѕСЃСЊ вЂ” Р±Р°Рі)
            lua_getglobal(L,"Time");
            if(lua_istable(L,-1)){
                lua_pushstring(L,"deltaTime");
                lua_pushnumber(L,dt);
                lua_settable(L,-3);
            }
            lua_pop(L,1);

            lua_getglobal(L,"onUpdate");
            if(lua_isfunction(L,-1)){
                lua_pushnumber(L,dt);
                if(lua_pcall(L,1,0,0)!=LUA_OK){
                    printOutput+="[Error] "+std::string(lua_tostring(L,-1))+"\n";
                    lua_pop(L,1);
                }
            } else lua_pop(L,1);
        }

        void pushObjectData()
        {
            lua_newtable(L);
            // Transform
            setField("x",      objX);
            setField("y",      objY);
            setField("z",      objZ);
            setField("rotX",   objRotX);
            setField("rotY",   objRotY);
            setField("rotZ",   objRotZ);
            setField("scaleX", objScaleX);
            setField("scaleY", objScaleY);
            setField("scaleZ", objScaleZ);
            // Color
            setField("r",      objR);
            setField("g",      objG);
            setField("b",      objB);
            setField("lookPitch", objLookPitch);
            lua_pushstring(L, objName.c_str());
            lua_setfield(L, -2, "name");
            lua_setglobal(L,"this");
        }

        void pullObjectData()
        {
            lua_getglobal(L,"this");
            if(!lua_istable(L,-1)){ lua_pop(L,1); return; }
            objX      = getField("x");
            objY      = getField("y");
            objZ      = getField("z");
            objRotX   = getField("rotX");
            objRotY   = getField("rotY");
            objRotZ   = getField("rotZ");
            objScaleX = getField("scaleX");
            objScaleY = getField("scaleY");
            objScaleZ = getField("scaleZ");
            objR      = getField("r");
            objG      = getField("g");
            objB      = getField("b");
            objLookPitch = getField("lookPitch");
            lua_pop(L,1);
        }

    private:
        // Р’С‹Р·РІР°С‚СЊ onCollisionEnter(other) Рё С‚.Рї. вЂ” СЃРѕР±РёСЂР°РµС‚ С‚Р°Р±Р»РёС†Сѓ other={name=...,id=...}
        void callCollisionFn(const char* fnName, const std::string& otherName, int otherID)
        {
            lua_getglobal(L, fnName);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }

            lua_newtable(L);
            lua_pushstring(L, "name");
            lua_pushstring(L, otherName.c_str());
            lua_settable(L, -3);
            lua_pushstring(L, "id");
            lua_pushinteger(L, otherID);
            lua_settable(L, -3);

            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                printOutput += "[Error] " + std::string(lua_tostring(L,-1)) + "\n";
                lua_pop(L, 1);
            }
        }

        void callFunction(const char* name)
        {
            lua_getglobal(L,name);
            if(lua_isfunction(L,-1)){
                if(lua_pcall(L,0,0,0)!=LUA_OK){
                    printOutput+="[Error] "+std::string(lua_tostring(L,-1))+"\n";
                    lua_pop(L,1);
                }
            } else lua_pop(L,1);
        }

        void setField(const char* key,float val){
            lua_pushstring(L,key);
            lua_pushnumber(L,val);
            lua_settable(L,-3);
        }

        float getField(const char* key){
            lua_pushstring(L,key);
            lua_gettable(L,-2);
            float v=(float)lua_tonumber(L,-1);
            lua_pop(L,1);
            return v;
        }

        void registerFunctions()
        {
            // Input С‚Р°Р±Р»РёС†Р°
            lua_newtable(L);

            // Input.GetKey("W") -> bool
            lua_pushstring(L,"GetKey");
            lua_pushcfunction(L,[](lua_State* L)->int{
                const char* key=lua_tostring(L,1);
                if(!key){lua_pushboolean(L,0);return 1;}
                lua_getglobal(L,"__glfwWindow");
                GLFWwindow* w=(GLFWwindow*)lua_touserdata(L,-1);
                lua_pop(L,1);
                if(!w){lua_pushboolean(L,0);return 1;}
                int glfwKey=GLFW_KEY_UNKNOWN;
                std::string k(key);
                if     (k=="W"||k=="w")     glfwKey=GLFW_KEY_W;
                else if(k=="A"||k=="a")     glfwKey=GLFW_KEY_A;
                else if(k=="S"||k=="s")     glfwKey=GLFW_KEY_S;
                else if(k=="D"||k=="d")     glfwKey=GLFW_KEY_D;
                else if(k=="Q"||k=="q")     glfwKey=GLFW_KEY_Q;
                else if(k=="E"||k=="e")     glfwKey=GLFW_KEY_E;
                else if(k=="R"||k=="r")     glfwKey=GLFW_KEY_R;
                else if(k=="F"||k=="f")     glfwKey=GLFW_KEY_F;
                else if(k=="Space")         glfwKey=GLFW_KEY_SPACE;
                else if(k=="Shift")         glfwKey=GLFW_KEY_LEFT_SHIFT;
                else if(k=="Ctrl")          glfwKey=GLFW_KEY_LEFT_CONTROL;
                else if(k=="Alt")           glfwKey=GLFW_KEY_LEFT_ALT;
                else if(k=="Up")            glfwKey=GLFW_KEY_UP;
                else if(k=="Down")          glfwKey=GLFW_KEY_DOWN;
                else if(k=="Left")          glfwKey=GLFW_KEY_LEFT;
                else if(k=="Right")         glfwKey=GLFW_KEY_RIGHT;
                else if(k=="1")             glfwKey=GLFW_KEY_1;
                else if(k=="2")             glfwKey=GLFW_KEY_2;
                else if(k=="3")             glfwKey=GLFW_KEY_3;
                bool pressed=(glfwGetKey(w,glfwKey)==GLFW_PRESS);
                lua_pushboolean(L,pressed?1:0);
                return 1;
            });
            lua_settable(L,-3);

            // Input.GetAxis("Horizontal") -> float (-1, 0, 1)
            lua_pushstring(L,"GetAxis");
            lua_pushcfunction(L,[](lua_State* L)->int{
                const char* axis=lua_tostring(L,1);
                if(!axis){lua_pushnumber(L,0);return 1;}
                lua_getglobal(L,"__glfwWindow");
                GLFWwindow* w=(GLFWwindow*)lua_touserdata(L,-1);
                lua_pop(L,1);
                if(!w){lua_pushnumber(L,0);return 1;}
                std::string ax(axis);
                float val=0.f;
                if(ax=="Horizontal"){
                    if(glfwGetKey(w,GLFW_KEY_D)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_RIGHT)==GLFW_PRESS) val=1.f;
                    if(glfwGetKey(w,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_LEFT)==GLFW_PRESS)  val=-1.f;
                } else if(ax=="Vertical"){
                    if(glfwGetKey(w,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_UP)==GLFW_PRESS)    val=1.f;
                    if(glfwGetKey(w,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_DOWN)==GLFW_PRESS)  val=-1.f;
                }
                lua_pushnumber(L,val);
                return 1;
            });
            lua_settable(L,-3);

            // Input.GetMouseDeltaX() / GetMouseDeltaY() -> float
            // в•ЁР¤в•Ёв•—в•¤Рџ FPS-в•Ёв•‘в•Ёв–‘в•Ёв•ќв•Ёв•Ўв•¤Рђв•¤Р›: в•Ёв•њв•Ёв–‘в•¤Р‘в•Ёв•‘в•Ёв•›в•Ёв•—в•¤Рњв•Ёв•‘в•Ёв•› в•Ёв•ќв•¤Р›в•¤Рв•¤Рњ в•¤Р‘в•Ёв”¤в•Ёв–“в•Ёв••в•Ёв•њв•¤Р“в•Ёв•—в•Ёв–‘в•¤Р‘в•¤Рњ в•¤Р‘ в•Ёв”ђв•¤Рђв•Ёв•›в•¤Рв•Ёв•—в•Ёв•›в•Ёв”‚в•Ёв•› в•Ёв•‘в•Ёв–‘в•Ёв”¤в•¤Рђв•Ёв–‘.
            // в•ЁР§в•Ёв•њв•Ёв–‘в•¤Р—в•Ёв•Ўв•Ёв•њв•Ёв••в•¤Рџ в•Ёв•–в•Ёв–‘в•Ёв”ђв•Ёв•›в•Ёв•—в•Ёв•њв•¤Рџв•¤Рћв•¤Р’в•¤Р‘в•¤Рџ в•Ёв••в•Ёв•– main.cpp в•¤Р—в•Ёв•Ўв•¤Рђв•Ёв•Ўв•Ёв•– extern в•Ёв”ђв•Ёв•Ўв•¤Рђв•Ёв•Ўв•Ёв•ќв•Ёв•Ўв•Ёв•њв•Ёв•њв•¤Р›в•Ёв•Ў.
            lua_pushstring(L,"GetMouseDeltaX");
            lua_pushcfunction(L,[](lua_State* L)->int{
                extern double g_RawMouseDX;
                lua_pushnumber(L,g_RawMouseDX);
                return 1;
            });
            lua_settable(L,-3);

            lua_pushstring(L,"GetMouseDeltaY");
            lua_pushcfunction(L,[](lua_State* L)->int{
                extern double g_RawMouseDY;
                lua_pushnumber(L,g_RawMouseDY);
                return 1;
            });
            lua_settable(L,-3);

            lua_setglobal(L,"Input");

            // Time.deltaTime
            lua_newtable(L);
            lua_pushstring(L,"deltaTime");
            lua_pushnumber(L,0.016f);
            lua_settable(L,-3);
            lua_setglobal(L,"Time");

            // math СѓР¶Рµ РµСЃС‚СЊ РІ Lua, РЅРѕ РґРѕР±Р°РІРёРј СѓРґРѕР±РЅС‹Рµ Р°Р»РёР°СЃС‹
            lua_register(L,"print",[](lua_State* L)->int{
                int n=lua_gettop(L);
                std::string out;
                for(int i=1;i<=n;i++){
                    if(lua_isstring(L,i)) out+=lua_tostring(L,i);
                    else if(lua_isnumber(L,i)) out+=std::to_string((float)lua_tonumber(L,i));
                    else if(lua_isboolean(L,i)) out+=(lua_toboolean(L,i)?"true":"false");
                    if(i<n) out+="\t";
                }
                std::cout<<out<<"\n";
                return 0;
            });

            // Р РµРіРёСЃС‚СЂРёСЂСѓРµРј РІСЃРµ СЂР°СЃС€РёСЂРµРЅРЅС‹Рµ Р±РёРЅРґРёРЅРіРё
            VE::LuaBindings::register_all_bindings(L);
        { extern int VE_LuaInstAdd(lua_State*); extern int VE_LuaInstRemove(lua_State*); extern int VE_LuaInstSetActive(lua_State*); extern int VE_LuaInstClear(lua_State*); lua_newtable(L); lua_pushcfunction(L,VE_LuaInstAdd); lua_setfield(L,-2,"addCube"); lua_pushcfunction(L,VE_LuaInstRemove); lua_setfield(L,-2,"remove"); lua_pushcfunction(L,VE_LuaInstSetActive); lua_setfield(L,-2,"setActive"); lua_pushcfunction(L,VE_LuaInstClear); lua_setfield(L,-2,"clear"); lua_setglobal(L,"Inst"); }
        }
    };
}

// Р’РєР»СЋС‡Р°РµРј LuaBindings РџРћРЎР›Р• РѕРїСЂРµРґРµР»РµРЅРёСЏ РїСЂРѕСЃС‚СЂР°РЅСЃС‚РІР° VE Рё РєР»Р°СЃСЃР° LuaEngine
#include "LuaBindings.h"  // РёР· РїР°РїРєРё src/Core/

