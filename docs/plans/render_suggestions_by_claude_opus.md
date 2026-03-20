# Rendering System Architecture

## Design Principles

1. **Nothing above the RHI knows what raylib is.** All backend specifics live behind a single interface.
2. **Handle-based resources** — no raw GPU pointers leak upward.
3. **Command buffer pattern** — the high-level renderer records draw commands; the backend replays them.
4. **Data-driven materials and pipeline** — defined in TOML, not hardcoded.
5. **Sort key packing** — draw order is a single `uint64_t` sort, not pointer-chasing.

---

## RHI Types — `rhi/Types.hpp`

Everything the rest of the engine touches. No backend headers.

```cpp
#pragma once

#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <variant>

namespace retro::rhi {

// ── Generational Handle ──────────────────────────────────

template <typename Tag>
struct Handle {
    uint32_t index      : 20 = 0;
    uint32_t generation : 12 = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return generation != 0;
    }

    constexpr auto operator<=>(const Handle&) const = default;

    static constexpr Handle invalid() noexcept { return {}; }
};

struct TextureTag {};
struct ShaderTag {};
struct MeshTag {};
struct RenderTargetTag {};
struct UniformBufferTag {};

using TextureHandle       = Handle<TextureTag>;
using ShaderHandle        = Handle<ShaderTag>;
using MeshHandle          = Handle<MeshTag>;
using RenderTargetHandle  = Handle<RenderTargetTag>;
using UniformBufferHandle = Handle<UniformBufferTag>;

// ── Enums ────────────────────────────────────────────────

enum class VertexAttrib : uint8_t {
    Position,  // vec3
    Normal,    // vec3
    Tangent,   // vec4
    Color0,    // vec4 (vertex color)
    TexCoord0, // vec2
    TexCoord1, // vec2
    Joints,    // uvec4 (skinning)
    Weights,   // vec4  (skinning)
    Count
};

enum class TextureFormat : uint8_t {
    RGBA8,
    RGB8,
    R8,
    RG8,
    RGBA16F,
    Depth24,
    Depth24Stencil8,
};

enum class TextureWrap : uint8_t {
    Repeat,
    Clamp,
    Mirror,
};

enum class TextureFilter : uint8_t {
    Nearest,
    Bilinear,
    Trilinear,
};

enum class BlendMode : uint8_t {
    Opaque,
    AlphaBlend,
    Additive,
    Multiply,
    Dither, // screen-door transparency
};

enum class CullFace : uint8_t {
    Back,
    Front,
    None,
};

enum class DepthFunc : uint8_t {
    Less,
    LessEqual,
    Equal,
    Always,
    None, // depth test off
};

enum class PrimitiveType : uint8_t {
    Triangles,
    Lines,
    Points,
};

// ── Descriptors (creation params) ────────────────────────

struct VertexLayout {
    struct Attrib {
        VertexAttrib type;
        uint8_t      components; // 2, 3, or 4
        bool         normalized = false;
    };

    std::vector<Attrib> attribs;
    uint32_t            stride = 0; // 0 = auto-calculate
};

struct TextureDesc {
    uint32_t      width  = 1;
    uint32_t      height = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureWrap   wrap   = TextureWrap::Repeat;
    TextureFilter filter = TextureFilter::Bilinear;
    bool          mips   = true;
    std::string   debug_name;
};

struct MeshDesc {
    VertexLayout                layout;
    std::span<const std::byte> vertex_data;
    std::span<const uint16_t>  index_data; // uint16 keeps it sixth-gen
    PrimitiveType              primitive = PrimitiveType::Triangles;
    std::string                debug_name;
};

struct RenderTargetDesc {
    uint32_t      width;
    uint32_t      height;
    TextureFormat color_format = TextureFormat::RGBA8;
    TextureFormat depth_format = TextureFormat::Depth24;
    uint8_t       color_attachment_count = 1;
    std::string   debug_name;
};

struct ShaderDesc {
    std::string vertex_source;
    std::string fragment_source;
    std::string debug_name;
};

// ── Pipeline State (per draw call) ───────────────────────

struct PipelineState {
    BlendMode blend    = BlendMode::Opaque;
    CullFace  cull     = CullFace::Back;
    DepthFunc depth    = DepthFunc::Less;
    bool      depth_write = true;
};

// ── Uniform types ────────────────────────────────────────

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
struct Mat4 { float m[16]; };

using UniformValue = std::variant<
    float, int32_t,
    Vec2, Vec3, Vec4,
    Mat4,
    TextureHandle
>;

// ── Error type ───────────────────────────────────────────

struct RHIError {
    std::string message;
};

template <typename T>
using Result = std::expected<T, RHIError>;

} // namespace retro::rhi
```

