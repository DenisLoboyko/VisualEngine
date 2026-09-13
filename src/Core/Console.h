#pragma once
// Console.h - dev console Source 2 (CS2) style. Header-only.
#include "imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <variant>

namespace VE {

enum class ConType { Int, Float, Bool, Str };
enum class LogLevel { Info, Warn, Error, Cmd, Echo };

struct ConVar {
    std::string name, desc;
    ConType type = ConType::Str;
    std::variant<int,float,bool,std::string> val;
    std::string Str() const {
        if (auto p = std::get_if<int>(&val)) return std::to_string(*p);
        if (auto p = std::get_if<float>(&val)) { std::ostringstream s; s << *p; return s.str(); }
        if (auto p = std::get_if<bool>(&val)) return *p ? "1" : "0";
        if (auto p = std::get_if<std::string>(&val)) return *p;
        return "";
    }
};

struct ConCmd {
    std::string name, desc, usage;
    std::function<void(const std::vector<std::string>&)> fn;
};

struct LogLine { LogLevel lvl; std::string text; };

class Console {
public:
    static Console& Get() { static Console c; return c; }
    void Init();
    void Print(LogLevel l, const std::string& t) {
        m_lines.push_back({l, t});
        if (m_lines.size() > 2000) m_lines.erase(m_lines.begin(), m_lines.begin() + (long)m_lines.size() - 2000);
        m_scroll = true;
    }
    void Execute(const std::string& line);
    void Toggle() { m_open = !m_open; if (m_open) m_focus = true; }
    void SetAlias(const std::string& n, const std::string& s) { m_aliases[Lc(n)] = s; }
    bool IsOpen() const { return m_open; }
    void Render();
    void RenderContents(const char* idp = "win");
    void AddInt(const std::string& n, int d, const std::string& desc = "") { m_vars[Lc(n)] = ConVar{n, desc, ConType::Int, d}; }
    void AddFloat(const std::string& n, float d, const std::string& desc = "") { m_vars[Lc(n)] = ConVar{n, desc, ConType::Float, d}; }
    void AddBool(const std::string& n, bool d, const std::string& desc = "") { m_vars[Lc(n)] = ConVar{n, desc, ConType::Bool, d}; }
    void AddStr(const std::string& n, const std::string& d, const std::string& desc = "") { m_vars[Lc(n)] = ConVar{n, desc, ConType::Str, d}; }
    void AddCmd(const std::string& n, const std::string& desc, const std::string& usage, std::function<void(const std::vector<std::string>&)> fn) { m_cmds[Lc(n)] = ConCmd{n, desc, usage, std::move(fn)}; }
    int GetInt(const std::string& n, int d = 0) const { auto i = m_vars.find(Lc(n)); return (i != m_vars.end() && std::holds_alternative<int>(i->second.val)) ? std::get<int>(i->second.val) : d; }
    float GetFloat(const std::string& n, float d = 0.f) const { auto i = m_vars.find(Lc(n)); return (i != m_vars.end() && std::holds_alternative<float>(i->second.val)) ? std::get<float>(i->second.val) : d; }
    bool GetBool(const std::string& n, bool d = false) const { auto i = m_vars.find(Lc(n)); return (i != m_vars.end() && std::holds_alternative<bool>(i->second.val)) ? std::get<bool>(i->second.val) : d; }
    std::string GetStr(const std::string& n, const std::string& d = "") const { auto i = m_vars.find(Lc(n)); return (i != m_vars.end() && std::holds_alternative<std::string>(i->second.val)) ? std::get<std::string>(i->second.val) : d; }
    void SetInt(const std::string& n, int v) { auto i = m_vars.find(Lc(n)); if (i != m_vars.end()) i->second.val = v; }
    void SetFloat(const std::string& n, float v) { auto i = m_vars.find(Lc(n)); if (i != m_vars.end()) i->second.val = v; }
    void SetBool(const std::string& n, bool v) { auto i = m_vars.find(Lc(n)); if (i != m_vars.end()) i->second.val = v; }
    void SetStr(const std::string& n, const std::string& v) { auto i = m_vars.find(Lc(n)); if (i != m_vars.end()) i->second.val = v; }
    std::string Lc(const std::string& s) const { std::string r = s; std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return (char)std::tolower(c); }); return r; }
    std::vector<std::string> Tokenize(const std::string& s) const;
private:
    Console() = default;
    void RegisterBuiltin();
    std::unordered_map<std::string, ConVar> m_vars;
    std::unordered_map<std::string, ConCmd> m_cmds;
    std::vector<LogLine> m_lines;
    std::vector<std::string> m_hist;
    std::unordered_map<std::string, std::string> m_aliases;
    int m_histIdx = -1;
    char m_input[1024] = {0};
    bool m_open = false, m_scroll = false, m_focus = false, m_init = false;
};

