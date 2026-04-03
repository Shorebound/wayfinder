# Console System

**Status:** Planned
**Parent document:** [application_architecture_v2.md](application_architecture_v2.md)
**Last updated:** 2026-04-03

Debug console providing commands, console variables (cvars), and log output display. Commands and cvars are registered by plugins via the `ConsoleRegistrar`. The `ConsoleService` (AppSubsystem) manages execution at runtime. The `ConsoleOverlay` (IOverlay) provides the in-game UI. The editor hosts a docked panel wrapping the same `ConsoleService`.

**Related documents:**
- [config_service.md](config_service.md) - ConfigService design, config loading, hot-reload, persistence
- [application_architecture_v2.md](application_architecture_v2.md) - Plugin model, capabilities, subsystems, overlays

---

## Scope

This document covers:
- Console commands: registration, namespacing, arguments, execution, capability gating
- Console variables: open type system, ConfigService-backed storage, change notification
- Console overlay: Quake-style dropdown, log mirroring, filtering, history
- Editor integration: docked console panel wrapping the same service
- Built-in commands provided by the console system itself

**Not covered here:** ConfigService design (see [config_service.md](config_service.md)), UI toolkit internals, exec file scripting (@todo).

---

## Goals

- **Plugin-registered.** Commands and cvars are declared during `Build()` via `ConsoleRegistrar`. No static initialisers, no global singletons.
- **Capability-gated.** Commands and cvars declare `RequiredCapabilities`. Unavailable commands are visible but grayed in autocomplete with a hint showing what capability is missing.
- **Debug + optional runtime.** Full console in Debug/Development builds. Shipping builds include only explicitly whitelisted commands and cvars.
- **Log mirror.** The console displays `Wayfinder::Log` output alongside command results. Filtering by category and verbosity via dropdown menus in the overlay UI.
- **ConfigService-backed cvars.** Cvars that correspond to plugin config fields bind directly to stable storage in `ConfigService`. See [config_service.md](config_service.md) for the ownership model and lifetime guarantees.
- **Structured output.** Commands return a human-readable message with optional `nlohmann::json` structured data for programmatic consumption by the editor and future remote console.
- **Remote-ready interface.** The `ConsoleService` API is designed so a network transport (TCP listener for remote debugging) can be added later without changing the command or cvar model.

---

## Architecture Overview

```
AppBuilder (build phase)
  |
  +-- ConsoleRegistrar         collects command/cvar descriptors from plugins
  |
  +-- ConfigRegistrar          collects config type declarations (see config_service.md)
  |
  v
AppDescriptor (read-only snapshot)
  |
  v
Application (runtime)
  |
  +-- ConsoleService           AppSubsystem: command execution, cvar store, history
  |     +-- CommandRegistry    resolved commands with capability metadata
  |     +-- CvarStore          typed values, bound to ConfigService fields where applicable
  |     +-- OutputBuffer       ring buffer of log + command output lines
  |     +-- CommandHistory     persistent command history
  |
  +-- ConfigService            AppSubsystem: stable config storage (see config_service.md)
  |
  +-- ConsoleOverlay           IOverlay: Quake-style dropdown UI
  |
  +-- (Editor) ConsolePanelWidget   docked panel in EditorPanelManager wrapping ConsoleService
```

---

## Commands

### Registration

Commands are registered by plugins during `Build()`:

```cpp
void GuildDebugPlugin::Build(AppBuilder& builder)
{
    auto& console = builder.Registrar<ConsoleRegistrar>();

    console.RegisterCommand({
        .Name = "guild.info",
        .Description = "Displays information about a guild",
        .Usage = "guild.info <GuildID>",
        .Handler = &GuildDebugCommands::Info,
        .RequiredCapabilities = { Capability::Simulation },
    });
}
```

### Bulk Registration

For plugins with many related commands, a constexpr-friendly table avoids per-command ceremony:

```cpp
namespace GuildDebugCommands
{
    auto Info(EngineContext& ctx, CommandArgs args) -> CommandResult;
    auto Rename(EngineContext& ctx, CommandArgs args) -> CommandResult;
    auto Reset(EngineContext& ctx, CommandArgs args) -> CommandResult;

    inline constexpr auto COMMANDS = std::array{
        CommandEntry{ "guild.info",   "Displays information about a guild", &Info },
        CommandEntry{ "guild.rename", "Renames a guild",                    &Rename },
        CommandEntry{ "guild.reset",  "Resets a guild to default state",    &Reset },
    };
}

void GuildDebugPlugin::Build(AppBuilder& builder)
{
    auto& console = builder.Registrar<ConsoleRegistrar>();
    console.RegisterGroup(GuildDebugCommands::COMMANDS, {
        .RequiredCapabilities = { Capability::Simulation },
    });
}
```

### Command Handler Signature

Every command handler receives `EngineContext&` and parsed arguments. No separate "with world" variant -- `EngineContext` provides access to all subsystems:

```cpp
using CommandHandler = std::function<CommandResult(EngineContext&, CommandArgs)>;
```

Commands access state-scoped subsystems via `TryGetStateSubsystem`:

```cpp
auto GuildDebugCommands::Info(EngineContext& ctx, CommandArgs args) -> CommandResult
{
    auto name = args.Get<std::string_view>(0);
    if (not name)
        return CommandResult::Error("Usage: guild.info <GuildID>");

    auto* sim = ctx.TryGetStateSubsystem<SimulationSubsystem>();
    if (not sim)
        return CommandResult::Error("No simulation active");

    auto& world = sim->GetSimulation().GetWorld();
    // ... query world, build result ...

    return CommandResult::Ok("Guild info retrieved")
        .WithData({
            {"name", guild->Name},
            {"funds", guild->Funds},
            {"experience", guild->Experience},
        });
}
```

### Namespacing

Commands use dot-separated namespaces matching `GameplayTag` format:

```
guild.info
guild.finance.add
guild.recruitment.generate
rendering.ssao.enable
physics.gravity_scale
scene.load
debug.show_wireframe
```

### Arguments

Commands receive positional arguments and optional flags:

```cpp
struct CommandArgs
{
    /// Positional argument by index. Returns nullopt if index out of range.
    template<typename T = std::string_view>
    auto Get(std::size_t index) const -> std::optional<T>;

    /// Flag presence check: --verbose, --force
    auto HasFlag(std::string_view flag) const -> bool;

    /// Flag value: --count=5, --name=test
    template<typename T = std::string_view>
    auto GetFlag(std::string_view flag) const -> std::optional<T>;

    /// Raw argument count (positional only, excludes flags).
    auto Count() const -> std::size_t;
};
```

**Parsing rules:**
- Tokens starting with `--` are flags
- `--flag` is a boolean flag (present = true)
- `--flag=value` is a valued flag
- Everything else is a positional argument in order
- Quoted strings (`"hello world"`) are a single token

### Command Output

```cpp
struct CommandResult
{
    enum class Status { Ok, Error };

    Status ResultStatus;
    std::string Message;                /// Always present, human-readable.
    std::optional<nlohmann::json> Data; /// Optional structured data.

    static auto Ok(std::string msg = {}) -> CommandResult;
    static auto Error(std::string msg) -> CommandResult;
    auto WithData(nlohmann::json data) -> CommandResult&&;
};
```

Simple commands return text only (zero heap allocation beyond SSO):
```cpp
return CommandResult::Ok("Gravity scale set to 2.0");
```

Data-rich commands attach JSON:
```cpp
return CommandResult::Ok("3 entities found")
    .WithData({{"entities", entityList}});
```

Console overlay displays `Message`. If `Data` is present, it formats the JSON as a table or tree below the message. Editor panels read `Data` programmatically.

### Capability Gating

Commands declare `RequiredCapabilities`. When capabilities are unmet:

- **Autocomplete:** command is visible but grayed out, with a hint (e.g. `physics.gravity_scale (requires: Simulation)`)
- **Execution:** returns an error: `"Command 'physics.gravity_scale' requires capability: Simulation"`

Commands with empty `RequiredCapabilities` are always available.

### Shipping Build Stripping

Commands and cvars are **debug-only by default**. To include a command in Shipping builds, it must explicitly opt in:

```cpp
console.RegisterCommand({
    .Name = "graphics.vsync",
    .Description = "Toggle vertical sync",
    .Handler = &GraphicsCommands::VSync,
    .ShippingVisible = true,   // included in Shipping builds
});
```

At build time, commands and cvars without `ShippingVisible = true` are compiled out via `#if !defined(WAYFINDER_SHIPPING)` guards in the registrar's generated descriptor tables.

