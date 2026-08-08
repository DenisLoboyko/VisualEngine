#pragma once
// ── Свет и камера сцены (вынесено из main.cpp) ──

struct LightObject {
    std::string name; glm::vec3 pos={0,3,0},color={1,1,1};
    float intensity=1.f,range=10.f; bool active=true;
    VE::EntityID ecsID=VE::NULL_ENTITY;
};
struct CameraObject {
    std::string name="GameCamera"; glm::vec3 pos={0,2,5},rot={0,0,0};
    float fov=45.f; bool active=true,isPrimary=true;
    VE::EntityID ecsID=VE::NULL_ENTITY;
    int followTargetIndex=-1;            // индекс в objects[], -1 = свободный полёт
    glm::vec3 followOffset={0,1.6f,0};   // смещение от followTarget (высота глаз)
};
// ── Material: цвет + текстура + параметры поверхности ──