inline std::vector<std::string> Console::Tokenize(const std::string& s) const {
    std::vector<std::string> out; std::string cur; bool q = false;
    for (char c : s) {
        if (c == '"') { q = !q; continue; }
        if ((c == ' ' || c == '\t') && !q) { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline void Console::Execute(const std::string& line) {
    if (line.empty()) return;
    if (line.find(';') != std::string::npos) {
        std::string cur;
        for (char ch : line) { if (ch == ';') { Execute(cur); cur.clear(); } else cur.push_back(ch); }
        if (!cur.empty()) Execute(cur);
        return;
    }
    Print(LogLevel::Cmd, "] " + line);
    m_hist.push_back(line); m_histIdx = -1;
    auto t = Tokenize(line); if (t.empty()) return;
    std::string c = Lc(t[0]);
    auto ia = m_aliases.find(c);
    if (ia != m_aliases.end()) { std::string full = ia->second; for (size_t i = 1; i < t.size(); ++i) full += " " + t[i]; Execute(full); return; }
    auto ic = m_cmds.find(c);
    if (ic != m_cmds.end()) { ic->second.fn(t); return; }
    auto iv = m_vars.find(c);
    if (iv != m_vars.end()) {
        if (t.size() < 2) { Print(LogLevel::Echo, iv->second.name + " = " + iv->second.Str() + (iv->second.desc.empty() ? "" : "  // " + iv->second.desc)); return; }
        try {
            switch (iv->second.type) {
            case ConType::Int: iv->second.val = std::stoi(t[1]); break;
            case ConType::Float: iv->second.val = std::stof(t[1]); break;
            case ConType::Bool: iv->second.val = (t[1]=="1"||t[1]=="true"||t[1]=="on"); break;
            case ConType::Str: iv->second.val = t[1]; break;
            }
            Print(LogLevel::Echo, iv->second.name + " -> " + iv->second.Str());
        } catch (...) { Print(LogLevel::Error, "bad value for " + iv->second.name); }
        return;
    }
    Print(LogLevel::Error, "unknown command: " + t[0] + " (type 'help')");
}

inline void Console::RegisterBuiltin() {
    AddCmd("help", "list all commands and cvars", "help [name]", [this](const std::vector<std::string>& a) {
        if (a.size() > 1) {
            std::string n = Lc(a[1]);
            auto ic = m_cmds.find(n); if (ic != m_cmds.end()) { Print(LogLevel::Echo, ic->second.name + " - " + ic->second.desc + " | " + ic->second.usage); return; }
            auto iv = m_vars.find(n); if (iv != m_vars.end()) { Print(LogLevel::Echo, iv->second.name + " = " + iv->second.Str() + "  // " + iv->second.desc); return; }
            Print(LogLevel::Error, "not found: " + a[1]); return;
        }
        Print(LogLevel::Echo, "---- commands ----");
        for (auto& kv : m_cmds) Print(LogLevel::Echo, "  " + kv.second.name + "  - " + kv.second.desc);
        Print(LogLevel::Echo, "---- cvars ----");
        for (auto& kv : m_vars) Print(LogLevel::Echo, "  " + kv.second.name + " = " + kv.second.Str() + "  // " + kv.second.desc);
    });
    AddCmd("clear", "clear console", "", [this](const std::vector<std::string>&) { m_lines.clear(); });
    AddCmd("echo", "print text", "echo <text>", [this](const std::vector<std::string>& a) {
        std::string s; for (size_t i = 1; i < a.size(); ++i) { if (i > 1) s += ' '; s += a[i]; } Print(LogLevel::Echo, s);
    });
    AddCmd("find", "search commands/cvars", "find <part>", [this](const std::vector<std::string>& a) {
        if (a.size() < 2) { Print(LogLevel::Error, "usage: find <part>"); return; }
        std::string p = Lc(a[1]);
        for (auto& kv : m_cmds) if (kv.first.find(p) != std::string::npos) Print(LogLevel::Echo, "[cmd] " + kv.second.name + " - " + kv.second.desc);
        for (auto& kv : m_vars) if (kv.first.find(p) != std::string::npos) Print(LogLevel::Echo, "[var] " + kv.second.name + " = " + kv.second.Str());
    });
    AddBool("r_wireframe", false, "wireframe mode");
    AddBool("r_showfps", true, "show FPS");
    AddBool("r_vsync", true, "vertical sync");
    AddInt("r_msaa", 4, "MSAA samples");
    AddFloat("r_fov", 60.f, "camera FOV");
    AddBool("r_bloom", true, "bloom");
    AddBool("r_tonemap", true, "ACES tonemap");
    AddBool("r_skybox", true, "skybox");
    AddBool("r_grid", true, "editor grid");
    AddFloat("phys_gravity", -9.81f, "gravity m/s^2");
    AddFloat("phys_timescale", 1.f, "time scale");
    AddBool("phys_pause", false, "pause physics");
    AddFloat("snd_master", 1.f, "master volume");
    AddFloat("snd_music", 1.f, "music volume");
    AddBool("snd_mute", false, "mute all");
    AddFloat("cam_speed", 5.f, "editor cam speed");
    AddFloat("cam_sens", 0.2f, "mouse sens");
    AddCmd("scene_list", "list scene objects (hook)", "", [](const std::vector<std::string>&) {});
    AddCmd("lua_list", "list attached scripts (hook)", "", [](const std::vector<std::string>&) {});
    AddCmd("ed_play", "enter play mode (hook)", "", [](const std::vector<std::string>&) {});
    AddCmd("ed_stop", "exit play mode (hook)", "", [](const std::vector<std::string>&) {});
    AddCmd("quit", "request exit (hook)", "", [](const std::vector<std::string>&) {});
}

inline void Console::Init() {
    if (m_init) return;
    RegisterBuiltin();
    m_init = true;
}

inline void Console::Render() {
    static bool s_first = true;
    if (!m_open) { s_first = true; return; }
    ImGuiIO& io = ImGui::GetIO();
    if (s_first) { ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.55f, io.DisplaySize.y * 0.75f), ImGuiCond_Always); s_first = false; }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.11f, 0.11f, 0.11f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.075f, 0.075f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.13f, 0.13f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.28f, 0.28f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.88f, 1.f));
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Console", &m_open, wf)) {
        RenderContents("win");
    }
    ImGui::End();
    ImGui::PopStyleColor(7);
}

