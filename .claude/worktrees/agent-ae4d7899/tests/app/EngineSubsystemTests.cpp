#include "app/AppSubsystem.h"
#include "app/EngineConfig.h"
#include "app/EngineContext.h"
#include "app/InputSubsystem.h"
#include "app/RenderDeviceSubsystem.h"
#include "app/RendererSubsystem.h"
#include "app/SubsystemManifest.h"
#include "app/SubsystemRegistry.h"
#include "app/TimeSubsystem.h"
#include "app/WindowSubsystem.h"
#include "gameplay/Capability.h"
#include "gameplay/NativeTag.h"
#include "gameplay/TagRegistry.h"
#include "platform/Input.h"
#include "platform/Time.h"
#include "platform/Window.h"
#include "rendering/backend/RenderDevice.h"
#include "rendering/pipeline/Renderer.h"

#include <doctest/doctest.h>

#include <string>
#include <typeindex>
#include <vector>

namespace Wayfinder::Tests
{
    // ── Lifecycle tracking ────────────────────────────────────

    struct SubsystemLifecycleLog
    {
        std::vector<std::string> Events;
    };

    static SubsystemLifecycleLog* s_subsystemLog = nullptr;

    struct SubsystemLifecycleLogGuard
    {
        explicit SubsystemLifecycleLogGuard(SubsystemLifecycleLog& log) : m_previous(s_subsystemLog)
        {
            s_subsystemLog = &log;
        }
        ~SubsystemLifecycleLogGuard()
        {
            s_subsystemLog = m_previous;
        }
        SubsystemLifecycleLogGuard(const SubsystemLifecycleLogGuard&) = delete;
        auto operator=(const SubsystemLifecycleLogGuard&) -> SubsystemLifecycleLogGuard& = delete;

    private:
        SubsystemLifecycleLog* m_previous;
    };

    // ── Proxy subsystems for headless testing ────────────────
    // These mirror the real subsystems' descriptor patterns (capabilities,
    // dependencies) but have no-op Initialise/Shutdown for headless tests.

