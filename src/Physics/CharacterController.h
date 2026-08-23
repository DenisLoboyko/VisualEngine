#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "ColliderComponent.h"

namespace VE {
struct CCCollider { glm::vec3 pos; ColliderComponent col; };

class CharacterController {
public:
    glm::vec3 Position = glm::vec3(0,1,0);
    float Radius=0.4f, Height=1.8f, Speed=5.f, RunSpeed=8.f, JumpForce=6.f, Gravity=-15.f;
    bool IsGrounded() const { return grounded; }
    void Jump(){ if(grounded) vel.y=JumpForce; }
    void Stop(){ vel=glm::vec3(0); }

    void Move(const glm::vec3& motion, float dt, const std::vector<CCCollider>& cols){
        vel.y += Gravity*dt;
        glm::vec3 h = glm::vec3(motion.x,0,motion.z);
        if (glm::length(h) > 0.0001f){
            glm::vec3 step = h*dt;
            int n = (int)(glm::length(step)/0.05f)+1;
            glm::vec3 s = step/(float)n;
            for(int k=0;k<n;k++){
                glm::vec3 np = Position + s;
                if (!Hits(np, cols)) Position = np; else break;
            }
        }
        float oldFeet = Position.y - Height*0.5f;
        Position.y += vel.y*dt;
        grounded=false;
        if (vel.y <= 0.f){
            float feet = Position.y - Height*0.5f;
            for (auto& c : cols){
                float top = SupportTop(c);
                if (top < -1e5f) continue;
                if (feet <= top && oldFeet >= top - 0.001f){
                    Position.y = top + Height*0.5f; vel.y=0; grounded=true;
                }
            }
        }
        if (Position.y < -100.f){ Position=glm::vec3(Position.x,10,Position.z); vel=glm::vec3(0); }
    }

private:
    glm::vec3 vel = glm::vec3(0); bool grounded=false;
    static float dist2(const glm::vec3&a,const glm::vec3&b){ glm::vec3 d=a-b; return d.x*d.x+d.y*d.y+d.z*d.z; }

    bool Hits(const glm::vec3& capPos, const std::vector<CCCollider>& cols) const {
        for (auto& c : cols){
            if (!c.col.IsSolid || c.col.IsTrigger) continue;
            if (c.col.Shape==ColliderComponent::ShapeType::Box){
                glm::vec3 mn=c.pos-c.col.HalfSize, mx=c.pos+c.col.HalfSize;
                glm::vec3 cl(glm::clamp(capPos.x,mn.x,mx.x), glm::clamp(capPos.y,mn.y,mx.y), glm::clamp(capPos.z,mn.z,mx.z));
                float t=glm::clamp(cl.y-capPos.y, -Height*0.5f, Height*0.5f);
                glm::vec3 onCap=capPos+glm::vec3(0,t,0);
                if (dist2(cl,onCap) < Radius*Radius) return true;
            } else if (c.col.Shape==ColliderComponent::ShapeType::Sphere){
                float t=glm::clamp(c.pos.y-capPos.y, -Height*0.5f, Height*0.5f);
                glm::vec3 onCap=capPos+glm::vec3(0,t,0);
                float r=Radius+c.col.Radius;
                if (dist2(c.pos,onCap) < r*r) return true;
            } else {
                float halfC=c.col.Height*0.5f;
                float t1=glm::clamp(c.pos.y-capPos.y,-Height*0.5f,Height*0.5f);
                float t2=glm::clamp(capPos.y+t1-c.pos.y,-halfC,halfC);
                glm::vec3 p1=capPos+glm::vec3(0,t1,0), p2=c.pos+glm::vec3(0,t2,0);
                float r=Radius+c.col.Radius;
                if (dist2(p1,p2)<r*r) return true;
            }
        }
        return false;
    }

    float SupportTop(const CCCollider& c) const {
        if (!c.col.IsSolid || c.col.IsTrigger) return -1e6f;
        if (c.col.Shape==ColliderComponent::ShapeType::Box){
            glm::vec3 mn=c.pos-c.col.HalfSize, mx=c.pos+c.col.HalfSize;
            if (Position.x>mn.x-Radius && Position.x<mx.x+Radius && Position.z>mn.z-Radius && Position.z<mx.z+Radius)
                return mx.y;
        } else if (c.col.Shape==ColliderComponent::ShapeType::Sphere){
            float dx=Position.x-c.pos.x, dz=Position.z-c.pos.z; float r=c.col.Radius+Radius;
            if (dx*dx+dz*dz < r*r) return c.pos.y + c.col.Radius;
        } else {
            float dx=Position.x-c.pos.x, dz=Position.z-c.pos.z; float r=c.col.Radius+Radius;
            if (dx*dx+dz*dz < r*r) return c.pos.y + c.col.Height*0.5f + c.col.Radius;
        }
        return -1e6f;
    }
};
}