---

## Console Variables (Cvars)

### Registration

Cvars are registered by plugins during `Build()`:

```cpp
void PhysicsPlugin::Build(AppBuilder& builder)
{
    auto& console = builder.Registrar<ConsoleRegistrar>();

    console.RegisterVariable<float>({
        .Name = "physics.gravity_scale",
        .Description = "Multiplier for world gravity",
        .Default = 1.0f,
        .Min = 0.0f,
        .Max = 100.0f,
        .RequiredCapabilities = { Capability::Simulation },
    });
}
```

### Open Type System

Cvars support an extensible set of value types. Each type must provide parse and format functions:

```cpp
template<typename T>
struct CvarTypeTraits
{
    static auto Parse(std::string_view text) -> std::optional<T>;
    static auto Format(const T& value) -> std::string;
};
```

**Built-in types:** `bool`, `int32_t`, `float`, `std::string`.

Plugins register additional types for their domains:

```cpp
// Maths plugin registers vec3 support
console.RegisterCvarType<glm::vec3>({
    .Parse = [](std::string_view text) -> std::optional<glm::vec3> {
        // parse "1.0 2.0 3.0" or "1.0,2.0,3.0"
    },
    .Format = [](const glm::vec3& v) -> std::string {
        return std::format("{} {} {}", v.x, v.y, v.z);
    },
});
```

Internally, cvar values are stored as type-erased objects with `std::type_index` keys. Parsing and formatting dispatch through the registered traits.

### Validation Constraints

Numeric cvars support optional range constraints:

```cpp
console.RegisterVariable<float>({
    .Name = "rendering.ssao.radius",
    .Default = 0.5f,
    .Min = 0.01f,
    .Max = 5.0f,   // clamped or rejected if out of range
});

console.RegisterVariable<int32_t>({
    .Name = "rendering.shadow_quality",
    .Default = 2,
    .Min = 0,
    .Max = 4,
});
```

Setting a value outside the declared range produces an error message with the valid range.

### ConfigService Backing

Cvars that correspond to plugin config fields are backed by stable storage in `ConfigService`. The cvar's value and the config struct field are the same memory. See [config_service.md](config_service.md) for how `ConfigService` provides address-stable config storage that survives state transitions.

The binding uses pointer-to-member, resolved to a real address after `ConfigService` is constructed:

```cpp
// During Build(): declare that this cvar maps to a config field
console.BindVariable("physics.gravity_scale", &PhysicsConfig::GravityScale);
```

At runtime, `ConsoleService` resolves the binding through `ConfigService`:
- **Read:** returns the value from the config field
- **Write:** updates the config field, triggers change notification

Cvars without config backing (debug-only toggles, diagnostic counters) are stored in `ConsoleService`'s own flat store. These are standalone values with no config file counterpart.

### Change Notification

Two mechanisms, matching Unreal's dual model:

**1. ConfigService batch reload (primary):**
When a cvar backed by `ConfigService` is changed, `ConfigService` marks the config section dirty. At the frame boundary, `OnConfigReloaded()` fires on subsystems whose config section changed. Subsystems re-read their config. This is the same path used by file-watcher hot-reload.

**2. Optional per-cvar callback (immediate):**
For cvars needing same-frame response (toggle wireframe, change debug visualisation):

```cpp
console.RegisterVariable<bool>({
    .Name = "debug.show_wireframe",
    .Default = false,
    .OnChanged = [](bool oldVal, bool newVal) {
        // Immediate response, same frame
    },
});
```

Most cvars rely on the batch mechanism. Per-cvar callbacks are the exception for latency-sensitive debug toggles.

### Cvar Commands

Cvars are accessible as console commands by name:

```
> physics.gravity_scale          # prints current value
physics.gravity_scale = 1.0

> physics.gravity_scale 2.5      # sets value
physics.gravity_scale: 1.0 -> 2.5

> physics.gravity_scale --reset  # resets to default
physics.gravity_scale: 2.5 -> 1.0 (default)
```

---

## ConsoleService (AppSubsystem)

The runtime service that owns command execution, cvar state, output buffering, and history.

