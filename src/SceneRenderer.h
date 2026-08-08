#pragma once
#include "EditorGlobals.h"
#include "Core/DebugDraw.h"
#include "Core/ParticleSystem.h"
// ── Отрисовка сцены, гизмо, VAO примитивов (вынесено из main.cpp) ──

unsigned int setupCubeVAO(){
    float v[]={
        -0.5f,-0.5f,-0.5f,0,0,-1, 0,0,  0.5f,-0.5f,-0.5f,0,0,-1, 1,0,  0.5f,0.5f,-0.5f,0,0,-1, 1,1,  -0.5f,0.5f,-0.5f,0,0,-1, 0,1,
        -0.5f,-0.5f,0.5f,0,0,1, 0,0,  0.5f,-0.5f,0.5f,0,0,1, 1,0,  0.5f,0.5f,0.5f,0,0,1, 1,1,  -0.5f,0.5f,0.5f,0,0,1, 0,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,0,  -0.5f,0.5f,-0.5f,-1,0,0, 1,0,  -0.5f,0.5f,0.5f,-1,0,0, 1,1,  -0.5f,-0.5f,0.5f,-1,0,0, 0,1,
        0.5f,-0.5f,-0.5f,1,0,0, 0,0,  0.5f,0.5f,-0.5f,1,0,0, 1,0,  0.5f,0.5f,0.5f,1,0,0, 1,1,  0.5f,-0.5f,0.5f,1,0,0, 0,1,
        -0.5f,-0.5f,-0.5f,0,-1,0, 0,0,  0.5f,-0.5f,-0.5f,0,-1,0, 1,0,  0.5f,-0.5f,0.5f,0,-1,0, 1,1,  -0.5f,-0.5f,0.5f,0,-1,0, 0,1,
        -0.5f,0.5f,-0.5f,0,1,0, 0,0,  0.5f,0.5f,-0.5f,0,1,0, 1,0,  0.5f,0.5f,0.5f,0,1,0, 1,1,  -0.5f,0.5f,0.5f,0,1,0, 0,1
    };
    unsigned int idx[]={0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
    unsigned int VAO,VBO,EBO;
    glGenVertexArrays(1,&VAO);glGenBuffers(1,&VBO);glGenBuffers(1,&EBO);glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));glEnableVertexAttribArray(2);
    glBindVertexArray(0);return VAO;
}
unsigned int buildArrowVAO(int& cnt){
    std::vector<float> v;v.insert(v.end(),{0,0,0,0,0.65f,0});
    for(int i=0;i<12;i++){float a=2*3.14159f*i/12,b=2*3.14159f*(i+1)/12;v.insert(v.end(),{0.04f*cos(a),0.65f,0.04f*sin(a),0.04f*cos(b),0.65f,0.04f*sin(b),0,1,0});}
    cnt=v.size()/3;unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);glGenBuffers(1,&VBO);glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glBindVertexArray(0);return VAO;
}
void drawMesh(const SceneObject& obj,unsigned int cubeVAO,VE::Mesh& sph,VE::Mesh& cyl,VE::Mesh& pyr,VE::Mesh& cap,VE::Mesh& pln){
    switch(obj.type){
        case PrimitiveType::Cube:     glBindVertexArray(cubeVAO);glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Sphere:   glBindVertexArray(sph.VAO);glDrawElements(GL_TRIANGLES,sph.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Cylinder: glBindVertexArray(cyl.VAO);glDrawElements(GL_TRIANGLES,cyl.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Pyramid:  glBindVertexArray(pyr.VAO);glDrawElements(GL_TRIANGLES,pyr.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Capsule:  glBindVertexArray(cap.VAO);glDrawElements(GL_TRIANGLES,cap.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Plane:    glBindVertexArray(pln.VAO);glDrawElements(GL_TRIANGLES,pln.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Model3D:  if(obj.model&&obj.model->loaded)obj.model->Draw();break;
        case PrimitiveType::Empty:    break; // невидим в игре — только Transform + скрипты, как пустой GameObject в Unity
    }
}
void openInVSCode(const std::string& path){
    std::string cmd="start \"\" code \""+path+"\"";
    system(cmd.c_str());
}

VE::Scene scene;
void addObject(std::vector<SceneObject>& objects,PrimitiveType type,int& sel,SelectionType& selType){
    const char* n[]={"Cube","Sphere","Cylinder","Pyramid","Capsule","Plane","Model","Empty"};
    SceneObject o;
    o.name=std::string(n[(int)type])+"_"+std::to_string(objects.size()+1);
    o.pos=glm::vec3(0,0.5f,0);o.type=type;o.color=glm::vec3(0.4f,0.6f,0.9f);
    o.ecsID=scene.CreateEntity(o.name);
    scene.GetTransform(o.ecsID).Position=o.pos;
    scene.GetTransform(o.ecsID).Scale=o.scale;
    scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
    objects.push_back(o);sel=(int)objects.size()-1;selType=SelectionType::Object;
    logInfo("Created "+o.name);
}

void drawRing(unsigned int sid,glm::vec3 center,int axis,float gs,glm::vec4 col,const glm::mat4& vp){
    const int SEG=64; std::vector<float> pts;
    for(int j=0;j<=SEG;j++){float a=2.f*3.14159f*j/SEG,cs=cos(a)*gs,sn=sin(a)*gs;
        if(axis==0) pts.insert(pts.end(),{0,cs,sn});
        else if(axis==1) pts.insert(pts.end(),{cs,0,sn});
        else pts.insert(pts.end(),{cs,sn,0});}
    unsigned int rVAO,rVBO;glGenVertexArrays(1,&rVAO);glGenBuffers(1,&rVBO);
    glBindVertexArray(rVAO);glBindBuffer(GL_ARRAY_BUFFER,rVBO);
    glBufferData(GL_ARRAY_BUFFER,pts.size()*sizeof(float),pts.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glm::mat4 m=glm::translate(glm::mat4(1),center);
    glUniformMatrix4fv(glGetUniformLocation(sid,"mvp"),1,GL_FALSE,glm::value_ptr(vp*m));
    glUniform4f(glGetUniformLocation(sid,"color"),col.r,col.g,col.b,col.a);
    glLineWidth(2.5f);glDrawArrays(GL_LINE_STRIP,0,SEG+1);
    glDeleteVertexArrays(1,&rVAO);glDeleteBuffers(1,&rVBO);
}
// Рисует значок камеры или света как billboard, повёрнутый к камере.
// Вместо плоского цветного квадрата — узнаваемая иконка на тёмной круглой подложке.
void drawBillboard(unsigned int sid,glm::vec3 pos,glm::vec4 col,float size,const glm::mat4& view,const glm::mat4& proj,bool isLight){
    glm::vec3 right=glm::normalize(glm::vec3(view[0][0],view[1][0],view[2][0]));
    glm::vec3 up=glm::normalize(glm::vec3(view[0][1],view[1][1],view[2][1]));
    float R=size*0.5f;

    auto bp=[&](float x,float y)->glm::vec3{ return pos+right*(x*R)+up*(y*R); };

    unsigned int vao=0,vbo=0;
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glUniformMatrix4fv(glGetUniformLocation(sid,"mvp"),1,GL_FALSE,glm::value_ptr(proj*view*glm::mat4(1)));

    auto upload=[&](const std::vector<float>& v){
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
    };
    auto setColor=[&](glm::vec4 c){
        glUniform4f(glGetUniformLocation(sid,"color"),c.r,c.g,c.b,c.a);
    };

    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    // ── 1. Тёмная круглая подложка для контраста на любом фоне ──
    {
        const int SEG=20;
        std::vector<float> f; f.reserve((SEG+2)*3);
        glm::vec3 c0=pos; f.insert(f.end(),{c0.x,c0.y,c0.z});
        for(int i=0;i<=SEG;i++){
            float a=2.f*3.14159265f*(float)i/SEG;
            glm::vec3 v=bp(cosf(a),sinf(a));
            f.insert(f.end(),{v.x,v.y,v.z});
        }
        upload(f); setColor(glm::vec4(0.03f,0.03f,0.04f,0.55f));
        glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));
    }

    if(isLight){
        // ── 2a. Лампочка: кружок-«голова» + основание + лучи ──
        {
            const int SEG=14;
            std::vector<float> f; f.reserve((SEG+2)*3);
            glm::vec3 c0=bp(0,0.10f); f.insert(f.end(),{c0.x,c0.y,c0.z});
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(cosf(a)*0.34f, 0.10f+sinf(a)*0.34f);
                f.insert(f.end(),{v.x,v.y,v.z});
            }
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));
        }
        {
            glm::vec3 a=bp(-0.10f,-0.24f),b=bp(0.10f,-0.24f),c=bp(0.10f,-0.44f),d=bp(-0.10f,-0.44f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col*0.65f+glm::vec4(0,0,0,0.35f));
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            std::vector<float> f;
            for(int i=0;i<6;i++){
                float a=2.f*3.14159265f*((float)i/6.f)+0.5f;
                glm::vec3 p0=bp(cosf(a)*0.42f, 0.10f+sinf(a)*0.42f);
                glm::vec3 p1=bp(cosf(a)*0.62f, 0.10f+sinf(a)*0.62f);
                f.insert(f.end(),{p0.x,p0.y,p0.z,p1.x,p1.y,p1.z});
            }
            upload(f); setColor(col);
            glLineWidth(2.f);
            glDrawArrays(GL_LINES,0,(GLsizei)(f.size()/3));
        }
    } else {
        // ── 2b. Камера: корпус + объектив + видоискатель ──
        {
            glm::vec3 a=bp(-0.44f,-0.20f),b=bp(0.30f,-0.20f),c=bp(0.30f,0.20f),d=bp(-0.44f,0.20f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            glm::vec3 a=bp(-0.16f,0.20f),b=bp(0.06f,0.20f),c=bp(0.06f,0.36f),d=bp(-0.16f,0.36f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            const int SEG=16;
            std::vector<float> f; f.reserve((SEG+2)*3);
            glm::vec3 c0=bp(0.30f,0.0f); f.insert(f.end(),{c0.x,c0.y,c0.z});
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(0.30f+cosf(a)*0.20f, sinf(a)*0.20f);
                f.insert(f.end(),{v.x,v.y,v.z});
            }
            upload(f); setColor(glm::vec4(0.04f,0.04f,0.05f,1.f));
            glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));

            std::vector<float> ring; ring.reserve((SEG+1)*3);
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(0.30f+cosf(a)*0.20f, sinf(a)*0.20f);
                ring.insert(ring.end(),{v.x,v.y,v.z});
            }
            upload(ring); setColor(col);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINE_LOOP,0,(GLsizei)(ring.size()/3));
        }
    }

    // ── 3. Тонкий контур подложки поверх всего — чтобы значок не сливался с фоном ──
    {
        const int SEG=20;
        std::vector<float> f; f.reserve((SEG+1)*3);
        for(int i=0;i<=SEG;i++){
            float a=2.f*3.14159265f*(float)i/SEG;
            glm::vec3 v=bp(cosf(a),sinf(a));
            f.insert(f.end(),{v.x,v.y,v.z});
        }
        upload(f); setColor(glm::vec4(col.r,col.g,col.b,0.55f));
        glLineWidth(1.2f);
        glDrawArrays(GL_LINE_LOOP,0,(GLsizei)(f.size()/3));
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);
}

