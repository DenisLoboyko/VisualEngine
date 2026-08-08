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

    // Forward-declaration — сама реализация подключается через LuaBindings.h
    // ПОСЛЕ класса LuaEngine (см. #include внизу файла). Без этой строки
    // вызов VE::LuaBindings::register_all_bindings(L) внутри registerFunctions()
    // не компилируется: тело inline-метода класса разворачивается компилятором
    // "как будто сразу после закрывающей } класса" — то есть ДО того места,
    // где обычный #include внизу файла успел бы объявить namespace LuaBindings.
    namespace LuaBindings { void register_all_bindings(lua_State* L); }

    class LuaEngine
    {
    public:
        lua_State* L = nullptr;
        bool started = false;       // вызван ли onStart() уже (по одному на скрипт)
        std::string scriptPath;     // какому файлу принадлежит этот инстанс (для отладки/показа)

        // Object transform
        float objX=0,objY=0,objZ=0;
        float objRotX=0,objRotY=0,objRotZ=0;
        float objScaleX=1,objScaleY=1,objScaleZ=1;
        // Object color
        float objR=1,objG=1,objB=1;
        // Имя своего объекта — read-only для скрипта (this.name), нужно для
        // Scene.Destroy(this.name) / поиска себя же через Scene.GetPosition и т.п.
        std::string objName;
        // Отдельный "взгляд вверх/вниз" для FPS-камеры — НЕ вращает саму модель,
        // используется только для follow-камеры (см. lookPitch у SceneObject)
        float objLookPitch=0;

        std::string printOutput;

        LuaEngine()
        {
            L = luaL_newstate();
            luaL_openlibs(L);
            registerFunctions();
        }

        ~LuaEngine() { if(L) lua_close(L); }

        // Устанавливаем GLFW окно для Input
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

        // other.name, other.id передаются в Lua как таблица "other"
        void callOnCollisionEnter(const std::string& otherName, int otherID) { callCollisionFn("onCollisionEnter", otherName, otherID); }
        void callOnCollisionExit (const std::string& otherName, int otherID) { callCollisionFn("onCollisionExit",  otherName, otherID); }
        void callOnTriggerEnter  (const std::string& otherName, int otherID) { callCollisionFn("onTriggerEnter",   otherName, otherID); }
        void callOnTriggerExit   (const std::string& otherName, int otherID) { callCollisionFn("onTriggerExit",    otherName, otherID); }

        void callOnUpdate(float dt)
        {
            // ── Обновляем Time.deltaTime реальным значением этого кадра ──
            // (раньше было захардкожено 0.016 и никогда не менялось — баг)
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
        // Вызвать onCollisionEnter(other) и т.п. — собирает таблицу other={name=...,id=...}
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
            // Input таблица
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
            // ╨Ф╨╗╤П FPS-╨║╨░╨╝╨╡╤А╤Л: ╨╜╨░╤Б╨║╨╛╨╗╤М╨║╨╛ ╨╝╤Л╤И╤М ╤Б╨┤╨▓╨╕╨╜╤Г╨╗╨░╤Б╤М ╤Б ╨┐╤А╨╛╤И╨╗╨╛╨│╨╛ ╨║╨░╨┤╤А╨░.
            // ╨Ч╨╜╨░╤З╨╡╨╜╨╕╤П ╨╖╨░╨┐╨╛╨╗╨╜╤П╤О╤В╤Б╤П ╨╕╨╖ main.cpp ╤З╨╡╤А╨╡╨╖ extern ╨┐╨╡╤А╨╡╨╝╨╡╨╜╨╜╤Л╨╡.
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

            // math уже есть в Lua, но добавим удобные алиасы
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

            // Регистрируем все расширенные биндинги
            VE::LuaBindings::register_all_bindings(L);
        }
    };
}

// Включаем LuaBindings ПОСЛЕ определения пространства VE и класса LuaEngine
#include "LuaBindings.h"  // из папки src/Core/