---

## Resource Pool — `rhi/ResourcePool.hpp`

Generational arena used by every backend to manage handle ↔ resource mapping.

```cpp
#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

namespace retro::rhi {

template <typename Tag, typename Resource>
class ResourcePool {
public:
    using HandleType = Handle<Tag>;

    HandleType acquire(Resource&& resource) {
        uint32_t index;
        if (!free_list_.empty()) {
            index = free_list_.back();
            free_list_.pop_back();
        } else {
            index = static_cast<uint32_t>(entries_.size());
            entries_.emplace_back();
        }

        auto& entry = entries_[index];
        entry.generation++;
        // Wrap around, skip 0 (invalid sentinel)
        if (entry.generation == 0) {
            entry.generation = 1;
        }
        entry.resource = std::move(resource);
        entry.alive = true;

        return HandleType{
            .index = index, .generation = entry.generation
        };
    }

    void release(HandleType handle) {
        if (!is_valid(handle)) return;
        auto& entry = entries_[handle.index];
        entry.alive = false;
        entry.resource = {};
        free_list_.push_back(handle.index);
    }

    [[nodiscard]] bool is_valid(HandleType handle) const {
        if (handle.index >= entries_.size()) return false;
        auto& entry = entries_[handle.index];
        return entry.alive
            && entry.generation == handle.generation;
    }

    [[nodiscard]] Resource* get(HandleType handle) {
        if (!is_valid(handle)) return nullptr;
        return &entries_[handle.index].resource;
    }

    [[nodiscard]] const Resource* get(HandleType handle) const {
        if (!is_valid(handle)) return nullptr;
        return &entries_[handle.index].resource;
    }

private:
    struct Entry {
        Resource resource{};
        uint32_t generation = 0;
        bool     alive = false;
    };

    std::vector<Entry>    entries_;
    std::vector<uint32_t> free_list_;
};

} // namespace retro::rhi
```

---

## Sort Key — `rhi/SortKey.hpp`

One `uint64_t` encodes everything needed to order draw calls optimally.

```cpp
#pragma once

#include <cstdint>

namespace retro::rhi {

// ┌──────────┬───────────┬──────────┬────────────┬──────────┐
// │ Layer(3) │ Trans(1)  │Shader(12)│Material(16)│ Depth(32)│
// └──────────┴───────────┴──────────┴────────────┴──────────┘
//
// Opaque:      sort front-to-back  (depth ascending)
// Transparent: sort back-to-front  (depth descending = bitflip)

struct SortKey {
    uint8_t  layer;       // 0-7: background, world, fx, overlay...
    bool     translucent;
    uint16_t shader_id;
    uint16_t material_id;
    float    depth;       // camera-space Z

    [[nodiscard]] uint64_t encode() const noexcept {
        uint64_t key = 0;

        key |= (static_cast<uint64_t>(layer & 0x7)) << 61;
        key |= (static_cast<uint64_t>(translucent ? 1 : 0)) << 60;
        key |= (static_cast<uint64_t>(shader_id & 0xFFF)) << 48;
        key |= (static_cast<uint64_t>(material_id)) << 32;

        // Reinterpret float depth as sortable uint32.
        // IEEE 754 floats sort correctly as uint when positive
        // (our depth values are always positive in view space).
        uint32_t depth_bits;
        static_assert(sizeof(float) == sizeof(uint32_t));
        std::memcpy(&depth_bits, &depth, sizeof(float));

        // Transparent: flip so we sort back-to-front
        if (translucent) {
            depth_bits = ~depth_bits;
        }

        key |= static_cast<uint64_t>(depth_bits);
        return key;
    }
};

} // namespace retro::rhi
```

---

## RHI Interface — `rhi/RHI.hpp`

The contract every backend implements. The rest of the engine only sees this.

