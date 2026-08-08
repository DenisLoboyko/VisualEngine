#pragma once
#include "EditorGlobals.h"
// ── GLFW колбэки мыши (вынесено из main.cpp) ──

void mouse_callback(GLFWwindow* w,double x,double y){
    mouseX=x;mouseY=y;
    // Сырая дельта для Lua (НЕ зависит от rightMouseDown — нужен всегда в Play)
    if(g_RawMouseFirst){g_LastRawMouseX=x;g_LastRawMouseY=y;g_RawMouseFirst=false;}
    g_RawMouseDX=x-g_LastRawMouseX; g_RawMouseDY=y-g_LastRawMouseY;
    g_LastRawMouseX=x; g_LastRawMouseY=y;
    // Пробрасываем событие в ImGui — иначе его собственная обработка мыши сломается
    if(g_PrevCursorPosCallback) g_PrevCursorPosCallback(w,x,y);
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

