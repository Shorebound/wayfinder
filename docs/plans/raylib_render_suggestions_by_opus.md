
---

## Raylib Backend — `rhi/backends/raylib/RaylibRHI.hpp`

```cpp
#pragma once

#include "../../RHI.hpp"
#include "../../ResourcePool.hpp"

#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>

#include <unordered_map>

namespace retro::rhi {

// Internal resource wrappers — raylib-specific
struct RaylibTexture {
    Texture2D     texture{};
    RenderTexture render_texture{}; // only if used as RT attachment
    bool          is_render_texture = false;
};

struct RaylibShader {
    Shader shader{};
    // Cache uniform locations by name
    std::unordered_map<std::string, int> uniform_locs;
};

struct RaylibMesh {
    Mesh          mesh{};
    Model         model{}; // raylib ties meshes to models for drawing
    uint32_t      index_count = 0;
    PrimitiveType primitive = PrimitiveType::Triangles;
};

struct RaylibRenderTarget {
    RenderTexture2D  rt{};
    RenderTargetDesc desc;
};

class RaylibRHI final : public IRHI {
public:
    ~RaylibRHI() override;

    Result<void> initialize(
        uint32_t width, uint32_t height,
        std::string_view window_title
    ) override;
    void shutdown() override;
    bool should_close() const override;
    void begin_frame() override;
    void end_frame() override;

    Result<TextureHandle> create_texture(
        const TextureDesc& desc,
        std::span<const std::byte> pixels
    ) override;
    Result<ShaderHandle> create_shader(
        const ShaderDesc& desc
    ) override;
    Result<MeshHandle> create_mesh(
        const MeshDesc& desc
    ) override;
    Result<RenderTargetHandle> create_render_target(
        const RenderTargetDesc& desc
    ) override;

    void destroy(TextureHandle h) override;
    void destroy(ShaderHandle h) override;
    void destroy(MeshHandle h) override;
    void destroy(RenderTargetHandle h) override;

    void update_texture(
        TextureHandle h,
        std::span<const std::byte> pixels
    ) override;

    void bind_render_target(RenderTargetHandle h) override;
    void clear(const ClearFlags& flags) override;
    void set_viewport(
        uint32_t x, uint32_t y,
        uint32_t w, uint32_t h
    ) override;

    void submit(const DrawCall& draw) override;

    TextureHandle get_render_target_color(
        RenderTargetHandle h, uint8_t index
    ) const override;
    TextureHandle get_render_target_depth(
        RenderTargetHandle h
    ) const override;
    std::pair<uint32_t, uint32_t>
        get_backbuffer_size() const override;

    void push_debug_group(std::string_view label) override;
    void pop_debug_group() override;

private:
    void apply_pipeline_state(const PipelineState& state);
    void bind_uniforms(
        RaylibShader& shader,
        const std::vector<DrawCall::Binding>& uniforms
    );
    int get_or_cache_uniform_loc(
        RaylibShader& shader,
        const std::string& name
    );

    ResourcePool<TextureTag, RaylibTexture>           textures_;
    ResourcePool<ShaderTag, RaylibShader>              shaders_;
    ResourcePool<MeshTag, RaylibMesh>                  meshes_;
    ResourcePool<RenderTargetTag, RaylibRenderTarget>  render_targets_;

    RenderTargetHandle active_rt_ = RenderTargetHandle::invalid();
    uint32_t screen_width_  = 0;
    uint32_t screen_height_ = 0;
    bool     initialized_   = false;
};

} // namespace retro::rhi
```

## Raylib Backend — `rhi/backends/raylib/RaylibRHI.cpp`

