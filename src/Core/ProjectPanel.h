#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

namespace VE {

    class ProjectPanel
    {
    public:
        std::string currentPath;
        std::string rootPath;
        std::string selectedFile;
        std::function<void(const std::string&)> onModelDropped;
        std::function<void(const std::string&)> onScriptOpened;

        ProjectPanel(const std::string& root) : rootPath(root), currentPath(root) {}

        void Draw()
        {
            ImGui::SetNextWindowPos(ImVec2(0, 440), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(240, 280), ImGuiCond_Always);
            ImGui::Begin("Project", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

            // Breadcrumb
            std::string rel = currentPath.size() > rootPath.size()
                ? currentPath.substr(rootPath.size())
                : "\\";
            ImGui::TextColored(ImVec4(0.5f, 0.75f, 1, 1), "Project%s", rel.c_str());

            // Back button
            if (currentPath != rootPath) {
                if (ImGui::Button("<- Back")) {
                    currentPath = fs::path(currentPath).parent_path().string();
                    selectedFile = "";
                }
                ImGui::SameLine();
            }

            ImGui::Separator();

            // List files
            if (!fs::exists(currentPath)) {
                ImGui::TextDisabled("Folder not found");
                ImGui::End();
                return;
            }

            for (auto& entry : fs::directory_iterator(currentPath))
            {
                std::string name = entry.path().filename().string();
                std::string ext  = entry.path().extension().string();
                bool isDir = entry.is_directory();

                // Icon
                std::string icon = isDir ? "[D] " :
                    (ext == ".obj" || ext == ".fbx") ? "[M] " :
                    (ext == ".lua") ? "[S] " :
                    (ext == ".png" || ext == ".jpg") ? "[T] " : "[F] ";

                bool selected = (selectedFile == entry.path().string());

                ImVec4 col = isDir ? ImVec4(0.9f,0.8f,0.4f,1) :
                    (ext==".obj"||ext==".fbx") ? ImVec4(0.5f,0.8f,1,1) :
                    (ext==".lua") ? ImVec4(0.5f,1,0.6f,1) :
                    (ext==".png"||ext==".jpg") ? ImVec4(1,0.7f,0.5f,1) :
                    ImVec4(0.7f,0.7f,0.7f,1);

                ImGui::PushStyleColor(ImGuiCol_Text, col);
                if (ImGui::Selectable((icon + name).c_str(), selected)) {
                    selectedFile = entry.path().string();
                    if (isDir) currentPath = entry.path().string();
                }
                ImGui::PopStyleColor();

                // Double click or button to load
                if (selected && !isDir) {
                    if (ext == ".obj" || ext == ".fbx") {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Add to Scene")) {
                            if (onModelDropped) onModelDropped(selectedFile);
                        }
                    }
                    if (ext == ".lua") {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Open")) {
                            if (onScriptOpened) onScriptOpened(selectedFile);
                        }
                    }
                }
            }

            ImGui::Separator();

            // New script button
            static char newScriptName[64] = "MyScript";
            ImGui::SetNextItemWidth(120);
            ImGui::InputText("##ns", newScriptName, sizeof(newScriptName));
            ImGui::SameLine();
            if (ImGui::Button("New Script")) {
                std::string scriptPath = rootPath + "\\Assets\\Scripts\\" + newScriptName + ".lua";
                std::ofstream f(scriptPath);
                if (f.is_open()) {
                    f << "-- " << newScriptName << "\n";
                    f << "function onStart()\n    print('Hello from " << newScriptName << "')\nend\n\n";
                    f << "function onUpdate(dt)\nend\n";
                    f.close();
                    currentPath = rootPath + "\\Assets\\Scripts";
                }
            }

            ImGui::End();
        }
    };
}