```cpp
#pragma once

#include "Types.hpp"
#include <functional>
#include <memory>
#include <string_view>

namespace retro::rhi {

struct DrawCall {
    MeshHandle    mesh;
    ShaderHandle  shader;
    PipelineState pipeline;

    // Uniforms for this draw
    struct Binding {
        std::string  name;
        UniformValue value;
    };
    std::vector<Binding> uniforms;

    uint32_t index_offset = 0;
    uint32_t index_count  = 0; // 0 = draw all
    uint32_t instance_count = 1;
};

struct ClearFlags {
    bool   color = true;
    bool   depth = true;
    Vec4   clear_color = {0.1f, 0.1f, 0.12f, 1.0f};
    float  clear_depth = 1.0f;
};

class IRHI {
public:
    virtual ~IRHI() = default;

    // ── Lifecycle ────────────────────────────────────
    virtual Result<void> initialize(
        uint32_t width, uint32_t height,
        std::string_view window_title
    ) = 0;
    virtual void shutdown() = 0;
    virtual bool should_close() const = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;    // present / swap

    // ── Resource creation ────────────────────────────
    virtual Result<TextureHandle>      create_texture(
        const TextureDesc& desc,
        std::span<const std::byte> pixels = {}
    ) = 0;
    virtual Result<ShaderHandle>       create_shader(
        const ShaderDesc& desc
    ) = 0;
    virtual Result<MeshHandle>         create_mesh(
        const MeshDesc& desc
    ) = 0;
    virtual Result<RenderTargetHandle> create_render_target(
        const RenderTargetDesc& desc
    ) = 0;

    // ── Resource destruction ─────────────────────────
    virtual void destroy(TextureHandle h) = 0;
    virtual void destroy(ShaderHandle h) = 0;
    virtual void destroy(MeshHandle h) = 0;
    virtual void destroy(RenderTargetHandle h) = 0;

    // ── Resource updates ─────────────────────────────
    virtual void update_texture(
        TextureHandle h,
        std::span<const std::byte> pixels
    ) = 0;

    // ── Render target binding ────────────────────────
    virtual void bind_render_target(
        RenderTargetHandle h // invalid = backbuffer
    ) = 0;
    virtual void clear(const ClearFlags& flags) = 0;
    virtual void set_viewport(
        uint32_t x, uint32_t y,
        uint32_t w, uint32_t h
    ) = 0;

    // ── Draw submission ──────────────────────────────
    virtual void submit(const DrawCall& draw) = 0;

    // ── Queries ──────────────────────────────────────
    virtual TextureHandle get_render_target_color(
        RenderTargetHandle h, uint8_t index = 0
    ) const = 0;
    virtual TextureHandle get_render_target_depth(
        RenderTargetHandle h
    ) const = 0;
    virtual std::pair<uint32_t, uint32_t>
        get_backbuffer_size() const = 0;

    // ── Debug ────────────────────────────────────────
    virtual void push_debug_group(std::string_view label) = 0;
    virtual void pop_debug_group() = 0;
};

enum class Backend {
    Raylib,
    // SDL_GPU,
    // Bgfx,
    // Vulkan,
};

std::unique_ptr<IRHI> create_rhi(Backend backend);

} // namespace retro::rhi
```

## Command Bucket — `rendering/CommandBucket.hpp`

The high-level renderer doesn't call `submit` directly. It fills buckets. Buckets sort. Then flush.

```cpp
#pragma once

#include "../rhi/RHI.hpp"
#include "../rhi/SortKey.hpp"

#include <algorithm>
#include <vector>

namespace retro::rendering {

class CommandBucket {
public:
    struct Entry {
        rhi::SortKey  key;
        rhi::DrawCall draw;
        uint64_t      encoded_key = 0;
    };

    void clear() { entries_.clear(); }

    void push(rhi::SortKey key, rhi::DrawCall draw) {
        entries_.push_back(Entry{
            .key = key,
            .draw = std::move(draw),
            .encoded_key = key.encode(),
        });
    }

    void sort() {
        std::sort(
            entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) {
                return a.encoded_key < b.encoded_key;
            }
        );
    }

    void flush(rhi::IRHI& rhi) {
        for (auto& entry : entries_) {
            rhi.submit(entry.draw);
        }
    }

    void sort_and_flush(rhi::IRHI& rhi) {
        sort();
        flush(rhi);
    }

    [[nodiscard]] size_t size() const {
        return entries_.size();
    }

private:
    std::vector<Entry> entries_;
};

} // namespace retro::rendering
```

