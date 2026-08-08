#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <algorithm>

// Bullet Physics
#include <bullet/btBulletDynamicsCommon.h>

#include "../ECS/Registry.h"
#include "../ECS/Components.h"
#include "RigidbodyComponent.h"
#include "ColliderComponent.h"
#include "PhysicsMaterial.h"

namespace VE {

    // Пара столкнувшихся Entity за этот кадр (для callback'ов в Lua)
    struct CollisionPair
    {
        EntityID A, B;
        bool     IsTrigger; // true если хотя бы один из участников триггер
    };

    // =========================================================
    //  Physics — синглтон. Использует Bullet Physics внутри.
    //  Снаружи API такой же — компоненты RigidbodyComponent
    //  и ColliderComponent как раньше.
    // =========================================================
    class Physics
    {
    public:
        static Physics& Get()
        {
            static Physics instance;
            return instance;
        }
        Physics(const Physics&)            = delete;
        Physics& operator=(const Physics&) = delete;

        // ----- Настройки -------------------------------------
        void SetGravity(const glm::vec3& g)
        {
            m_Gravity = g;
            if (m_DynamicsWorld)
                m_DynamicsWorld->setGravity(btVector3(g.x, g.y, g.z));
        }
        glm::vec3 GetGravity() const { return m_Gravity; }
        void SetTimeScale(float s)   { m_TimeScale = s; }
        float GetTimeScale() const   { return m_TimeScale; }

        // ----- Главный тик -----------------------------------
        void Step(Registry& registry, float deltaTime)
        {
            float dt = deltaTime * m_TimeScale;
            if (dt <= 0.f) return;

            // Синхронизируем ECS → Bullet (позиции из редактора)
            SyncToBullet(registry);

            // Шаг симуляции Bullet
            m_DynamicsWorld->stepSimulation(dt, 10, 1.f / 120.f);

            // Синхронизируем Bullet → ECS (позиции после физики)
            SyncFromBullet(registry);

            // Обновляем флаги IsSleeping в RigidbodyComponent
            UpdateSleepFlags(registry);

            // Собираем коллизии этого кадра (для onCollision/onTrigger в Lua)
            UpdateCollisionEvents();
        }

        // ----- События коллизий этого кадра ------------------
        // Вызывать ПОСЛЕ Step(). Возвращает пары Entity, которые
        // начали соприкасаться именно в этом кадре (enter-события).
        const std::vector<CollisionPair>& GetCollisionEnters() const { return m_EntersThisFrame; }
        // Пары, которые перестали соприкасаться в этом кадре (exit-события).
        const std::vector<CollisionPair>& GetCollisionExits()  const { return m_ExitsThisFrame; }

        // Вызвать при удалении Entity с физикой
        void RemoveBody(EntityID id)
        {
            auto it = m_Bodies.find(id);
            if (it == m_Bodies.end()) return;

            btRigidBody* body = it->second;
            if (body->getMotionState())
                delete body->getMotionState();
            m_DynamicsWorld->removeRigidBody(body);

            auto shapeIt = m_Shapes.find(id);
            if (shapeIt != m_Shapes.end()){
                delete shapeIt->second;
                m_Shapes.erase(shapeIt);
            }
            delete body;
            m_Bodies.erase(it);
        }

        // Сброс всех тел (при Stop)
        void ClearAllBodies()
        {
            for (auto& pair : m_Bodies){
                btRigidBody* body = pair.second;
                if (body->getMotionState()) delete body->getMotionState();
                m_DynamicsWorld->removeRigidBody(body);
                delete body;
            }
            for (auto& pair : m_Shapes) delete pair.second;
            m_Bodies.clear();
            m_Shapes.clear();
            m_ActivePairsLastFrame.clear();
            m_EntersThisFrame.clear();
            m_ExitsThisFrame.clear();
        }

        // ----- Raycast (для выстрелов, проверки земли под ногами, клика по объекту в игре) -----
        struct RaycastHit {
            bool hit = false;
            EntityID entity = 0;
            glm::vec3 point{0,0,0};
            glm::vec3 normal{0,0,0};
            float distance = 0.f;
        };

