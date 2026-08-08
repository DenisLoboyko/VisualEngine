#pragma once
// =========================================================
//  UndoSystem.h  —  Undo/Redo для редактора VisualEngine
//
//  Кладём файл: src/Core/UndoSystem.h
//
//  Идея: НЕ пишем отдельный класс-команду под каждое действие
//  (Move, Rotate, Scale, ChangeColor, ChangeMass, ...) — вместо
//  этого один шаблонный PropertyChangeCommand<T> хранит
//  "было/стало" + лямбду-сеттер, а LambdaCommand — для действий
//  посложнее (создание/удаление объекта), где проще описать
//  Undo/Redo двумя функциями, чем городить структуру.
//
//  Использование (после того как изменение уже применено к объекту):
//
//    VE::UndoSystem::Get().Push(std::make_unique<VE::PropertyChangeCommand<glm::vec3>>(
//        [&obj](const glm::vec3& v){ obj.pos = v; },   // сеттер
//        oldPos, obj.pos,                               // было / стало
//        "Move " + obj.name                             // имя для UI
//    ));
//
//    VE::UndoSystem::Get().Undo();   // например по Ctrl+Z
//    VE::UndoSystem::Get().Redo();   // например по Ctrl+Y
// =========================================================

#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace VE {

    // ── Базовый интерфейс любой обратимой операции ──
    class IUndoCommand
    {
    public:
        virtual ~IUndoCommand() = default;
        virtual void Undo() = 0;
        virtual void Redo() = 0;
        virtual const char* Name() const { return "Action"; }
    };

    // ── "Было -> стало" через сеттер. Работает для любого поля:
    //    позиция, поворот, масштаб, цвет, масса, intensity света и т.д. ──
    template<typename T>
    class PropertyChangeCommand : public IUndoCommand
    {
    public:
        PropertyChangeCommand(std::function<void(const T&)> setter,
                               T before, T after,
                               std::string name = "Edit")
            : m_Setter(std::move(setter))
            , m_Before(std::move(before))
            , m_After(std::move(after))
            , m_Name(std::move(name))
        {}

        void Undo() override { m_Setter(m_Before); }
        void Redo() override { m_Setter(m_After); }
        const char* Name() const override { return m_Name.c_str(); }

    private:
        std::function<void(const T&)> m_Setter;
        T m_Before, m_After;
        std::string m_Name;
    };

    // ── Команда на двух лямбдах — для Create/Delete объекта и
    //    прочих действий, которые не сводятся к одному полю. ──
    class LambdaCommand : public IUndoCommand
    {
    public:
        LambdaCommand(std::function<void()> undoFn,
                       std::function<void()> redoFn,
                       std::string name = "Action")
            : m_Undo(std::move(undoFn))
            , m_Redo(std::move(redoFn))
            , m_Name(std::move(name))
        {}

        void Undo() override { m_Undo(); }
        void Redo() override { m_Redo(); }
        const char* Name() const override { return m_Name.c_str(); }

    private:
        std::function<void()> m_Undo, m_Redo;
        std::string m_Name;
    };

    // ── Центральный стек. Синглтон — как у вас сделаны Physics::Get(),
    //    ParticleSystem::Get() и т.д., чтобы стиль совпадал. ──
    class UndoSystem
    {
    public:
        static UndoSystem& Get() { static UndoSystem instance; return instance; }

        UndoSystem(const UndoSystem&)            = delete;
        UndoSystem& operator=(const UndoSystem&) = delete;

        static constexpr size_t kMaxHistory = 200;

        // Кладём УЖЕ применённое изменение в историю.
        // Сама Push() ничего не выполняет и не трогает состояние —
        // ожидается, что redo-эффект уже случился до вызова.
        void Push(std::unique_ptr<IUndoCommand> cmd)
        {
            m_UndoStack.push_back(std::move(cmd));
            if (m_UndoStack.size() > kMaxHistory)
                m_UndoStack.erase(m_UndoStack.begin());
            m_RedoStack.clear(); // новое действие обнуляет "будущее"
        }

        void Undo()
        {
            if (m_UndoStack.empty()) return;
            auto cmd = std::move(m_UndoStack.back());
            m_UndoStack.pop_back();
            cmd->Undo();
            m_RedoStack.push_back(std::move(cmd));
        }

        void Redo()
        {
            if (m_RedoStack.empty()) return;
            auto cmd = std::move(m_RedoStack.back());
            m_RedoStack.pop_back();
            cmd->Redo();
            m_UndoStack.push_back(std::move(cmd));
        }

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

        const char* PeekUndoName() const { return m_UndoStack.empty() ? "" : m_UndoStack.back()->Name(); }
        const char* PeekRedoName() const { return m_RedoStack.empty() ? "" : m_RedoStack.back()->Name(); }

        // Вызывать при загрузке/смене сцены — иначе Undo сможет
        // "откатить" в объект, которого уже нет в новой сцене.
        void Clear() { m_UndoStack.clear(); m_RedoStack.clear(); }

    private:
        UndoSystem() = default;
        std::vector<std::unique_ptr<IUndoCommand>> m_UndoStack;
        std::vector<std::unique_ptr<IUndoCommand>> m_RedoStack;
    };

} // namespace VE