---

## Render Graph — `rendering/RenderGraph.hpp`

Declares passes and their dependencies. The renderer walks this graph each frame.

```cpp
#pragma once

#include "../rhi/RHI.hpp"

#include <functional>
#include <string>
#include <vector>

namespace retro::rendering {

class RenderGraph {
public:
    using PassExecuteFn = std::function<void(rhi::IRHI&)>;

    struct PassDesc {
        std::string              name;
        std::vector<std::string> reads;   // RT names read
        std::vector<std::string> writes;  // RT names written
        PassExecuteFn            execute;
    };

    void add_pass(PassDesc desc) {
        passes_.push_back(std::move(desc));
    }

    void clear() { passes_.clear(); }

    // Simple topological execute — for now, just runs in
    // insertion order. A real implementation would build a
    // DAG and reorder/cull unused passes.
    void execute(rhi::IRHI& rhi) {
        for (auto& pass : passes_) {
            rhi.push_debug_group(pass.name);
            pass.execute(rhi);
            rhi.pop_debug_group();
        }
    }

    [[nodiscard]] const std::vector<PassDesc>& passes() const {
        return passes_;
    }

private:
    std::vector<PassDesc> passes_;
};

} // namespace retro::rendering
```

---

## Material System — `rendering/Material.hpp`

Data-driven. Defined in TOML. Maps cleanly to shader + uniforms + pipeline state.

```cpp
#pragma once

#include "../rhi/Types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace retro::rendering {

struct Material {
    std::string name;
    uint16_t    id = 0; // for sort key

    // Which shader to use
    std::string shader_name;
    rhi::ShaderHandle shader = rhi::ShaderHandle::invalid();

    // Pipeline state
    rhi::PipelineState pipeline;

    // Uniform values (set per-material, can be overridden
    // per-instance)
    std::unordered_map<std::string, rhi::UniformValue> params;

    // Texture slots by name
    std::unordered_map<std::string, rhi::TextureHandle> textures;

    // Rendering layer for sort key
    uint8_t layer = 1; // 0=sky, 1=world, 2=fx, 3=overlay

    [[nodiscard]] bool is_translucent() const {
        return pipeline.blend != rhi::BlendMode::Opaque;
    }
};

} // namespace retro::rendering
```

## Material TOML Example — `assets/materials/toon.toml`

```toml
[material]
name = "toon_character"
shader = "toon"
layer = 1

[pipeline]
blend = "opaque"
cull = "back"
depth = "less"
depth_write = true

[params]
ambient_color = [0.15, 0.12, 0.18]
sun_color = [1.0, 0.95, 0.8]
shadow_steps = 3          # cel-shading bands
shadow_softness = 0.02
rim_power = 3.0
rim_color = [0.4, 0.6, 1.0]

[textures]
diffuse = "characters/ratchet_diffuse.png"
ramp = "shared/toon_ramp_3step.png"
```

## Material Library — `rendering/MaterialLibrary.hpp`

```cpp
#pragma once

#include "Material.hpp"
#include "../rhi/RHI.hpp"

#include <toml++/toml.hpp>
#include <filesystem>
#include <unordered_map>

namespace retro::rendering {

class ShaderLibrary; // forward decl

class MaterialLibrary {
public:
    MaterialLibrary(rhi::IRHI& rhi, ShaderLibrary& shaders)
        : rhi_(rhi), shaders_(shaders) {}

    Material* load(const std::filesystem::path& path);
    Material* get(std::string_view name);
    void      reload_all();

private:
    Material parse_toml(const toml::table& tbl);

    rhi::IRHI&       rhi_;
    ShaderLibrary&   shaders_;
    uint16_t         next_id_ = 1;
    std::unordered_map<std::string, Material> materials_;
};

} // namespace retro::rendering
```

---

## View — `rendering/View.hpp`

Camera + viewport bundle passed into rendering.