```cpp
class ConsoleService : public AppSubsystem
{
public:
    auto Initialise(SubsystemRegistry& registry) -> Result<void> override;
    void Shutdown() override;

    // -- Command execution --
    auto Execute(EngineContext& ctx, std::string_view input) -> CommandResult;

    // -- Cvar access --
    template<typename T> auto GetVariable(std::string_view name) const -> std::optional<T>;
    template<typename T> auto SetVariable(std::string_view name, T value) -> CommandResult;
    void ResetVariable(std::string_view name);

    // -- Output buffer --
    auto GetOutputBuffer() const -> std::span<const OutputLine>;
    void AppendOutput(OutputLine line);
    void ClearOutput();

    // -- History --
    auto GetHistory() const -> std::span<const std::string>;

    // -- Autocomplete --
    auto Complete(std::string_view partial,
                  const GameplayTagContainer& activeCapabilities) const
        -> std::vector<CompletionEntry>;

    // -- Query --
    auto GetCommandDescriptors() const -> std::span<const CommandDescriptor>;
    auto GetCvarDescriptors() const -> std::span<const CvarDescriptor>;

private:
    CommandRegistry m_commands;
    CvarStore m_cvars;
    OutputBuffer m_output;       // ring buffer, configurable max lines
    CommandHistory m_history;    // persistent, configurable max entries
    ConfigService* m_configService = nullptr;
};
```

### Output Buffer

The output buffer is a ring buffer of `OutputLine` entries. Each line carries a source tag:

```cpp
struct OutputLine
{
    enum class Source { Command, CommandResult, Log };

    Source LineSource;
    std::string Text;
    LogVerbosity Verbosity = LogVerbosity::Info;     // for Log-sourced lines
    std::string_view Category;                        // for Log-sourced lines
};
```

Maximum scrollback is configurable via `console.max_scrollback` (default: 1000).

### Log Integration

`ConsoleService` registers a log output sink (`ILogOutput` implementation) during initialisation. All `Wayfinder::Log` output is mirrored into the output buffer with category and verbosity metadata. The overlay uses this metadata for filtering.

### Command History

- Arrow up/down navigates history
- History saved to `saved/console_history.txt` on shutdown, loaded on startup
- Maximum entries configurable via `console.max_history` (cvar)
- One command per line, plain text

---

## ConsoleRegistrar (Build-Time)

Collects command and cvar descriptors from plugins during `Build()`. Validates at finalisation:

- Duplicate command names rejected (startup error)
- Duplicate cvar names rejected (startup error)
- Cvar type traits must be registered before cvar declaration (or use a built-in type)
- Config bindings validated against `ConfigRegistrar` declarations

```cpp
class ConsoleRegistrar
{
public:
    // -- Commands --
    void RegisterCommand(CommandDescriptor descriptor);

    template<std::size_t N>
    void RegisterGroup(const std::array<CommandEntry, N>& entries,
                       GroupOptions options);

    // -- Cvars --
    template<typename T>
    void RegisterVariable(CvarDescriptor<T> descriptor);

    template<typename TConfig, typename TField>
    void BindVariable(std::string_view name, TField TConfig::* member);

    // -- Type extensions --
    template<typename T>
    void RegisterCvarType(CvarTypeTraits<T> traits);
};
```

### Descriptor Types

```cpp
struct CommandDescriptor
{
    std::string_view Name;
    std::string_view Description;
    std::string_view Usage;               // optional usage string for help
    CommandHandler Handler;
    std::vector<GameplayTag> RequiredCapabilities;
    bool ShippingVisible = false;
};

struct CommandEntry
{
    std::string_view Name;
    std::string_view Description;
    CommandHandler Handler;
};

struct GroupOptions
{
    std::vector<GameplayTag> RequiredCapabilities;
    bool ShippingVisible = false;
};

template<typename T>
struct CvarDescriptor
{
    std::string_view Name;
    std::string_view Description;
    T Default;
    std::optional<T> Min;                 // numeric types only
    std::optional<T> Max;
    std::vector<GameplayTag> RequiredCapabilities;
    bool ShippingVisible = false;
    std::function<void(const T&, const T&)> OnChanged;  // optional per-cvar callback
};

struct CompletionEntry
{
    std::string_view Name;
    std::string_view Description;
    bool Available;                       // false = grayed (capabilities unmet)
    std::string_view RequiresHint;        // e.g. "requires: Simulation"
};
```

---

## ConsoleOverlay (IOverlay)

