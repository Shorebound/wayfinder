#pragma once

#include "Assert.h"
#include "wayfinder_exports.h"

#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    /**
     * @class Subsystem
     * @brief Base class for lifecycle-managed engine services.
     *
     * Subsystems are automatic: register a type with a SubsystemCollection and
     * the collection handles construction, initialisation, and shutdown.
     *
     * Each lifetime scope (Game, Scene, Engine, ...) is represented by a
     * distinct base class that derives from Subsystem. A SubsystemCollection
     * is templated on that base, so the type system enforces scope membership
     * without enums.
     *
     * Access any live subsystem via the scope's static accessor:
     * @code
     *   auto& tags = GameSubsystems::Get<GameplayTagRegistry>();
     * @endcode
     *
     * No per-subsystem static pointers or boilerplate needed. Just derive
     * from the appropriate scope base and register with the collection.
     *
     * @code
     * class MySubsystem : public GameSubsystem {
     * public:
     *     void Initialise() override { ... }
     *     void Shutdown()   override { ... }
     * };
     * @endcode
     */
    class WAYFINDER_API Subsystem
    {
    public:
        virtual ~Subsystem() = default;

        /// Called after construction by the owning SubsystemCollection.
        virtual void Initialise() {}

        /// Called in reverse-registration order when the collection shuts down.
        virtual void Shutdown() {}

        /// Return false to skip creation for this run. Checked once before
        /// Initialise(). Default is true (always create).
        virtual bool ShouldCreate() const { return true; }
    };

    // -----------------------------------------------------------------
    //  Scope base classes.
    //  Add new scopes by deriving from Subsystem and giving the owning
    //  object (Game, Scene, Application, ...) its own SubsystemCollection
    //  templated on that base.
    // -----------------------------------------------------------------

    /**
     * @class GameSubsystem
     * @brief A subsystem whose lifetime is tied to the Game.
     *
     * Destroyed in reverse order when Game::Shutdown() is called.
     */
    class WAYFINDER_API GameSubsystem : public Subsystem {};

    // Future scopes (uncomment when the owning object is ready):
    // class WAYFINDER_API SceneSubsystem    : public Subsystem {};
    // class WAYFINDER_API EngineSubsystem   : public Subsystem {};

    // -----------------------------------------------------------------
    //  SubsystemCollection
    // -----------------------------------------------------------------

    /**
     * @class SubsystemCollection
     * @brief Owns and manages subsystems of a given scope.
     *
     * Registered subsystem types are instantiated during Initialise()
     * and torn down in reverse order during Shutdown(). Access a live
     * instance with Get<T>().
     *
     * @tparam TBase The scope base class (e.g. GameSubsystem).
     */
    template <typename TBase>
    class SubsystemCollection
    {
    public:
        using FactoryFn = std::unique_ptr<TBase> (*)();

        /// Register a subsystem type. It will be created during Initialise().
        /// Returns false if the type is already registered.
        template <typename T>
        bool Register()
        {
            static_assert(std::is_base_of_v<TBase, T>, "T must derive from the collection's scope base");
            const std::type_index type{typeid(T)};
            for (const auto& entry : m_factories)
            {
                if (entry.Type == type)
                    return false;
            }
            m_factories.push_back({type,
                                   []() -> std::unique_ptr<TBase> { return std::make_unique<T>(); }});
            return true;
        }

        /// Register a subsystem from an external factory (e.g. from ModuleRegistry).
        /// Returns false if the type is already registered.
        bool Register(std::type_index type, FactoryFn factory)
        {
            for (const auto& entry : m_factories)
            {
                if (entry.Type == type)
                    return false;
            }
            m_factories.push_back({type, factory});
            return true;
        }

        /// Create all registered subsystems and call Initialise() on each.
        void Initialise()
        {
            for (auto& [type, factory] : m_factories)
            {
                auto subsystem = factory();
                if (!subsystem->ShouldCreate())
                    continue;

                subsystem->Initialise();
                m_lookup[type] = subsystem.get();
                m_subsystems.push_back(std::move(subsystem));
            }
        }

        /// Shutdown and destroy all subsystems in reverse order.
        void Shutdown()
        {
            for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it)
                (*it)->Shutdown();

            m_lookup.clear();
            m_subsystems.clear();
        }

        /// Retrieve a live subsystem by type. Returns nullptr if not present.
        template <typename T>
        T* Get()
        {
            if (auto it = m_lookup.find(std::type_index(typeid(T))); it != m_lookup.end())
                return static_cast<T*>(it->second);
            return nullptr;
        }

        /// Retrieve a live subsystem by type (const). Returns nullptr if not present.
        template <typename T>
        const T* Get() const
        {
            if (auto it = m_lookup.find(std::type_index(typeid(T))); it != m_lookup.end())
                return static_cast<const T*>(it->second);
            return nullptr;
        }

    private:
        struct FactoryEntry
        {
            std::type_index Type;
            FactoryFn Factory;
        };

        std::vector<FactoryEntry> m_factories;
        std::vector<std::unique_ptr<TBase>> m_subsystems;
        std::unordered_map<std::type_index, TBase*> m_lookup;
    };

    // -----------------------------------------------------------------
    //  GameSubsystems — static accessor for the Game-scoped collection.
    //
    //  Usage:  auto& tags = GameSubsystems::Get<GameplayTagRegistry>();
    //
    //  Game calls Bind/Unbind during init/shutdown. No individual
    //  subsystem needs to maintain its own static pointer.
    // -----------------------------------------------------------------

    /**
     * @class GameSubsystems
     * @brief Static accessor for the active Game-scoped SubsystemCollection.
     *
     * This eliminates per-subsystem singleton boilerplate. Any code that
     * needs a game subsystem calls GameSubsystems::Get<T>() without
     * needing a reference to Game or the world.
     */
    class WAYFINDER_API GameSubsystems
    {
    public:
        /// Retrieve a live game subsystem by type.
        /// Asserts if the collection has not been bound (Game not initialised).
        template <typename T>
        static T& Get()
        {
            WAYFINDER_ASSERT(s_collection, "GameSubsystems::Get() called before Game initialisation or after shutdown");
            auto* subsystem = s_collection->Get<T>();
            WAYFINDER_ASSERT(subsystem, "Requested GameSubsystem type is not registered");
            return *subsystem;
        }

        /// Retrieve a live game subsystem, or nullptr if not registered.
        template <typename T>
        static T* Find()
        {
            return s_collection ? s_collection->Get<T>() : nullptr;
        }

    private:
        friend class Game;
        static void Bind(SubsystemCollection<GameSubsystem>* collection) { s_collection = collection; }
        static void Unbind() { s_collection = nullptr; }

        static inline SubsystemCollection<GameSubsystem>* s_collection = nullptr;
    };

} // namespace Wayfinder