        // origin — откуда пускаем луч, dir — направление (не обязательно нормализовано),
        // maxDistance — максимальная дальность в метрах.
        RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance = 1000.f)
        {
            RaycastHit result;
            float len = glm::length(dir);
            if (len < 1e-6f || !m_DynamicsWorld) return result;
            glm::vec3 nd = dir / len;

            btVector3 from(origin.x, origin.y, origin.z);
            btVector3 to(origin.x + nd.x*maxDistance, origin.y + nd.y*maxDistance, origin.z + nd.z*maxDistance);
            btCollisionWorld::ClosestRayResultCallback cb(from, to);
            m_DynamicsWorld->rayTest(from, to, cb);

            if (cb.hasHit()) {
                result.hit = true;
                result.entity = (EntityID)cb.m_collisionObject->getUserIndex();
                result.point = { cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z() };
                result.normal = { cb.m_hitNormalWorld.x(), cb.m_hitNormalWorld.y(), cb.m_hitNormalWorld.z() };
                result.distance = glm::length(result.point - origin);
            }
            return result;
        }

    private:
        Physics()
        {
            // Инициализация Bullet
            m_CollisionConfig     = new btDefaultCollisionConfiguration();
            m_Dispatcher          = new btCollisionDispatcher(m_CollisionConfig);
            m_Broadphase          = new btDbvtBroadphase();
            m_Solver              = new btSequentialImpulseConstraintSolver();
            m_DynamicsWorld       = new btDiscreteDynamicsWorld(
                m_Dispatcher, m_Broadphase, m_Solver, m_CollisionConfig);
            m_DynamicsWorld->setGravity(btVector3(0.f, -9.81f, 0.f));
        }

        ~Physics()
        {
            ClearAllBodies();
            delete m_DynamicsWorld;
            delete m_Solver;
            delete m_Broadphase;
            delete m_Dispatcher;
            delete m_CollisionConfig;
        }

        // Bullet объекты
        btDefaultCollisionConfiguration*     m_CollisionConfig = nullptr;
        btCollisionDispatcher*               m_Dispatcher      = nullptr;
        btDbvtBroadphase*                    m_Broadphase      = nullptr;
        btSequentialImpulseConstraintSolver* m_Solver          = nullptr;
        btDiscreteDynamicsWorld*             m_DynamicsWorld   = nullptr;

        // ECS EntityID → Bullet RigidBody
        std::unordered_map<EntityID, btRigidBody*>        m_Bodies;
        std::unordered_map<EntityID, btCollisionShape*>   m_Shapes;

        glm::vec3 m_Gravity    = { 0.f, -9.81f, 0.f };
        float     m_TimeScale  = 1.f;

        // Состояние коллизий: набор активных пар в текущем и прошлом кадре
        // (используем uint64 ключ = меньший id << 32 | больший id)
        std::unordered_map<uint64_t, bool> m_ActivePairsLastFrame; // bool = IsTrigger
        std::vector<CollisionPair> m_EntersThisFrame;
        std::vector<CollisionPair> m_ExitsThisFrame;

        static uint64_t PairKey(EntityID a, EntityID b)
        {
            if (a > b) std::swap(a, b);
            return (uint64_t(a) << 32) | uint64_t(b);
        }

        // Найти EntityID по Bullet btCollisionObject (обратный поиск в m_Bodies)
        EntityID FindEntityForBody(const btCollisionObject* obj) const
        {
            for (auto& pair : m_Bodies)
                if (pair.second == obj) return pair.first;
            return 0; // 0 = NULL_ENTITY по конвенции движка
        }