Quake-style dropdown overlay. Slides from the top of the screen when toggled. Persistent overlay with no capability requirements (always available).

### Registration

```cpp
void ConsolePlugin::Build(AppBuilder& builder)
{
    builder.RegisterOverlay<ConsoleOverlay>({
        .RequiredCapabilities = {},     // always available
        .DefaultActive = false,         // toggled at runtime
    });
}
```

### Toggle Key

Configurable via cvar, default tilde/backtick:

```cpp
console.RegisterVariable<std::string>({
    .Name = "console.toggle_key",
    .Description = "Key to toggle the console overlay",
    .Default = "`",
    .ShippingVisible = true,
});
```

### Display Layout

```
+----------------------------------------------------------+
| [Verbosity: v] [Category: v]                    [Clear]  |   filter bar
|----------------------------------------------------------|
| [Info] Engine: Initialising Wayfinder Engine             |
| [Info] Engine: EngineRuntime initialised                 |
| [Info] Game: Loaded bootstrap scene from: ...            |
| > guild.info Adventurers                                 |   command echo
| Guild info retrieved                                     |   result message
|   name: Adventurers                                      |   structured data
|   funds: 1250                                            |
| [Warn] Physics: Collision shape missing for entity 42    |   log output
|----------------------------------------------------------|
| > _                                                      |   input line
+----------------------------------------------------------+
```

- **Filter bar:** two dropdown menus -- one for verbosity level (show messages at or above selected level), one for log category (multi-select: which categories to display)
- **Output area:** scrollable. Interleaves command I/O and log output chronologically
- **Input line:** text input with cursor, standard editing (home/end, ctrl+left/right)
- **Scrollback:** mouse wheel or page up/down. Default max 1000 lines, configurable via `console.max_scrollback`

### Autocomplete

Tab triggers autocomplete on the current input:

- Matches against command names and cvar names
- Grayed entries (capabilities unmet) are shown with a hint: `physics.gravity_scale (requires: Simulation)`
- Single match: auto-completes inline
- Multiple matches: displays list below input

@todo: Dynamic argument completion. Future expansion where commands provide a `CompletionProvider` callback that returns contextual suggestions (entity names, asset paths, enum values) based on partial input and runtime state.

### Event Handling

The overlay consumes keyboard events when active:
- Toggle key: activates/deactivates the overlay
- When active: all keyboard input goes to the console (text input, history navigation, autocomplete)
- Events are marked `Handled` so they don't reach the active state

### Log Filtering State

Filter state is owned by the overlay (or editor panel), not `ConsoleService`. Different views can have different filters on the same output buffer.

---

## Editor Integration

In the editor, the console is a docked panel within `EditorPanelManager` (`IStateUI` for `EditorState`). It wraps the same `ConsoleService` -- same command registry, same cvar store, same output buffer.

```cpp
class ConsolePanelWidget
{
public:
    void OnAttach(EngineContext& ctx)
    {
        m_consoleService = &ctx.GetAppSubsystem<ConsoleService>();
    }

    void OnRender(EngineContext& ctx)
    {
        // Render the same output buffer, input line, filter controls
        // as the overlay, but within the editor's docking system
    }

    void OnEvent(EngineContext& ctx, Event& event)
    {
        // Forward keyboard input to ConsoleService when the panel is focused
    }

private:
    ConsoleService* m_consoleService = nullptr;
    // Own filter state (independent of overlay)
};
```

The editor panel and the overlay can coexist. Both read from the same `ConsoleService`. Each maintains its own filter state and scroll position.

---

## Built-in Commands

The console system registers these commands itself (via `ConsolePlugin`):

| Command | Description |
|---|---|
| `help` | List all commands. `help <name>` shows a command's description and usage. |
| `help.cvars` | List all cvars with current values and defaults. |
| `clear` | Clear the console output buffer. |
| `history` | Display command history. |
| `log.filter <category> <on\|off>` | Toggle log category visibility (convenience for the dropdown). |
| `log.verbosity <level>` | Set minimum log verbosity displayed. |
| `cvar.list` | List all cvars, optionally filtered by prefix: `cvar.list physics`. |
| `cvar.reset <name>` | Reset a cvar to its default value. |
| `cvar.reset_all` | Reset all cvars to defaults. |

### Built-in Cvars

| Cvar | Type | Default | Description |
|---|---|---|---|
| `console.toggle_key` | `string` | `` ` `` | Key binding to toggle the console overlay. |
| `console.max_scrollback` | `int32_t` | `1000` | Maximum output buffer lines. |
| `console.max_history` | `int32_t` | `500` | Maximum persisted command history entries. |