```cpp
#pragma once

#include "../rhi/Types.hpp"

namespace retro::rendering {

struct View {
    rhi::Mat4 view_matrix;
    rhi::Mat4 projection_matrix;

    uint32_t viewport_x = 0;
    uint32_t viewport_y = 0;
    uint32_t viewport_w = 0;
    uint32_t viewport_h = 0;

    float near_plane = 0.1f;
    float far_plane  = 1000.0f;

    // For sort key depth calculation
    [[nodiscard]] rhi::Vec3 get_position() const {
        // Extract from inverse view matrix
        // (simplified — real impl inverts or caches)
        return {
            -view_matrix.m[12],
            -view_matrix.m[13],
            -view_matrix.m[14]
        };
    }
};

} // namespace retro::rendering
```

---

## High-Level Renderer — `rendering/Renderer.hpp`

Orchestrates everything. Builds the render graph each frame, collects draw calls, sorts, flushes.

```cpp
#pragma once

#include "CommandBucket.hpp"
#include "MaterialLibrary.hpp"
#include "RenderGraph.hpp"
#include "View.hpp"
#include "../rhi/RHI.hpp"

#include <memory>

namespace retro::rendering {

class Renderer {
public:
    explicit Renderer(std::unique_ptr<rhi::IRHI> rhi);
    ~Renderer();

    // ── Frame lifecycle ──────────────────────────────
    void begin_frame();
    void end_frame();

    // ── Draw call submission (from scene collector) ──
    void submit_renderable(
        rhi::MeshHandle   mesh,
        Material&         material,
        const rhi::Mat4&  transform,
        float             camera_depth
    );

    // ── Configuration ────────────────────────────────
    void set_view(const View& view);
    void set_clear_color(rhi::Vec4 color);

    // ── Access ───────────────────────────────────────
    rhi::IRHI&        rhi()       { return *rhi_; }
    MaterialLibrary&  materials() { return *materials_; }
    bool              should_close() const;

private:
    void build_render_graph();

    std::unique_ptr<rhi::IRHI>       rhi_;
    std::unique_ptr<ShaderLibrary>   shaders_;
    std::unique_ptr<MaterialLibrary> materials_;

    RenderGraph    graph_;
    CommandBucket  opaque_bucket_;
    CommandBucket  transparent_bucket_;
    View           current_view_;
    rhi::ClearFlags clear_flags_;

    // Render targets for the pipeline
    rhi::RenderTargetHandle main_rt_;
    rhi::RenderTargetHandle ssao_rt_;
    rhi::RenderTargetHandle bloom_rt_;
};

} // namespace retro::rendering
```

## Renderer Implementation — `rendering/Renderer.cpp`