        // ----- Опросить Bullet на предмет активных контактов -
        void UpdateCollisionEvents()
        {
            std::unordered_map<uint64_t, bool> activeNow; // key -> isTrigger
            std::unordered_map<uint64_t, std::pair<EntityID,EntityID>> pairLookup;

            int numManifolds = m_Dispatcher->getNumManifolds();
            for (int i = 0; i < numManifolds; i++)
            {
                btPersistentManifold* manifold = m_Dispatcher->getManifoldByIndexInternal(i);
                if (manifold->getNumContacts() <= 0) continue;

                const btCollisionObject* objA = manifold->getBody0();
                const btCollisionObject* objB = manifold->getBody1();

                EntityID a = FindEntityForBody(objA);
                EntityID b = FindEntityForBody(objB);
                if (a == 0 || b == 0) continue;

                bool isTrigger =
                    (objA->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) ||
                    (objB->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE);

                uint64_t key = PairKey(a, b);
                activeNow[key] = isTrigger;
                pairLookup[key] = {a, b};
            }

            m_EntersThisFrame.clear();
            m_ExitsThisFrame.clear();

            // Enter: есть сейчас, не было в прошлом кадре
            for (auto& [key, isTrigger] : activeNow)
            {
                if (m_ActivePairsLastFrame.find(key) == m_ActivePairsLastFrame.end())
                {
                    auto [a, b] = pairLookup[key];
                    m_EntersThisFrame.push_back({a, b, isTrigger});
                }
            }
            // Exit: было в прошлом кадре, нет сейчас
            for (auto& [key, isTrigger] : m_ActivePairsLastFrame)
            {
                if (activeNow.find(key) == activeNow.end())
                {
                    // Восстановить EntityID из key (a в старших 32 бита, b в младших)
                    EntityID a = EntityID(key >> 32);
                    EntityID b = EntityID(key & 0xFFFFFFFF);
                    m_ExitsThisFrame.push_back({a, b, isTrigger});
                }
            }

            m_ActivePairsLastFrame = std::move(activeNow);
        }

        // ----- Создать/обновить Bullet тело для Entity -------
        btRigidBody* GetOrCreateBody(EntityID id,
                                     RigidbodyComponent& rb,
                                     ColliderComponent&  col,
                                     TransformComponent& tr)
        {
            auto it = m_Bodies.find(id);
            if (it != m_Bodies.end()) return it->second;

            // Форма коллайдера
            btCollisionShape* shape = CreateShape(col, tr);
            m_Shapes[id] = shape;

            // Масса и инерция
            float mass = (rb.IsKinematic || col.IsTrigger) ? 0.f : rb.Mass;
            btVector3 inertia(0, 0, 0);
            if (mass > 0.f)
                shape->calculateLocalInertia(mass, inertia);

            // Начальная позиция
            btTransform startTransform;
            startTransform.setIdentity();
            startTransform.setOrigin(btVector3(tr.Position.x, tr.Position.y, tr.Position.z));

            btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
            btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);

            // Материал
            info.m_friction    = col.Material.Friction;
            info.m_restitution = col.Material.Bounciness;
            info.m_linearDamping  = rb.LinearDrag;
            info.m_angularDamping = rb.AngularDrag;

            btRigidBody* body = new btRigidBody(info);

            // Кинематик
            if (rb.IsKinematic){
                body->setCollisionFlags(body->getCollisionFlags()
                    | btCollisionObject::CF_KINEMATIC_OBJECT);
                body->setActivationState(DISABLE_DEACTIVATION);
            }

            // Триггер
            if (col.IsTrigger){
                body->setCollisionFlags(body->getCollisionFlags()
                    | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            }

            // Заморозка осей
            btVector3 linearFactor(
                rb.FreezePositionX ? 0.f : 1.f,
                rb.FreezePositionY ? 0.f : 1.f,
                rb.FreezePositionZ ? 0.f : 1.f);
            btVector3 angularFactor(
                rb.FreezeRotationX ? 0.f : 1.f,
                rb.FreezeRotationY ? 0.f : 1.f,
                rb.FreezeRotationZ ? 0.f : 1.f);
            body->setLinearFactor(linearFactor);
            body->setAngularFactor(angularFactor);

            // Гравитация через GravityScale
            if (!rb.UseGravity || rb.IsKinematic)
                body->setGravity(btVector3(0, 0, 0));
            else
                body->setGravity(btVector3(
                    m_Gravity.x * rb.GravityScale,
                    m_Gravity.y * rb.GravityScale,
                    m_Gravity.z * rb.GravityScale));

            m_DynamicsWorld->addRigidBody(body);
            body->setUserIndex((int)id); // нужно для Raycast — определить, чей это Entity
            m_Bodies[id] = body;
            return body;
        }