---

## Saved Directory

Console history and other generated runtime data are stored in `saved/` under the project root. This directory is engine-generated and non-essential (safe to delete):

```
<project_root>/
  saved/
    console_history.txt      command history (one per line, capped)
    logs/                    log files (future: move from current location)
    user_overrides.toml      persisted cvar changes (see config_service.md)
```

`ProjectDescriptor` gains a `ResolveSavedDir()` method returning `ProjectRoot / "saved"`. The directory is created on first write. Should be gitignored.

---

## Plugin Composition

The console system is packaged as a plugin:

```cpp
struct ConsolePlugin : IPlugin
{
    void Build(AppBuilder& builder) override
    {
        builder.RegisterSubsystem<ConsoleService>({
            // No capability requirement -- always available
        });

        builder.RegisterOverlay<ConsoleOverlay>({
            .RequiredCapabilities = {},
            .DefaultActive = false,
        });

        // Register built-in commands and cvars
        auto& console = builder.Registrar<ConsoleRegistrar>();
        RegisterBuiltinCommands(console);
        RegisterBuiltinCvars(console);
    }

    auto GetName() const -> std::string_view override { return "Console"; }
};
```

Games include it in their plugin set:

```cpp
app.AddPlugin<ConsolePlugin>();
```

Headless tools and tests can omit it entirely.

---

## Design Rationale

| Decision | Rationale |
|---|---|
| ConsoleRegistrar is a typed registrar | Follows the AppBuilder extensibility model. Third-party plugins add commands without modifying engine code. Same pattern as SystemRegistrar, StateRegistrar. |
| Dot-separated command namespaces | Matches GameplayTag format. Consistent naming throughout the engine. Familiar to Unreal users. |
| Positional args + flags | Covers simple commands (`scene.load forest`) and complex ones (`guild.recruitment.generate Adventurers --count=10 --clear`). No need for named-only or positional-only. |
| Optional nlohmann::json on CommandResult | Leverages existing dependency. Avoids inventing bespoke serialisation. Simple commands pay zero cost (Data is nullopt). Editor and remote console consume structured data directly. |
| ConfigService-backed cvars (not standalone) | Single source of truth. No manual sync between cvars and config structs. Hot-reload flows through the same path for file changes and console commands. Avoids CryEngine-style dangling pointer issues via stable ConfigService storage. See [config_service.md](config_service.md). |
| Grayed commands when capabilities unmet | Discoverability over cleanliness. Debug users benefit from seeing what exists even when unavailable. Printed capability hint eliminates "did I spell it wrong?" confusion. |
| Whitelist for Shipping | Secure default. Debug commands never accidentally ship. Explicit `ShippingVisible = true` forces conscious decision. |
| Quake-style dropdown | Proven UX for game consoles. Non-intrusive when hidden. Familiar convention. |
| Log mirror with filter dropdowns | Unifies command output and log output in one view. Category/verbosity dropdowns provide visual filtering without typing filter commands. |
| History persisted to saved/ | Cross-session workflow continuity. Plain text format, trivially loadable. Per-project because debug workflows differ between projects. |
| Open cvar type system | Plugins can register domain-specific types (vec3, colour, enums) without engine changes. Built-in types cover 95% of cases. |

---

## Future Work

| Item | Notes |
|---|---|
| @todo Exec files | `exec <filename>` runs a plain-text script of console commands from `config/exec/`. One command per line, `#` comments. Useful for debug setup macros, benchmark presets, team-shared configurations. File extension: `.exec`. |
| @todo Dynamic autocomplete | Commands provide a `CompletionProvider` callback for contextual argument completion (entity names, asset paths, enum values). |
| @todo Remote console | TCP listener in Debug/Development builds. Connects from external tools (custom debugger, CI scripts). Serialises `CommandResult::Data` as JSON over the wire. ConsoleService API is already transport-agnostic. |
| @todo Console aliases | `alias ginfo guild.info` for shorthand. Stored in saved directory alongside history. |
