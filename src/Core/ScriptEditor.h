#pragma once
#include <imgui.h>
#include <string>
#include <fstream>
#include <sstream>
#include "LuaEngine.h"

namespace VE {

    class ScriptEditor
    {
    public:
        bool isOpen = false;
        std::string filePath;
        std::string title = "Script Editor";
        char code[1024 * 64] = {};
        std::string consoleOutput;
        LuaEngine lua;

        void open(const std::string& path)
        {
            filePath = path;
            isOpen = true;

            // Извлечь имя файла
            size_t pos = path.find_last_of("\\/");
            title = pos != std::string::npos ? path.substr(pos+1) : path;

            // Загрузить файл
            std::ifstream f(path);
            if (f.is_open()) {
                std::stringstream ss;
                ss << f.rdbuf();
                std::string content = ss.str();
                strncpy_s(code, content.c_str(), sizeof(code)-1);
            } else {
                // Новый файл — шаблон
                std::string tmpl =
                    "-- " + title + "\n\n"
                    "function onStart()\n"
                    "    print('Script started!')\n"
                    "end\n\n"
                    "function onUpdate(dt)\n"
                    "    -- this.x = this.x + dt\n"
                    "end\n";
                strncpy_s(code, tmpl.c_str(), sizeof(code)-1);
            }
        }

        void save()
        {
            if (filePath.empty()) return;
            std::ofstream f(filePath);
            if (f.is_open()) {
                f << code;
                consoleOutput += "[Saved] " + title + "\n";
            }
        }

        void draw()
        {
            if (!isOpen) return;

            ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(200, 100), ImGuiCond_FirstUseEver);

            std::string windowTitle = "Script: " + title + "###ScriptEditor";
            ImGui::Begin(windowTitle.c_str(), &isOpen,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

            // Menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::MenuItem("Save (Ctrl+S)")) save();
                if (ImGui::MenuItem("Run")) runScript();
                if (ImGui::MenuItem("Clear Console")) consoleOutput.clear();
                ImGui::EndMenuBar();
            }

            // Keyboard shortcut Ctrl+S
            if (ImGui::IsWindowFocused() &&
                ImGui::GetIO().KeyCtrl &&
                ImGui::IsKeyPressed(ImGuiKey_S)) {
                save();
            }

            // Code editor
            float codeHeight = ImGui::GetContentRegionAvail().y - 140;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f,0.08f,0.10f,1));
            ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.85f,0.95f,0.75f,1));
            ImGui::InputTextMultiline(
                "##code", code, sizeof(code),
                ImVec2(-1, codeHeight),
                ImGuiInputTextFlags_AllowTabInput
            );
            ImGui::PopStyleColor(2);

            // Run button
            ImGui::Separator();
            if (ImGui::Button("▶  Run Script", ImVec2(120, 28))) runScript();
            ImGui::SameLine();
            if (ImGui::Button("💾 Save", ImVec2(80, 28))) save();
            ImGui::SameLine();
            if (ImGui::Button("🗑 Clear", ImVec2(80, 28))) consoleOutput.clear();

            // Console output
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f,0.75f,1,1), "Console:");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f,0.05f,0.07f,1));
            ImGui::BeginChild("##console", ImVec2(-1, 90), true);

            // Цветной вывод
            std::istringstream stream(consoleOutput);
            std::string line;
            while (std::getline(stream, line)) {
                ImVec4 col = ImVec4(0.85f,0.85f,0.85f,1);
                if (line.find("[Error]") != std::string::npos)
                    col = ImVec4(1,0.3f,0.3f,1);
                else if (line.find("[OK]") != std::string::npos)
                    col = ImVec4(0.3f,1,0.4f,1);
                else if (line.find("[Saved]") != std::string::npos)
                    col = ImVec4(0.4f,0.8f,1,1);
                ImGui::TextColored(col, "%s", line.c_str());
            }
            // Авто скролл вниз
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::End();
        }

    private:
        void runScript()
        {
            save();
            consoleOutput += "--- Running " + title + " ---\n";
            bool ok = lua.loadScript(std::string(code));
            if (ok) {
                lua.callOnStart();
                consoleOutput += "[OK] Script executed successfully\n";
            } else {
                consoleOutput += lua.printOutput + "\n";
            }
        }
    };
}