```cpp
#include "RaylibRHI.hpp"

#include <format>

namespace retro::rhi {

// ── Factory ──────────────────────────────────────────────

std::unique_ptr<IRHI> create_rhi(Backend backend) {
    switch (backend) {
        case Backend::Raylib:
            return std::make_unique<RaylibRHI>();
        // Future backends go here
    }
    return nullptr;
}

// ── Lifecycle ────────────────────────────────────────────

RaylibRHI::~RaylibRHI() {
    if (initialized_) shutdown();
}

Result<void> RaylibRHI::initialize(
    uint32_t width, uint32_t height,
    std::string_view window_title
) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        std::string(window_title).c_str()
    );

    if (!IsWindowReady()) {
        return std::unexpected(
            RHIError{"Failed to create raylib window"}
        );
    }

    SetTargetFPS(60);
    screen_width_ = width;
    screen_height_ = height;
    initialized_ = true;

    return {};
}

void RaylibRHI::shutdown() {
    if (!initialized_) return;
    CloseWindow();
    initialized_ = false;
}

bool RaylibRHI::should_close() const {
    return WindowShouldClose();
}

void RaylibRHI::begin_frame() {
    BeginDrawing();
}

void RaylibRHI::end_frame() {
    EndDrawing();
}

// ── Texture ──────────────────────────────────────────────

Result<TextureHandle> RaylibRHI::create_texture(
    const TextureDesc& desc,
    std::span<const std::byte> pixels
) {
    Image image{};
    image.width   = static_cast<int>(desc.width);
    image.height  = static_cast<int>(desc.height);
    image.mipmaps = 1;

    switch (desc.format) {
        case TextureFormat::RGBA8:
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            break;
        case TextureFormat::RGB8:
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
            break;
        case TextureFormat::R8:
            image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
            break;
        default:
            return std::unexpected(RHIError{std::format(
                "Unsupported texture format for raylib: {}",
                static_cast<int>(desc.format)
            )});
    }

    if (!pixels.empty()) {
        // raylib wants a non-const void* but won't modify it
        // during LoadTextureFromImage
        image.data = const_cast<void*>(
            static_cast<const void*>(pixels.data())
        );
    } else {
        image.data = RL_CALLOC(
            desc.width * desc.height * 4, 1
        );
    }

    RaylibTexture tex;
    tex.texture = LoadTextureFromImage(image);

    // Apply filtering
    switch (desc.filter) {
        case TextureFilter::Nearest:
            SetTextureFilter(
                tex.texture, TEXTURE_FILTER_POINT
            );
            break;
        case TextureFilter::Bilinear:
            SetTextureFilter(
                tex.texture, TEXTURE_FILTER_BILINEAR
            );
            break;
        case TextureFilter::Trilinear:
            SetTextureFilter(
                tex.texture, TEXTURE_FILTER_TRILINEAR
            );
            break;
    }

    // Apply wrap mode
    switch (desc.wrap) {
        case TextureWrap::Repeat:
            SetTextureWrap(tex.texture, TEXTURE_WRAP_REPEAT);
            break;
        case TextureWrap::Clamp:
            SetTextureWrap(tex.texture, TEXTURE_WRAP_CLAMP);
            break;
        case TextureWrap::Mirror:
            SetTextureWrap(
                tex.texture, TEXTURE_WRAP_MIRROR_REPEAT
            );
            break;
    }

    if (desc.mips) {
        GenTextureMipmaps(&tex.texture);
    }

    // Free the temp buffer only if we allocated it
    if (pixels.empty()) {
        RL_FREE(image.data);
    }

    return textures_.acquire(std::move(tex));
}

void RaylibRHI::destroy(TextureHandle h) {
    if (auto* tex = textures_.get(h)) {
        if (tex->is_render_texture) {
            UnloadRenderTexture(tex->render_texture);
        } else {
            UnloadTexture(tex->texture);
        }
        textures_.release(h);
    }
}

void RaylibRHI::update_texture(
    TextureHandle h,
    std::span<const std::byte> pixels
) {
    if (auto* tex = textures_.get(h)) {
        UpdateTexture(
            tex->texture,
            static_cast<const void*>(pixels.data())
        );
    }
}

// ── Shader ───────────────────────────────────────────────

Result<ShaderHandle> RaylibRHI::create_shader(
    const ShaderDesc& desc
) {
    RaylibShader s;

    // raylib can load from strings via LoadShaderFromMemory
    const char* vs = desc.vertex_source.empty()
        ? nullptr
        : desc.vertex_source.c_str();
    const char* fs = desc.fragment_source.empty()
        ? nullptr
        : desc.fragment_source.c_str();

    s.shader = LoadShaderFromMemory(vs, fs);

    if (s.shader.id == 0) {
        return std::unexpected(RHIError{std::format(
            "Failed to compile shader '{}'", desc.debug_name
        )});
    }

    return shaders_.acquire(std::move(s));
}

void RaylibRHI::destroy(ShaderHandle h) {
    if (auto* s = shaders_.get(h)) {
        UnloadShader(s->shader);
        shaders_.release(h);
    }
}

int RaylibRHI::get_or_cache_uniform_loc(
    RaylibShader& shader,
    const std::string& name
) {
    auto it = shader.uniform_locs.find(name);
    if (it != shader.uniform_locs.end()) {
        return it->second;
    }
    int loc = GetShaderLocation(shader.shader, name.c_str());
    shader.uniform_locs[name] = loc;
    return loc;
}

// ── Mesh ─────────────────────────────────────────────────

Result<MeshHandle> RaylibRHI::create_mesh(const MeshDesc& desc) {
    // Calculate vertex count from layout stride and data size
    uint32_t stride = desc.layout.stride;
    if (stride == 0) {
        for (auto& attr : desc.layout.attribs) {
            stride += attr.components * sizeof(float);
        }
    }

    uint32_t vertex_count = static_cast<uint32_t>(
        desc.vertex_data.size() / stride
    );

    Mesh mesh{};
    mesh.vertexCount   = static_cast<int>(vertex_count);
    mesh.triangleCount = desc.index_data.empty()
        ? static_cast<int>(vertex_count / 3)
        : static_cast<int>(desc.index_data.size() / 3);

    // Parse vertex data into raylib's separate-array format.
    // Raylib wants float* vertices, float* normals, etc.
    // We need to deinterleave.
    const auto* raw = reinterpret_cast<const float*>(
        desc.vertex_data.data()
    );

    // Compute per-attrib offsets within the interleaved vertex
    uint32_t float_offset = 0;
    struct AttribMapping {
        VertexAttrib type;
        uint32_t     float_offset;
        uint8_t      components;
    };
    std::vector<AttribMapping> mappings;
    for (auto& attr : desc.layout.attribs) {
        mappings.push_back({
            attr.type, float_offset, attr.components
        });
        float_offset += attr.components;
    }
    uint32_t floats_per_vertex = float_offset;

    for (auto& m : mappings) {
        auto* buf = static_cast<float*>(RL_MALLOC(
            vertex_count * m.components * sizeof(float)
        ));
        for (uint32_t v = 0; v < vertex_count; ++v) {
            for (uint8_t c = 0; c < m.components; ++c) {
                buf[v * m.components + c] =
                    raw[v * floats_per_vertex + m.float_offset + c];
            }
        }

        switch (m.type) {
            case VertexAttrib::Position:
                mesh.vertices = buf; break;
            case VertexAttrib::Normal:
                mesh.normals = buf; break;
            case VertexAttrib::TexCoord0:
                mesh.texcoords = buf; break;
            case VertexAttrib::TexCoord1:
                mesh.texcoords2 = buf; break;
            case VertexAttrib::Color0: {
                // Convert float colors to unsigned char
                auto* cbuf = static_cast<unsigned char*>(
                    RL_MALLOC(vertex_count * 4)
                );
                for (uint32_t v = 0; v < vertex_count; ++v) {
                    for (uint8_t c = 0; c < 4; ++c) {
                        float val = (c < m.components)
                            ? buf[v * m.components + c]
                            : 1.0f;
                        cbuf[v * 4 + c] = static_cast<
                            unsigned char>(val * 255.0f);
                    }
                }
                mesh.colors = cbuf;
                RL_FREE(buf);
                break;
            }
            case VertexAttrib::Tangent:
                mesh.tangents = buf; break;
            default:
                RL_FREE(buf);
                break;
        }
    }

    // Indices
    if (!desc.index_data.empty()) {
        mesh.indices = static_cast<unsigned short*>(RL_MALLOC(
            desc.index_data.size() * sizeof(unsigned short)
        ));
        std::memcpy(
            mesh.indices,
            desc.index_data.data(),
            desc.index_data.size() * sizeof(unsigned short)
        );
    }

    UploadMesh(&mesh, false);

    RaylibMesh rm;
    rm.mesh = mesh;
    rm.index_count = desc.index_data.empty()
        ? vertex_count
        : static_cast<uint32_t>(desc.index_data.size());
    rm.primitive = desc.primitive;

    // Create a Model wrapper (raylib uses Models for drawing)
    rm.model = LoadModelFromMesh(mesh);

    return meshes_.acquire(std::move(rm));
}

void RaylibRHI::destroy(MeshHandle h) {
    if (auto* m = meshes_.get(h)) {
        UnloadModel(m->model);
        meshes_.release(h);
    }
}

// ── Render Target ────────────────────────────────────────

Result<RenderTargetHandle> RaylibRHI::create_render_target(
    const RenderTargetDesc& desc
) {
    RaylibRenderTarget rt;
    rt.desc = desc;
    rt.rt = LoadRenderTexture(
        static_cast<int>(desc.width),
        static_cast<int>(desc.height)
    );

    if (rt.rt.id == 0) {
        return std::unexpected(RHIError{std::format(
            "Failed to create render target '{}'",
            desc.debug_name
        )});
    }

    return render_targets_.acquire(std::move(rt));
}

void RaylibRHI::destroy(RenderTargetHandle h) {
    if (auto* rt = render_targets_.get(h)) {
        UnloadRenderTexture(rt->rt);
        render_targets_.release(h);
    }
}

// ── Render Target Binding ────────────────────────────────

void RaylibRHI::bind_render_target(RenderTargetHandle h) {
    // Unbind previous if needed
    if (active_rt_.is_valid()) {
        EndTextureMode();
    }

    active_rt_ = h;

    if (h.is_valid()) {
        if (auto* rt = render_targets_.get(h)) {
            BeginTextureMode(rt->rt);
        }
    }
    // invalid handle = draw to backbuffer (default after
    // EndTextureMode)
}

void RaylibRHI::clear(const ClearFlags& flags) {
    if (flags.color) {
        ClearBackground(Color{
            static_cast<unsigned char>(
                flags.clear_color.x * 255
            ),
            static_cast<unsigned char>(
                flags.clear_color.y * 255
            ),
            static_cast<unsigned char>(
                flags.clear_color.z * 255
            ),
            static_cast<unsigned char>(
                flags.clear_color.w * 255
            ),
        });
    }
    // raylib doesn't expose separate depth clear — it's
    // handled automatically. A real backend (Vulkan/bgfx)
    // would clear depth here.
}

void RaylibRHI::set_viewport(
    uint32_t x, uint32_t y,
    uint32_t w, uint32_t h
) {
    rlViewport(
        static_cast<int>(x), static_cast<int>(y),
        static_cast<int>(w), static_cast<int>(h)
    );
}

// ── Pipeline State ───────────────────────────────────────

void RaylibRHI::apply_pipeline_state(
    const PipelineState& state
) {
    // Blending
    switch (state.blend) {
        case BlendMode::Opaque:
            rlDisableColorBlend();
            break;
        case BlendMode::AlphaBlend:
            rlEnableColorBlend();
            rlSetBlendMode(BLEND_ALPHA);
            break;
        case BlendMode::Additive:
            rlEnableColorBlend();
            rlSetBlendMode(BLEND_ADDITIVE);
            break;
        case BlendMode::Multiply:
            rlEnableColorBlend();
            rlSetBlendMode(BLEND_MULTIPLIED);
            break;
        case BlendMode::Dither:
            rlDisableColorBlend();
            // Dither handled in shader
            break;
    }

    // Depth
    if (state.depth != DepthFunc::None) {
        rlEnableDepthTest();
        rlEnableDepthMask(); // simplified
    } else {
        rlDisableDepthTest();
    }

    // Culling
    switch (state.cull) {
        case CullFace::Back:
            rlEnableBackfaceCulling();
            break;
        case CullFace::Front:
            // raylib doesn't natively support front-face
            // culling cleanly; we'd use rlgl
            rlEnableBackfaceCulling();
            break;
        case CullFace::None:
            rlDisableBackfaceCulling();
            break;
    }
}

// ── Uniform Binding ──────────────────────────────────────

void RaylibRHI::bind_uniforms(
    RaylibShader& shader,
    const std::vector<DrawCall::Binding>& uniforms
) {
    int texture_slot = 1; // slot 0 reserved for diffuse

    for (auto& binding : uniforms) {
        int loc = get_or_cache_uniform_loc(
            shader, binding.name
        );
        if (loc < 0) continue;

        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) {
                SetShaderValue(
                    shader.shader, loc, &val,
                    SHADER_UNIFORM_FLOAT
                );
            } else if constexpr (std::is_same_v<T, int32_t>) {
                SetShaderValue(
                    shader.shader, loc, &val,
                    SHADER_UNIFORM_INT
                );
            } else if constexpr (std::is_same_v<T, Vec2>) {
                SetShaderValue(
                    shader.shader, loc, &val,
                    SHADER_UNIFORM_VEC2
                );
            } else if constexpr (std::is_same_v<T, Vec3>) {
                SetShaderValue(
                    shader.shader, loc, &val,
                    SHADER_UNIFORM_VEC3
                );
            } else if constexpr (std::is_same_v<T, Vec4>) {
                SetShaderValue(
                    shader.shader, loc, &val,
                    SHADER_UNIFORM_VEC4
                );
            } else if constexpr (std::is_same_v<T, Mat4>) {
                SetShaderValueMatrix(
                    shader.shader, loc,
                    *reinterpret_cast<const Matrix*>(&val)
                );
            } else if constexpr (
                std::is_same_v<T, TextureHandle>
            ) {
                if (auto* tex = textures_.get(val)) {
                    SetShaderValueTexture(
                        shader.shader, loc, tex->texture
                    );
                    texture_slot++;
                }
            }
        }, binding.value);
    }
}

// ── Draw Submission ──────────────────────────────────────

void RaylibRHI::submit(const DrawCall& draw) {
    auto* mesh_res   = meshes_.get(draw.mesh);
    auto* shader_res = shaders_.get(draw.shader);
    if (!mesh_res) return;

    apply_pipeline_state(draw.pipeline);

    if (shader_res) {
        BeginShaderMode(shader_res->shader);
        bind_uniforms(shader_res, draw.uniforms);
    }

    // Set the shader on the model's material
    if (shader_res) {
        mesh_res->model.materials[0].shader =
            shader_res->shader;
    }

    DrawModel(
        mesh_res->model,
        Vector3{0, 0, 0},
        1.0f,
        WHITE
    );

    if (shader_res) {
        EndShaderMode();
    }
}

// ── Queries ──────────────────────────────────────────────

TextureHandle RaylibRHI::get_render_target_color(
    RenderTargetHandle h, uint8_t /*index*/
) const {
    // In a real implementation you'd have a handle mapping.
    // For raylib, RT textures are accessed through the
    // RT itself. This is a limitation we'd improve with
    // bgfx/vulkan.
    return TextureHandle::invalid();
}

TextureHandle RaylibRHI::get_render_target_depth(
    RenderTargetHandle h
) const {
    return TextureHandle::invalid();
}

std::pair<uint32_t, uint32_t>
RaylibRHI::get_backbuffer_size() const {
    return {
        static_cast<uint32_t>(GetScreenWidth()),
        static_cast<uint32_t>(GetScreenHeight())
    };
}

// ── Debug ────────────────────────────────────────────────

void RaylibRHI::push_debug_group(std::string_view /*label*/) {
    // No-op for raylib. Vulkan/bgfx would insert a GPU
    // debug marker.
}

void RaylibRHI::pop_debug_group() {
    // No-op
}

} // namespace retro::rhi
```

---