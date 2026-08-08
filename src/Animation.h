#pragma once
// ── Покадровая анимация объектов (вынесено из main.cpp) ──

// ═══════════════════════════════════════════════════════
//   КАСТОМНАЯ ПОКАДРОВАЯ АНИМАЦИЯ ОБЪЕКТА (как Animation window в Unity)
//   Работает с ЛЮБЫМ объектом (куб, сфера, свет...) — двигает/крутит/
//   масштабирует его по ключевым кадрам. Независима от скелетной
//   анимации импортированных моделей (та живёт в Model.h).
// ═══════════════════════════════════════════════════════
struct ObjectKeyframe {
    float time = 0.f; // секунды от начала клипа
    glm::vec3 pos{0}, rot{0}, scale{1};
};
struct ObjectAnimClip {
    std::string name = "Clip";
    std::vector<ObjectKeyframe> keys;
    bool loop = true;
};

// Линейная интерполяция позы объекта в момент t внутри клипа.
// Кадры должны быть отсортированы по времени (сортируем при добавлении).
void SampleObjectClip(ObjectAnimClip& clip, float t, glm::vec3& outPos, glm::vec3& outRot, glm::vec3& outScale) {
    if (clip.keys.empty()) { outPos=glm::vec3(0); outRot=glm::vec3(0); outScale=glm::vec3(1); return; }
    if (clip.keys.size()==1) { outPos=clip.keys[0].pos; outRot=clip.keys[0].rot; outScale=clip.keys[0].scale; return; }
    if (t <= clip.keys.front().time) { outPos=clip.keys.front().pos; outRot=clip.keys.front().rot; outScale=clip.keys.front().scale; return; }
    if (t >= clip.keys.back().time)  { outPos=clip.keys.back().pos;  outRot=clip.keys.back().rot;  outScale=clip.keys.back().scale;  return; }
    for (size_t i=0;i+1<clip.keys.size();i++) {
        if (t >= clip.keys[i].time && t <= clip.keys[i+1].time) {
            float span = clip.keys[i+1].time - clip.keys[i].time;
            float f = span>0.f ? (t-clip.keys[i].time)/span : 0.f;
            outPos   = glm::mix(clip.keys[i].pos,   clip.keys[i+1].pos,   f);
            outRot   = glm::mix(clip.keys[i].rot,   clip.keys[i+1].rot,   f);
            outScale = glm::mix(clip.keys[i].scale, clip.keys[i+1].scale, f);
            return;
        }
    }
}