    class ProxyWindowSubsystem : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& /*context*/) -> Result<void> override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Window.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Window.Shutdown");
            }
        }
    };

    class ProxyInputSubsystem : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& /*context*/) -> Result<void> override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Input.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Input.Shutdown");
            }
        }
    };

    class ProxyTimeSubsystem : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& /*context*/) -> Result<void> override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Time.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Time.Shutdown");
            }
        }
    };

    class ProxyRenderDeviceSubsystem : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& /*context*/) -> Result<void> override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("RenderDevice.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("RenderDevice.Shutdown");
            }
        }
    };

    class ProxyRendererSubsystem : public AppSubsystem
    {
    public:
        auto Initialise(EngineContext& /*context*/) -> Result<void> override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Renderer.Init");
            }
            return {};
        }
        void Shutdown() override
        {
            if (s_subsystemLog)
            {
                s_subsystemLog->Events.push_back("Renderer.Shutdown");
            }
        }
    };

    // ── Fixtures ─────────────────────────────────────────────

    /// Ensures NativeTag constants are registered for capability tests.
    struct EngineSubsystemTagFixture
    {
        TagRegistry Registry;

        EngineSubsystemTagFixture()
        {
            NativeTag::RegisterAll(Registry);
        }
    };

    /// Builds the standard descriptor set matching the five engine subsystems.
    struct EngineSubsystemDescriptors
    {
        SubsystemDescriptor WindowDesc;
        SubsystemDescriptor InputDesc;
        SubsystemDescriptor TimeDesc;
        SubsystemDescriptor RenderDeviceDesc;
        SubsystemDescriptor RendererDesc;

        EngineSubsystemDescriptors()
        {
            CapabilitySet presentationCaps;
            presentationCaps.AddTag(Capability::Presentation);

            CapabilitySet renderingCaps;
            renderingCaps.AddTag(Capability::Rendering);

            WindowDesc = {.RequiredCapabilities = presentationCaps};
            InputDesc = {};
            TimeDesc = {};
            RenderDeviceDesc = {.RequiredCapabilities = renderingCaps, .DependsOn = Deps<ProxyWindowSubsystem>()};
            RendererDesc = {.RequiredCapabilities = renderingCaps, .DependsOn = Deps<ProxyRenderDeviceSubsystem>()};
        }
    };

    // ── Tests ────────────────────────────────────────────────

    TEST_SUITE("EngineSubsystems")
    {
        TEST_CASE("Five engine subsystems register via SubsystemRegistry")
        {
            EngineSubsystemTagFixture tags;

            CapabilitySet presentationCaps;
            presentationCaps.AddTag(Capability::Presentation);

            CapabilitySet renderingCaps;
            renderingCaps.AddTag(Capability::Rendering);

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<WindowSubsystem>({.RequiredCapabilities = presentationCaps});
            registry.Register<InputSubsystem>({});
            registry.Register<TimeSubsystem>({});
            registry.Register<RenderDeviceSubsystem>({.RequiredCapabilities = renderingCaps, .DependsOn = Deps<WindowSubsystem>()});
            registry.Register<RendererSubsystem>({.RequiredCapabilities = renderingCaps, .DependsOn = Deps<RenderDeviceSubsystem>()});

            auto result = registry.Finalise();
            REQUIRE(result.has_value());

            // Verify all five are registered in the manifest
            auto& manifest = *result;
            CHECK(manifest.IsRegistered<WindowSubsystem>());
            CHECK(manifest.IsRegistered<InputSubsystem>());
            CHECK(manifest.IsRegistered<TimeSubsystem>());
            CHECK(manifest.IsRegistered<RenderDeviceSubsystem>());
            CHECK(manifest.IsRegistered<RendererSubsystem>());
        }

        TEST_CASE("Dependency ordering preserves Window before RenderDevice before Renderer")
        {
            EngineSubsystemTagFixture tags;
            EngineSubsystemDescriptors descs;
            SubsystemLifecycleLog log;
            SubsystemLifecycleLogGuard guard(log);

            SubsystemRegistry<AppSubsystem> registry;
            // Register in reverse order to prove topological sort works
            registry.Register<ProxyRendererSubsystem>(std::move(descs.RendererDesc));
            registry.Register<ProxyRenderDeviceSubsystem>(std::move(descs.RenderDeviceDesc));
            registry.Register<ProxyWindowSubsystem>(std::move(descs.WindowDesc));
            registry.Register<ProxyInputSubsystem>(std::move(descs.InputDesc));
            registry.Register<ProxyTimeSubsystem>(std::move(descs.TimeDesc));

            auto manifestResult = registry.Finalise();
            REQUIRE(manifestResult.has_value());
            auto manifest = std::move(*manifestResult);

            // Initialise with full capabilities so all subsystems activate
            CapabilitySet fullCaps;
            fullCaps.AddTag(Capability::Presentation);
            fullCaps.AddTag(Capability::Rendering);
            fullCaps.AddTag(Capability::Simulation);

            EngineContext ctx;
            REQUIRE(manifest.Initialise(ctx, fullCaps));

            // Find positions of Window, RenderDevice, Renderer in init order
            int windowPos = -1;
            int renderDevicePos = -1;
            int rendererPos = -1;

            for (int i = 0; i < static_cast<int>(log.Events.size()); ++i)
            {
                if (log.Events[i] == "Window.Init")
                {
                    windowPos = i;
                }
                if (log.Events[i] == "RenderDevice.Init")
                {
                    renderDevicePos = i;
                }
                if (log.Events[i] == "Renderer.Init")
                {
                    rendererPos = i;
                }
            }

            // All three must have been initialised
            REQUIRE(windowPos >= 0);
            REQUIRE(renderDevicePos >= 0);
            REQUIRE(rendererPos >= 0);

            // Dependency ordering: Window < RenderDevice < Renderer
            CHECK(windowPos < renderDevicePos);
            CHECK(renderDevicePos < rendererPos);

            manifest.Shutdown();
        }

        TEST_CASE("Input and Time have no dependencies")
        {
            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ProxyInputSubsystem>({});
            registry.Register<ProxyTimeSubsystem>({});

            auto manifestResult = registry.Finalise();
            REQUIRE(manifestResult.has_value());
            auto manifest = std::move(*manifestResult);

            EngineContext ctx;
            REQUIRE(manifest.Initialise(ctx, CapabilitySet{}));

            // Both activate with empty capabilities (no required caps)
            CHECK(manifest.TryGet<ProxyInputSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyTimeSubsystem>() != nullptr);

            manifest.Shutdown();
        }

        TEST_CASE("Capability gating excludes rendering subsystems in headless mode")
        {
            EngineSubsystemTagFixture tags;
            EngineSubsystemDescriptors descs;

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ProxyWindowSubsystem>(std::move(descs.WindowDesc));
            registry.Register<ProxyInputSubsystem>(std::move(descs.InputDesc));
            registry.Register<ProxyTimeSubsystem>(std::move(descs.TimeDesc));
            registry.Register<ProxyRenderDeviceSubsystem>(std::move(descs.RenderDeviceDesc));
            registry.Register<ProxyRendererSubsystem>(std::move(descs.RendererDesc));

            auto manifestResult = registry.Finalise();
            REQUIRE(manifestResult.has_value());
            auto manifest = std::move(*manifestResult);

            // Headless mode: no Presentation or Rendering capabilities
            EngineContext ctx;
            REQUIRE(manifest.Initialise(ctx, CapabilitySet{}));

            // Window, RenderDevice, Renderer are NOT activated
            CHECK(manifest.TryGet<ProxyWindowSubsystem>() == nullptr);
            CHECK(manifest.TryGet<ProxyRenderDeviceSubsystem>() == nullptr);
            CHECK(manifest.TryGet<ProxyRendererSubsystem>() == nullptr);

            // Input and Time ARE activated (empty RequiredCapabilities = always active)
            CHECK(manifest.TryGet<ProxyInputSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyTimeSubsystem>() != nullptr);

            manifest.Shutdown();
        }

        TEST_CASE("Full capabilities activate all subsystems")
        {
            EngineSubsystemTagFixture tags;
            EngineSubsystemDescriptors descs;

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ProxyWindowSubsystem>(std::move(descs.WindowDesc));
            registry.Register<ProxyInputSubsystem>(std::move(descs.InputDesc));
            registry.Register<ProxyTimeSubsystem>(std::move(descs.TimeDesc));
            registry.Register<ProxyRenderDeviceSubsystem>(std::move(descs.RenderDeviceDesc));
            registry.Register<ProxyRendererSubsystem>(std::move(descs.RendererDesc));

            auto manifestResult = registry.Finalise();
            REQUIRE(manifestResult.has_value());
            auto manifest = std::move(*manifestResult);

            // Full capabilities: Presentation + Rendering + Simulation
            CapabilitySet fullCaps;
            fullCaps.AddTag(Capability::Presentation);
            fullCaps.AddTag(Capability::Rendering);
            fullCaps.AddTag(Capability::Simulation);

            EngineContext ctx;
            REQUIRE(manifest.Initialise(ctx, fullCaps));

            // All five subsystems are activated
            CHECK(manifest.TryGet<ProxyWindowSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyInputSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyTimeSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyRenderDeviceSubsystem>() != nullptr);
            CHECK(manifest.TryGet<ProxyRendererSubsystem>() != nullptr);

            manifest.Shutdown();
        }

        TEST_CASE("Input and Time activate even with empty capabilities")
        {
            EngineSubsystemTagFixture tags;
            EngineSubsystemDescriptors descs;
            SubsystemLifecycleLog log;
            SubsystemLifecycleLogGuard guard(log);

            SubsystemRegistry<AppSubsystem> registry;
            registry.Register<ProxyWindowSubsystem>(std::move(descs.WindowDesc));
            registry.Register<ProxyInputSubsystem>(std::move(descs.InputDesc));
            registry.Register<ProxyTimeSubsystem>(std::move(descs.TimeDesc));
            registry.Register<ProxyRenderDeviceSubsystem>(std::move(descs.RenderDeviceDesc));
            registry.Register<ProxyRendererSubsystem>(std::move(descs.RendererDesc));

            auto manifestResult = registry.Finalise();
            REQUIRE(manifestResult.has_value());
            auto manifest = std::move(*manifestResult);

            // Empty capabilities: only always-active subsystems should initialise
            EngineContext ctx;
            REQUIRE(manifest.Initialise(ctx, CapabilitySet{}));

            // Only Input and Time should have been initialised
            bool hasInputInit = false;
            bool hasTimeInit = false;
            bool hasWindowInit = false;
            bool hasRenderDeviceInit = false;
            bool hasRendererInit = false;

            for (const auto& event : log.Events)
            {
                if (event == "Input.Init")
                {
                    hasInputInit = true;
                }
                if (event == "Time.Init")
                {
                    hasTimeInit = true;
                }
                if (event == "Window.Init")
                {
                    hasWindowInit = true;
                }
                if (event == "RenderDevice.Init")
                {
                    hasRenderDeviceInit = true;
                }
                if (event == "Renderer.Init")
                {
                    hasRendererInit = true;
                }
            }

            CHECK(hasInputInit);
            CHECK(hasTimeInit);
            CHECK_FALSE(hasWindowInit);
            CHECK_FALSE(hasRenderDeviceInit);
            CHECK_FALSE(hasRendererInit);

            manifest.Shutdown();
        }
    }

} // namespace Wayfinder::Tests
