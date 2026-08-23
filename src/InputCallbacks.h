#pragma once
#include "EditorGlobals.h"
// в”Ђв”Ђ GLFW РєРѕР»Р±СЌРєРё РјС‹С€Рё (РІС‹РЅРµСЃРµРЅРѕ РёР· main.cpp) в”Ђв”Ђ

void mouse_callback(GLFWwindow* w,double x,double y){
    mouseX=x;mouseY=y;
    // РЎС‹СЂР°СЏ РґРµР»СЊС‚Р° РґР»СЏ Lua (РќР• Р·Р°РІРёСЃРёС‚ РѕС‚ rightMouseDown вЂ” РЅСѓР¶РµРЅ РІСЃРµРіРґР° РІ Play)
    if(g_RawMouseFirst){g_LastRawMouseX=x;g_LastRawMouseY=y;g_RawMouseFirst=false;}
    g_RawMouseDX=x-g_LastRawMouseX; g_RawMouseDY=y-g_LastRawMouseY;
    g_LastRawMouseX=x; g_LastRawMouseY=y;
    // РџСЂРѕР±СЂР°СЃС‹РІР°РµРј СЃРѕР±С‹С‚РёРµ РІ ImGui вЂ” РёРЅР°С‡Рµ РµРіРѕ СЃРѕР±СЃС‚РІРµРЅРЅР°СЏ РѕР±СЂР°Р±РѕС‚РєР° РјС‹С€Рё СЃР»РѕРјР°РµС‚СЃСЏ
    if(g_PrevCursorPosCallback) g_PrevCursorPosCallback(w,x,y);
    if (g_MouseCaptured) gameCamera.ProcessMouse(g_RawMouseDX, -g_RawMouseDY);
    if(!rightMouseDown)return;
    if(firstMouse){lastX=x;lastY=y;firstMouse=false;}
    camera.ProcessMouse(x-lastX,lastY-y);lastX=x;lastY=y;
}
void mouse_button_callback(GLFWwindow* w,int btn,int action,int){
    if(btn==GLFW_MOUSE_BUTTON_RIGHT){rightMouseDown=(action==GLFW_PRESS);firstMouse=true;}
    if(btn==GLFW_MOUSE_BUTTON_LEFT){
        if(action==GLFW_PRESS){leftDown=true;leftClickThisFrame=true;double cx,cy;glfwGetCursorPos(w,&cx,&cy);clickX=cx;clickY=cy;}
        else{leftDown=false;dragAxis=GizmoAxis::None;}
    }
}