```cpp
#include "Renderer.hpp"
#include "ShaderLibrary.hpp"

namespace retro::rendering {

Renderer::Renderer(std::unique_ptr<rhi::IRHI> rhi)
    : rhi_(std::move(rhi))
{
    shaders_ = std::make_unique<ShaderLibrary>(*rhi_);
    materials_ = std::make_unique<MaterialLibrary>(
        *rhi_, *shaders_
    );
}

Renderer::~Renderer() = default;

bool Renderer::should_close() const {
    return rhi_->should_close();
}

void Renderer::set_view(const View& view) {
    current_view_ = view;
}

void Renderer::set_clear_color(rhi::Vec4 color) {
    clear_flags_.clear_color = color;
}

void Renderer::submit_renderable(
    rhi::MeshHandle   mesh,
    Material&         material,
    const rhi::Mat4&  transform,
    float             camera_depth
) {
    rhi::DrawCall draw;
    draw.mesh = mesh;
    draw.shader = material.shader;
    draw.pipeline = material.pipeline;

    // Bind material uniforms
    for (auto& [name, val] : material.params) {
        draw.uniforms.push_back({name, val});
    }
    for (auto& [name, tex] : material.textures) {
        draw.uniforms.push_back({name, tex});
    }

    // Model transform
    draw.uniforms.push_back({"matModel", transform});
    draw.uniforms.push_back({
        "matView", current_view_.view_matrix
    });
    draw.uniforms.push_back({
        "matProjection", current_view_.projection_matrix
    });

    rhi::SortKey key{
        .layer       = material.layer,
        .translucent = material.is_translucent(),
        .shader_id   = static_cast<uint16_t>(
            material.shader.index & 0xFFF
        ),
        .material_id = material.id,
        .depth       = camera_depth,
    };

    if (material.is_translucent()) {
        transparent_bucket_.push(key, std::move(draw));
    } else {
        opaque_bucket_.push(key, std::move(draw));
    }
}

void Renderer::build_render_graph() {
    graph_.clear();

    // Pass 1: Shadow maps (placeholder)
    graph_.add_pass({
        .name = "shadows",
        .writes = {"shadow_map"},
        .execute = [this](rhi::IRHI& rhi) {
            // TODO: CSM shadow pass
        },
    });

    // Pass 2: Main opaque geometry
    graph_.add_pass({
        .name = "opaque",
        .reads = {"shadow_map"},
        .writes = {"main_color", "main_depth"},
        .execute = [this](rhi::IRHI& rhi) {
            rhi.bind_render_target(
                rhi::RenderTargetHandle::invalid()
            );
            rhi.clear(clear_flags_);
            rhi.set_viewport(
                current_view_.viewport_x,
                current_view_.viewport_y,
                current_view_.viewport_w,
                current_view_.viewport_h
            );

            opaque_bucket_.sort_and_flush(rhi);
        },
    });

    // Pass 3: Transparent geometry
    graph_.add_pass({
        .name = "transparent",
        .reads = {"main_depth"},
        .writes = {"main_color"},
        .execute = [this](rhi::IRHI& rhi) {
            transparent_bucket_.sort_and_flush(rhi);
        },
    });

    // Pass 4: Post-processing (placeholder)
    graph_.add_pass({
        .name = "post_process",
        .reads = {"main_color", "main_depth"},
        .writes = {"final"},
        .execute = [this](rhi::IRHI& rhi) {
            // TODO: SSAO, bloom, color grading
        },
    });
}

void Renderer::begin_frame() {
    rhi_->begin_frame();
    opaque_bucket_.clear();
    transparent_bucket_.clear();
}

void Renderer::end_frame() {
    build_render_graph();
    graph_.execute(*rhi_);
    rhi_->end_frame();
}

} // namespace retro::rendering
```

---

## Usage Example — `main.cpp`

How it all comes together from the application side.

```cpp
#include "rhi/RHI.hpp"
#include "rendering/Renderer.hpp"

using namespace retro;

int main() {
    // Create RHI — swap Backend::Raylib for Backend::Bgfx
    // later with zero changes above this line.
    auto rhi = rhi::create_rhi(rhi::Backend::Raylib);

    auto result = rhi->initialize(1280, 720, "Retro Engine");
    if (!result) {
        return 1;
    }

    rendering::Renderer renderer(std::move(rhi));

    // Load a material from data
    auto* mat = renderer.materials().load(
        "assets/materials/toon.toml"
    );

    // Create a mesh (normally through asset pipeline)
    // ... mesh loading code ...

    rendering::View view{};
    view.viewport_w = 1280;
    view.viewport_h = 720;
    // ... set up view/projection matrices ...

    renderer.set_view(view);
    renderer.set_clear_color({0.1f, 0.1f, 0.12f, 1.0f});

    while (!renderer.should_close()) {
        renderer.begin_frame();

        // Scene collector would do this automatically
        // from ECS queries:
        // renderer.submit_renderable(
        //     mesh, *mat, transform, depth
        // );

        renderer.end_frame();
    }

    return 0;
}
```

---

## Key Architectural Properties

| Property | How it's achieved |
|---|---|
| **Backend swappable** | Everything above `IRHI` never includes raylib. New backend = new `IRHI` impl + a case in `create_rhi()`. |
| **No raw GPU state leaks** | Generational handles. You can't dereference a destroyed texture. |
| **Draw order is data, not code** | Sort key encodes layer, translucency, shader, material, depth in one `uint64_t`. No manual ordering. |
| **Materials are data files** | TOML → `Material` struct → uniforms bound at draw time. Artists/designers never touch C++. |
| **Pipeline is declarative** | Render graph declares passes, reads, writes. Passes can be added/removed without touching other passes. |
| **Batch-friendly** | Sort key groups by shader then material — minimizes state changes automatically. |
| **Future-proof for LPV/SSAO/Bloom** | They're just render graph passes that read/write named render targets. The plumbing exists, the implementation slots in. |

When you move to bgfx, the `submit()` implementation becomes `bgfx::submit()` with an encoded view ID, and everything upstream — materials, sort keys, the render graph — stays identical.