inline void Console::RenderContents(const char* idp) {
    ImGui::PushID(idp);
    if (true) {
        
        ImGui::BeginChild("##log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (auto& ln : m_lines) {
            ImVec4 col(0.85f, 0.90f, 0.95f, 1.f);
            if (ln.lvl == LogLevel::Warn) col = ImVec4(1.f, 0.85f, 0.3f, 1.f);
            if (ln.lvl == LogLevel::Error) col = ImVec4(1.f, 0.4f, 0.4f, 1.f);
            if (ln.lvl == LogLevel::Cmd) col = ImVec4(0.90f, 0.90f, 0.88f, 1.f);
            if (ln.lvl == LogLevel::Echo) col = ImVec4(0.7f, 0.95f, 0.7f, 1.f);
            ImGui::TextColored(col, "%s", ln.text.c_str());
        }
        if (m_scroll) { ImGui::SetScrollHereY(1.f); m_scroll = false; }
        ImGui::EndChild();
        auto t = Tokenize(m_input);
        if (!t.empty()) {
            std::string p = Lc(t[0]); std::string hint; int n = 0;
            for (auto& kv : m_cmds) if (kv.first.find(p) == 0 && n < 6) { if (n) hint += "   "; hint += kv.second.name; ++n; }
            for (auto& kv : m_vars) if (kv.first.find(p) == 0 && n < 6) { if (n) hint += "   "; hint += kv.second.name; ++n; }
            if (!hint.empty()) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.57f, 1.f)); ImGui::TextUnformatted(hint.c_str()); ImGui::PopStyleColor(); }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.88f, 1.f));
        ImGui::TextUnformatted("]");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (m_focus) { ImGui::SetKeyboardFocusHere(); m_focus = false; }
        if (ImGui::InputText("##in", m_input, sizeof(m_input), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            [](ImGuiInputTextCallbackData* d) -> int {
                if (d->EventFlag != ImGuiInputTextFlags_CallbackHistory) return 0;
                auto& h = Console::Get().m_hist; auto& idx = Console::Get().m_histIdx;
                if (h.empty()) return 0;
                if (d->EventKey == ImGuiKey_UpArrow) { if (idx < (int)h.size() - 1) ++idx; }
                else if (d->EventKey == ImGuiKey_DownArrow) { if (idx > 0) --idx; else { idx = -1; d->DeleteChars(0, d->BufTextLen); return 0; } }
                else return 0;
                d->DeleteChars(0, d->BufTextLen);
                d->InsertChars(0, h[h.size() - 1 - idx].c_str());
                return 0;
            })) {
            Execute(m_input); m_input[0] = 0; m_focus = true;
        }
    }
    ImGui::PopID();
}

} // namespace VE