        btCollisionShape* CreateShape(const ColliderComponent& col,
                                      const TransformComponent& tr)
        {
            switch (col.Shape)
            {
                case ColliderComponent::ShapeType::Sphere:
                {
                    float r = col.Radius * std::max({tr.Scale.x, tr.Scale.y, tr.Scale.z});
                    return new btSphereShape(r);
                }
                case ColliderComponent::ShapeType::Capsule:
                {
                    float r = col.Radius * std::max({tr.Scale.x, tr.Scale.z});
                    float h = col.Height * tr.Scale.y;
                    return new btCapsuleShape(r, h);
                }
                default: // Box
                {
                    glm::vec3 half = col.HalfSize * tr.Scale;
                    return new btBoxShape(btVector3(half.x, half.y, half.z));
                }
            }
        }

        // ----- Синхронизация ECS → Bullet --------------------
        void SyncToBullet(Registry& registry)
        {
            registry.Each<RigidbodyComponent, ColliderComponent, TransformComponent>(
                [&](EntityID id, RigidbodyComponent& rb,
                    ColliderComponent& col, TransformComponent& tr)
                {
                    btRigidBody* body = GetOrCreateBody(id, rb, col, tr);

                    // Применяем накопленные силы из RigidbodyComponent
                    if (!rb.IsKinematic && rb.Mass > 0.f)
                    {
                        if (glm::length(rb._ForceAccum) > 0.001f)
                        {
                            body->activate();
                            body->applyCentralForce(btVector3(
                                rb._ForceAccum.x,
                                rb._ForceAccum.y,
                                rb._ForceAccum.z));
                            rb._ForceAccum = {0,0,0};
                        }
                        if (glm::length(rb._TorqueAccum) > 0.001f)
                        {
                            body->applyTorque(btVector3(
                                rb._TorqueAccum.x,
                                rb._TorqueAccum.y,
                                rb._TorqueAccum.z));
                            rb._TorqueAccum = {0,0,0};
                        }
                    }

                    // Кинематик — двигаем через setWorldTransform
                    if (rb.IsKinematic)
                    {
                        btTransform t;
                        t.setIdentity();
                        t.setOrigin(btVector3(tr.Position.x, tr.Position.y, tr.Position.z));
                        body->getMotionState()->setWorldTransform(t);
                    }
                });
        }

        // ----- Синхронизация Bullet → ECS --------------------
        void SyncFromBullet(Registry& registry)
        {
            registry.Each<RigidbodyComponent, TransformComponent>(
                [&](EntityID id, RigidbodyComponent& rb, TransformComponent& tr)
                {
                    if (rb.IsKinematic) return;

                    auto it = m_Bodies.find(id);
                    if (it == m_Bodies.end()) return;

                    btRigidBody* body = it->second;
                    btTransform t;
                    body->getMotionState()->getWorldTransform(t);

                    btVector3 pos = t.getOrigin();
                    tr.Position = { pos.x(), pos.y(), pos.z() };

                    // Если вращение полностью заморожено по всем осям —
                    // это значит поворотом объекта управляет кто-то другой
                    // (обычно Lua-скрипт, например FPS-камера игрока).
                    // НЕ перетираем tr.Rotation физикой в этом случае,
                    // иначе скриптовый поворот откатывается каждый кадр.
                    bool fullyFrozen = rb.FreezeRotationX && rb.FreezeRotationY && rb.FreezeRotationZ;
                    if (!fullyFrozen) {
                        btQuaternion q = t.getRotation();
                        btScalar yaw, pitch, roll;
                        q.getEulerZYX(yaw, pitch, roll);
                        tr.Rotation = { glm::degrees((float)roll), glm::degrees((float)pitch), glm::degrees((float)yaw) };
                    }

                    btVector3 vel = body->getLinearVelocity();
                    rb.Velocity = { vel.x(), vel.y(), vel.z() };

                    btVector3 avel = body->getAngularVelocity();
                    rb.AngularVelocity = { avel.x(), avel.y(), avel.z() };
                });
        }

        // ----- Обновить флаги сна ----------------------------
        void UpdateSleepFlags(Registry& registry)
        {
            auto entities = registry.GetEntitiesWith<RigidbodyComponent>();
            for (EntityID id : entities)
            {
                auto it = m_Bodies.find(id);
                if (it == m_Bodies.end()) continue;
                RigidbodyComponent& rb = registry.GetComponent<RigidbodyComponent>(id);
                rb.IsSleeping = !it->second->isActive();
            }
        }
    };

} // namespace VE