void renderScene(std::vector<SceneObject>& objects,int sel,bool isGameView,
    VE::Shader& shader,VE::Shader& skinnedShader,VE::Shader& outlineShader,VE::Shader& gridShader,
    VE::Shader& gizmoShader,VE::Shader& skyboxShader,
    VE::Skybox& skybox,VE::Grid& grid,
    unsigned int cubeVAO,VE::Mesh& sph,VE::Mesh& cyl,VE::Mesh& pyr,VE::Mesh& cap,VE::Mesh& pln,
    unsigned int arrowVAO,int arrowCnt,
    VE::Camera& cam,float aspect,GizmoMode gizmoMode,GizmoAxis dragAxis,
    bool showSkybox,bool showGrid,bool showGizmos,float gs,
    std::vector<LightObject>& lights,std::vector<CameraObject>& sceneCameras,
    int selLight,int selCamera,SelectionType selType,int excludeIndex=-1)
{
    glClearColor(0.10f,0.10f,0.13f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    glm::mat4 view=cam.GetViewMatrix(),proj=cam.GetProjectionMatrix(aspect),vp=proj*view;
    if(showSkybox) drawProceduralSky(skyboxShader.ID,view,proj,skybox.VAO,ComputeSunDir(g_TimeOfDay),(float)glfwGetTime());
    if(showGrid&&!isGameView) grid.Draw(gridShader.ID,view,proj);

    // ── Направленный свет солнца: направление/цвет/яркость зависят от времени суток ──
    glm::vec3 sunDir = ComputeSunDir(g_TimeOfDay);
    float sunH = sunDir.y;
    float sunDayF = glm::clamp((sunH+0.20f)/0.45f, 0.f, 1.f);
    glm::vec3 sunCol = glm::mix(glm::vec3(0.05f,0.06f,0.12f), glm::vec3(1.0f,0.95f,0.85f), sunDayF);
    float sunFinalIntensity = sunDayF * g_SunIntensity;

    shader.Use();
    glUniformMatrix4fv(glGetUniformLocation(shader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(shader.ID,"viewPos"),cam.Position.x,cam.Position.y,cam.Position.z);
    glUniform3f(glGetUniformLocation(shader.ID,"fogColor"),g_FogColor.x,g_FogColor.y,g_FogColor.z);
    glUniform1f(glGetUniformLocation(shader.ID,"fogDensity"),g_FogDensity);
    glUniform3f(glGetUniformLocation(shader.ID,"sunDir"),sunDir.x,sunDir.y,sunDir.z);
    glUniform3f(glGetUniformLocation(shader.ID,"sunColor"),sunCol.x,sunCol.y,sunCol.z);
    glUniform1f(glGetUniformLocation(shader.ID,"sunIntensity"),sunFinalIntensity);
    glUniform1f(glGetUniformLocation(shader.ID,"ambientStrength"),g_AmbientStrength);
    int lCount=(int)std::min(lights.size(),(size_t)8);
    glUniform1i(glGetUniformLocation(shader.ID,"lightCount"),lCount);
    for(int i=0;i<lCount;i++){
        std::string idx="["+std::to_string(i)+"]";
        glUniform3f(glGetUniformLocation(shader.ID,("lightPos"+idx).c_str()),lights[i].pos.x,lights[i].pos.y,lights[i].pos.z);
        glUniform3f(glGetUniformLocation(shader.ID,("lightColor"+idx).c_str()),lights[i].color.r,lights[i].color.g,lights[i].color.b);
        glUniform1f(glGetUniformLocation(shader.ID,("lightIntensity"+idx).c_str()),lights[i].intensity);
        glUniform1f(glGetUniformLocation(shader.ID,("lightRange"+idx).c_str()),lights[i].range);
    }
    glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

    // ── Те же общие uniform'ы (вид/проекция/свет/туман) настраиваем и на skinned-шейдере ──
    skinnedShader.Use();
    glUniformMatrix4fv(glGetUniformLocation(skinnedShader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(skinnedShader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"viewPos"),cam.Position.x,cam.Position.y,cam.Position.z);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"sunDir"),sunDir.x,sunDir.y,sunDir.z);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"sunColor"),sunCol.x,sunCol.y,sunCol.z);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"sunIntensity"),sunFinalIntensity);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"ambientStrength"),g_AmbientStrength);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"fogColor"),g_FogColor.x,g_FogColor.y,g_FogColor.z);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"fogDensity"),g_FogDensity);
    glUniform1i(glGetUniformLocation(skinnedShader.ID,"lightCount"),lCount);
    for(int i=0;i<lCount;i++){
        std::string idx="["+std::to_string(i)+"]";
        glUniform3f(glGetUniformLocation(skinnedShader.ID,("lightPos"+idx).c_str()),lights[i].pos.x,lights[i].pos.y,lights[i].pos.z);
        glUniform3f(glGetUniformLocation(skinnedShader.ID,("lightColor"+idx).c_str()),lights[i].color.r,lights[i].color.g,lights[i].color.b);
        glUniform1f(glGetUniformLocation(skinnedShader.ID,("lightIntensity"+idx).c_str()),lights[i].intensity);
        glUniform1f(glGetUniformLocation(skinnedShader.ID,("lightRange"+idx).c_str()),lights[i].range);
    }
    shader.Use();
    for(int i=0;i<(int)objects.size();i++){
        if(!objects[i].active) continue;
        if(i==excludeIndex) continue; // своё тело не рисуем от первого лица
        auto& obj=objects[i];
        if(scene.IsAlive(obj.ecsID)){auto& t=scene.GetTransform(obj.ecsID);t.Position=obj.pos;t.Rotation=obj.rot;t.Scale=obj.scale;}
        bool isSel=(selType==SelectionType::Object&&i==sel);
        if(!isGameView&&isSel){glStencilFunc(GL_ALWAYS,1,0xFF);glStencilMask(0xFF);}
        else{glStencilFunc(GL_ALWAYS,0,0xFF);glStencilMask(0x00);}
        glm::mat4 model=glm::mat4(1);
        model=glm::translate(model,obj.pos);
        model=glm::rotate(model,glm::radians(obj.rot.x),glm::vec3(1,0,0));
        model=glm::rotate(model,glm::radians(obj.rot.y),glm::vec3(0,1,0));
        model=glm::rotate(model,glm::radians(obj.rot.z),glm::vec3(0,0,1));
        model=glm::scale(model,obj.scale);

        bool useSkinning = (obj.type==PrimitiveType::Model3D && obj.model && obj.model->hasSkeleton && obj.animIndex>=0);
        VE::Shader& activeShader = useSkinning ? skinnedShader : shader;
        activeShader.Use();

        if (useSkinning) {
            auto boneMats = obj.model->GetBoneMatrices(obj.animIndex, obj.animTime, obj.animLoop);
            int n = std::min((int)boneMats.size(), VE::MAX_BONES);
            for (int b=0;b<n;b++) {
                std::string u = "boneMatrices["+std::to_string(b)+"]";
                glUniformMatrix4fv(glGetUniformLocation(activeShader.ID,u.c_str()),1,GL_FALSE,glm::value_ptr(boneMats[b]));
            }
        }

        glUniformMatrix4fv(glGetUniformLocation(activeShader.ID,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(activeShader.ID,"objectColor"),obj.color.r,obj.color.g,obj.color.b);
        GLuint texToBind=(obj.textureID!=0)?obj.textureID:VE::GetWhiteTexture();
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texToBind);
        glUniform1i(glGetUniformLocation(activeShader.ID,"uTexture"),0);
        glUniform1i(glGetUniformLocation(activeShader.ID,"useTexture"),obj.textureID!=0);

        glm::vec2 tiling(1.f,1.f);
        bool hasLayer2 = false, hasMask = false;
        GLuint layer2Tex = 0, maskTex = 0;
        glm::vec2 layer2Tiling(1.f,1.f);
        if (!obj.materials.empty()) {
            auto& m0 = obj.materials[0];
            tiling = { m0.tilingX, m0.tilingY };
            hasLayer2 = (m0.layer2TextureID != 0);
            hasMask   = (m0.maskTextureID != 0);
            layer2Tex = m0.layer2TextureID;
            maskTex   = m0.maskTextureID;
            layer2Tiling = { m0.layer2TilingX, m0.layer2TilingY };
        }
        glUniform2f(glGetUniformLocation(activeShader.ID,"uTiling"), tiling.x, tiling.y);
        glUniform1i(glGetUniformLocation(activeShader.ID,"useLayer2"), hasLayer2 && hasMask);
        glUniform1i(glGetUniformLocation(activeShader.ID,"useMask"),   hasLayer2 && hasMask);
        if (hasLayer2 && hasMask) {
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, layer2Tex);
            glUniform1i(glGetUniformLocation(activeShader.ID,"uLayer2Texture"), 1);
            glUniform2f(glGetUniformLocation(activeShader.ID,"uLayer2Tiling"), layer2Tiling.x, layer2Tiling.y);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, maskTex);
            glUniform1i(glGetUniformLocation(activeShader.ID,"uMaskTexture"), 2);
            glActiveTexture(GL_TEXTURE0);
        }

        drawMesh(obj,cubeVAO,sph,cyl,pyr,cap,pln);
        if (useSkinning) shader.Use(); // возвращаем основной шейдер для следующих объектов
    }
    if(!isGameView&&selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()){
        glStencilFunc(GL_NOTEQUAL,1,0xFF);glStencilMask(0x00);glDisable(GL_DEPTH_TEST);
        auto& obj=objects[sel];
        glm::mat4 model=glm::translate(glm::mat4(1),obj.pos);
        model=glm::rotate(model,glm::radians(obj.rot.x),glm::vec3(1,0,0));
        model=glm::rotate(model,glm::radians(obj.rot.y),glm::vec3(0,1,0));
        model=glm::rotate(model,glm::radians(obj.rot.z),glm::vec3(0,0,1));
        model=glm::scale(model,obj.scale);
        outlineShader.Use();
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform1f(glGetUniformLocation(outlineShader.ID,"outlineSize"),0.012f);
        glUniform4f(glGetUniformLocation(outlineShader.ID,"outlineColor"),.35f,.65f,1,1);
        drawMesh(obj,cubeVAO,sph,cyl,pyr,cap,pln);
        glStencilMask(0xFF);glStencilFunc(GL_ALWAYS,0,0xFF);glEnable(GL_DEPTH_TEST);
    }
    if(!isGameView&&showGizmos){
        glClear(GL_DEPTH_BUFFER_BIT);
        gizmoShader.Use();
        for(int i=0;i<(int)lights.size();i++){
            bool isSel=(selType==SelectionType::Light&&i==selLight);
            float d=glm::length(lights[i].pos-cam.Position);
            float iconSize=glm::clamp(d*0.10f,0.28f,1.4f);
            drawBillboard(gizmoShader.ID,lights[i].pos,isSel?glm::vec4(1,1,0,1):glm::vec4(1,.85f,.1f,1),iconSize,view,proj,true);
            if(isSel) drawRing(gizmoShader.ID,lights[i].pos,1,lights[i].range,glm::vec4(1,.85f,.1f,.4f),vp);
        }
        for(int i=0;i<(int)sceneCameras.size();i++){
            bool isSel=(selType==SelectionType::Camera&&i==selCamera);
            glm::vec3 camWorldPos = sceneCameras[i].pos;
            if(sceneCameras[i].followTargetIndex>=0 && sceneCameras[i].followTargetIndex<(int)objects.size())
                camWorldPos = objects[sceneCameras[i].followTargetIndex].pos + sceneCameras[i].followOffset;
            float d=glm::length(camWorldPos-cam.Position);
            float iconSize=glm::clamp(d*0.10f,0.28f,1.4f);
            drawBillboard(gizmoShader.ID,camWorldPos,isSel?glm::vec4(.3f,.9f,1,1):glm::vec4(.2f,.7f,1,1),iconSize,view,proj,false);
        }
        for(int i=0;i<(int)objects.size();i++){
            if(objects[i].type!=PrimitiveType::Empty) continue;
            bool isSel=(selType==SelectionType::Object&&i==sel);
            float d=glm::length(objects[i].pos-cam.Position);
            float iconSize=glm::clamp(d*0.08f,0.22f,1.1f);
            drawBillboard(gizmoShader.ID,objects[i].pos,isSel?glm::vec4(1,1,1,1):glm::vec4(.75f,.75f,.8f,1),iconSize,view,proj,false);
        }
        glm::vec3 gPos(0);bool showGiz=false;
        if(selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()){gPos=objects[sel].pos;showGiz=true;}
        else if(selType==SelectionType::Light&&selLight>=0&&selLight<(int)lights.size()){gPos=lights[selLight].pos;showGiz=true;}
        else if(selType==SelectionType::Camera&&selCamera>=0&&selCamera<(int)sceneCameras.size()){
            auto& sc=sceneCameras[selCamera];
            gPos = (sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size())
                 ? objects[sc.followTargetIndex].pos + sc.followOffset
                 : sc.pos;
            showGiz=true;
        }
        if(showGiz&&gizmoMode!=GizmoMode::Select){
            if(gizmoMode==GizmoMode::Move||gizmoMode==GizmoMode::Scale){
                glBindVertexArray(arrowVAO);
                glm::vec4 cols[3]={glm::vec4(1,.15f,.15f,1),glm::vec4(.15f,1,.15f,1),glm::vec4(.15f,.4f,1,1)};
                float rA[3]={-90,0,90};glm::vec3 rX[3]={glm::vec3(0,0,1),glm::vec3(0,1,0),glm::vec3(1,0,0)};
                for(int i=0;i<3;i++){
                    glm::vec4 col=dragAxis==(GizmoAxis)(i+1)?glm::vec4(1,1,.2f,1):cols[i];
                    glm::mat4 m=glm::translate(glm::mat4(1),gPos);
                    m=glm::rotate(m,glm::radians(rA[i]),rX[i]);m=glm::scale(m,glm::vec3(gs));
                    glUniformMatrix4fv(glGetUniformLocation(gizmoShader.ID,"mvp"),1,GL_FALSE,glm::value_ptr(vp*m));
                    glUniform4f(glGetUniformLocation(gizmoShader.ID,"color"),col.r,col.g,col.b,col.a);
                    glDrawArrays(GL_TRIANGLES,2,arrowCnt-2);glLineWidth(2.f);glDrawArrays(GL_LINES,0,2);
                }
            } else if(gizmoMode==GizmoMode::Rotate){
                glm::vec4 rc[3]={glm::vec4(1,.15f,.15f,1),glm::vec4(.15f,1,.15f,1),glm::vec4(.15f,.4f,1,1)};
                for(int i=0;i<3;i++){
                    glm::vec4 col=dragAxis==(GizmoAxis)(i+1)?glm::vec4(1,1,.2f,1):rc[i];
                    drawRing(gizmoShader.ID,gPos,i,gs,col,vp);
                }
            }
        }
    }

    // ── Debug.DrawLine/Sphere/Box и Particles.Spawn — только в Game View
    //    (те же самые примитивы, что Unity рисует поверх Game view при Play) ──
    if (isGameView) {
        VE::ParticleSystem::Get().Render(gizmoShader.ID, vp, cubeVAO);
        VE::DebugDraw::Get().Render(gizmoShader.ID, vp, (float)glfwGetTime());
    }
}

bool DragFloat3XYZ(const char* label,float* v,float speed=0.05f){
    bool changed=false;
    ImGui::PushID(label);
    float w=(ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize("X").x*3-ImGui::GetStyle().ItemSpacing.x*5)/3;
    ImGui::TextDisabled("%s",label);
    ImGui::SameLine(80);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.5f,.1f,.1f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##x",&v[0],speed))changed=true;
    ImGui::PopStyleColor();ImGui::SameLine(0,3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.1f,.4f,.1f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##y",&v[1],speed))changed=true;
    ImGui::PopStyleColor();ImGui::SameLine(0,3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.1f,.2f,.5f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##z",&v[2],speed))changed=true;
    ImGui::PopStyleColor();
    ImGui::PopID();
    return changed;
}
