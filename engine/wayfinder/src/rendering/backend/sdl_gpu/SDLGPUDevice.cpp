#include "SDLGPUDevice.h"
#include "BlitMipGenerator.h"
#include "platform/Window.h"
#include "rendering/backend/null/NullDevice.h"

#include "core/Log.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <format>
#include <ranges>
#include <vector>

namespace Wayfinder
{
    namespace
    {
        SDL_GPUVertexElementFormat ToSDLVertexFormat(VertexAttributeFormat fmt)
        {
            switch (fmt)
            {
            case VertexAttributeFormat::Float2:
                return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            case VertexAttributeFormat::Float3:
                return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            case VertexAttributeFormat::Float4:
                return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            }
            return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        }

        SDL_GPUBlendFactor ToSDLBlendFactor(BlendFactor f)
        {
            switch (f)
            {
            case BlendFactor::Zero:
                return SDL_GPU_BLENDFACTOR_ZERO;
            case BlendFactor::One:
                return SDL_GPU_BLENDFACTOR_ONE;
            case BlendFactor::SrcAlpha:
                return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha:
                return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha:
                return SDL_GPU_BLENDFACTOR_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha:
                return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
            case BlendFactor::SrcColour:
                return SDL_GPU_BLENDFACTOR_SRC_COLOR;
            case BlendFactor::OneMinusSrcColour:
                return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColour:
                return SDL_GPU_BLENDFACTOR_DST_COLOR;
            case BlendFactor::OneMinusDstColour:
                return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
            case BlendFactor::ConstantColour:
                return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
            case BlendFactor::OneMinusConstantColour:
                return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
            }
            return SDL_GPU_BLENDFACTOR_ONE;
        }

        SDL_GPUBlendOp ToSDLBlendOp(BlendOp op)
        {
            switch (op)
            {
            case BlendOp::Add:
                return SDL_GPU_BLENDOP_ADD;
            case BlendOp::Subtract:
                return SDL_GPU_BLENDOP_SUBTRACT;
            case BlendOp::ReverseSubtract:
                return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
            case BlendOp::Min:
                return SDL_GPU_BLENDOP_MIN;
            case BlendOp::Max:
                return SDL_GPU_BLENDOP_MAX;
            }
            return SDL_GPU_BLENDOP_ADD;
        }

        SDL_GPUTextureFormat ToSDLTextureFormat(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::SwapchainFormat:
            case TextureFormat::RGBA8_UNORM:
                return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            case TextureFormat::BGRA8_UNORM:
                return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
            case TextureFormat::R16_FLOAT:
                return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
            case TextureFormat::RGBA16_FLOAT:
                return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            case TextureFormat::R32_FLOAT:
                return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
            case TextureFormat::D32_FLOAT:
                return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            case TextureFormat::D24_UNORM_S8:
                return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
            }
            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        }

        SDL_GPUTextureUsageFlags ToSDLTextureUsage(TextureUsage usage)
        {
            SDL_GPUTextureUsageFlags flags = 0;
            if (HasFlag(usage, TextureUsage::Sampler))
            {
                flags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
            }
            if (HasFlag(usage, TextureUsage::ColourTarget))
            {
                flags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            }
            if (HasFlag(usage, TextureUsage::DepthTarget))
            {
                flags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            }
            return flags;
        }

    } // namespace

    // ── Factory ──────────────────────────────────────────────

    std::unique_ptr<RenderDevice> RenderDevice::Create(RenderBackend backend)
    {
        switch (backend)
        {
        case RenderBackend::SDL_GPU:
            return std::make_unique<SDLGPUDevice>();
        case RenderBackend::Null:
            return std::make_unique<NullDevice>();
        }

        return nullptr;
    }

    // ── Lifecycle ────────────────────────────────────────────

    SDLGPUDevice::~SDLGPUDevice() noexcept
    {
        try
        {
            Shutdown();
        }
        catch (const std::exception& ex)
        {
            WAYFINDER_WARN(LogRenderer, "SDLGPUDevice::~SDLGPUDevice suppressed exception during Shutdown(): {}", ex.what());
        }
        catch (...)
        {
            WAYFINDER_WARN(LogRenderer, "SDLGPUDevice::~SDLGPUDevice suppressed unknown exception during Shutdown().");
        }
    }

    Result<void> SDLGPUDevice::Initialise(Window& window)
    {
        m_window = static_cast<SDL_Window*>(window.GetNativeHandle());
        if (!m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Window has no valid native handle");
            return MakeError("SDLGPUDevice: Window has no valid native handle");
        }

        // Only request formats we can actually provide.
        // Currently all shaders are compiled to SPIR-V.
        m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,
            true, // debug mode
            nullptr);

        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to create GPU device — {}", SDL_GetError());
            return MakeError(std::format("SDLGPUDevice: Failed to create GPU device — {}", SDL_GetError()));
        }

        m_shaderFormats = SDL_GetGPUShaderFormats(m_device);

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to claim window for GPU device — {}", SDL_GetError());
            auto error = MakeError(std::format("SDLGPUDevice: Failed to claim window — {}", SDL_GetError()));
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return error;
        }

        m_info.BackendName = "SDL_GPU";

        const char* driver = SDL_GetGPUDeviceDriver(m_device);
        m_info.DriverInfo = driver ? driver : "unknown";

        WAYFINDER_INFO(LogRenderer, "SDLGPUDevice: Initialised (driver: {})", m_info.DriverInfo);

        m_mipGenerator = std::make_unique<BlitMipGenerator>();

        // Create persistent staging ring for batched uploads
        SDL_GPUTransferBufferCreateInfo stagingInfo{};
        stagingInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        stagingInfo.size = STAGING_RING_CAPACITY;

        m_stagingRing = SDL_CreateGPUTransferBuffer(m_device, &stagingInfo);
        if (!m_stagingRing)
        {
            WAYFINDER_WARN(LogRenderer, "SDLGPUDevice: Failed to create staging ring — uploads will use dedicated buffers");
        }

        return {};
    }

    void SDLGPUDevice::Shutdown()
    {
        if (m_device)
        {
            // Release all pooled GPU resources before destroying the device.
            m_shaderPool.ForEachAlive([&](SDL_GPUShader* s)
            {
                SDL_ReleaseGPUShader(m_device, s);
            });
            m_shaderPool.Clear();

            m_pipelinePool.ForEachAlive([&](SDL_GPUGraphicsPipeline* p)
            {
                SDL_ReleaseGPUGraphicsPipeline(m_device, p);
            });
            m_pipelinePool.Clear();

            m_computePipelinePool.ForEachAlive([&](SDL_GPUComputePipeline* p)
            {
                SDL_ReleaseGPUComputePipeline(m_device, p);
            });
            m_computePipelinePool.Clear();

            m_bufferPool.ForEachAlive([&](SDL_GPUBuffer* b)
            {
                SDL_ReleaseGPUBuffer(m_device, b);
            });
            m_bufferPool.Clear();

            m_samplerPool.ForEachAlive([&](SDL_GPUSampler* s)
            {
                SDL_ReleaseGPUSampler(m_device, s);
            });
            m_samplerPool.Clear();

            m_texturePool.ForEachAlive([&](TextureEntry& entry)
            {
                SDL_ReleaseGPUTexture(m_device, entry.texture);
            });
            m_texturePool.Clear();

            if (m_stagingRing)
            {
                if (m_stagingMapped)
                {
                    SDL_UnmapGPUTransferBuffer(m_device, m_stagingRing);
                    m_stagingMapped = nullptr;
                }
                SDL_ReleaseGPUTransferBuffer(m_device, m_stagingRing);
                m_stagingRing = nullptr;
                m_stagingCursor = 0;
            }
            m_pendingBufferCopies.clear();
            m_pendingTextureCopies.clear();

            if (m_depthTexture)
            {
                SDL_ReleaseGPUTexture(m_device, m_depthTexture);
                m_depthTexture = nullptr;
            }

            if (m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            m_window = nullptr;
        }
    }

    // ── Frame Lifecycle ──────────────────────────────────────

    bool SDLGPUDevice::BeginFrame()
    {
        m_commandBuffer = SDL_AcquireGPUCommandBuffer(m_device);
        if (!m_commandBuffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to acquire command buffer — {}", SDL_GetError());
            return false;
        }

        // Map the staging ring for this frame's uploads (cycle=true lets the
        // driver internally double-buffer so we don't stall on the previous frame).
        if (m_stagingRing && !m_stagingMapped)
        {
            m_stagingMapped = SDL_MapGPUTransferBuffer(m_device, m_stagingRing, true);
            m_stagingCursor = 0;
        }

        if (!SDL_AcquireGPUSwapchainTexture(m_commandBuffer, m_window, &m_swapchainTexture, &m_swapchainWidth, &m_swapchainHeight))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to acquire swapchain texture — {}", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        if (!m_swapchainTexture)
        {
            // Window is minimised or occluded — submit empty command buffer and skip rendering
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        EnsureDepthTexture(m_swapchainWidth, m_swapchainHeight);

        return true;
    }

    void SDLGPUDevice::EndFrame()
    {
        // Flush any uploads that weren't consumed (e.g. graph failed to compile).
        FlushUploads();

        // Unmap the staging ring — it will be re-mapped next BeginFrame.
        if (m_stagingMapped)
        {
            SDL_UnmapGPUTransferBuffer(m_device, m_stagingRing);
            m_stagingMapped = nullptr;
            m_stagingCursor = 0;
        }

        if (m_commandBuffer)
        {
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
        }

        m_swapchainTexture = nullptr;
    }

    void SDLGPUDevice::PushDebugGroup(std::string_view name)
    {
        if (!m_commandBuffer)
        {
            return;
        }

        const std::string label = name.empty() ? "Unnamed GPU Scope" : std::string(name);
        SDL_PushGPUDebugGroup(m_commandBuffer, label.c_str());
    }

    void SDLGPUDevice::PopDebugGroup()
    {
        if (m_commandBuffer)
        {
            SDL_PopGPUDebugGroup(m_commandBuffer);
        }
    }

    void SDLGPUDevice::EnsureDepthTexture(uint32_t width, uint32_t height)
    {
        if (m_depthTexture && m_depthWidth == width && m_depthHeight == height)
        {
            return;
        }

        if (m_depthTexture)
        {
            SDL_ReleaseGPUTexture(m_device, m_depthTexture);
            m_depthTexture = nullptr;
        }

        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        m_depthTexture = SDL_CreateGPUTexture(m_device, &info);
        if (!m_depthTexture)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to create depth texture ({}x{}) — {}", width, height, SDL_GetError());
            return;
        }

        m_depthWidth = width;
        m_depthHeight = height;
    }

    // ── Render Pass ──────────────────────────────────────────

    bool SDLGPUDevice::BeginRenderPass(const RenderPassDescriptor& descriptor)
    {
        if (!m_commandBuffer)
        {
            return false;
        }

        const uint32_t numTargets = std::min(descriptor.ColourTargets, MAX_COLOUR_TARGETS);
        if (numTargets == 0 && !descriptor.DepthAttachment.Enabled)
        {
            WAYFINDER_ERROR(LogRenderer, "BeginRenderPass '{}': no colour targets and no depth attachment — skipping pass", descriptor.DebugName);
            return false;
        }

        // ── Build colour target array ────────────────────────
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        std::array<SDL_GPUColorTargetInfo, MAX_COLOUR_TARGETS> colourTargets{};

        for (uint32_t i = 0; i < numTargets; ++i)
        {
            const auto& attachment = descriptor.ColourAttachments[i];
            SDL_GPUTexture* texture = nullptr;

            if (descriptor.TargetSwapchain && i == 0)
            {
                if (!m_swapchainTexture)
                {
                    return false;
                }
                texture = m_swapchainTexture;
            }
            else if (attachment.Target.IsValid())
            {
                auto* pTex = m_texturePool.Get(attachment.Target);
                texture = pTex ? pTex->texture : nullptr;
            }

            if (!texture)
            {
                WAYFINDER_WARN(LogRenderer, "BeginRenderPass '{}': colour target at slot {} is null — skipping pass", descriptor.DebugName, i);
                return false;
            }

            auto& target = colourTargets[i];
            target.texture = texture;
            target.clear_color.r = attachment.ClearColour.Data.r;
            target.clear_color.g = attachment.ClearColour.Data.g;
            target.clear_color.b = attachment.ClearColour.Data.b;
            target.clear_color.a = attachment.ClearColour.Data.a;

            switch (attachment.LoadOp)
            {
            case LoadOp::Clear:
                target.load_op = SDL_GPU_LOADOP_CLEAR;
                break;
            case LoadOp::Load:
                target.load_op = SDL_GPU_LOADOP_LOAD;
                break;
            case LoadOp::DontCare:
                target.load_op = SDL_GPU_LOADOP_DONT_CARE;
                break;
            }

            switch (attachment.StoreOp)
            {
            case StoreOp::Store:
                target.store_op = SDL_GPU_STOREOP_STORE;
                break;
            case StoreOp::DontCare:
                target.store_op = SDL_GPU_STOREOP_DONT_CARE;
                break;
            }
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

        // ── Depth target ─────────────────────────────────────
        if (descriptor.DepthAttachment.Enabled)
        {
            SDL_GPUTexture* depthTexture = nullptr;
            if (descriptor.DepthTarget.IsValid())
            {
                auto* pTex = m_texturePool.Get(descriptor.DepthTarget);
                depthTexture = pTex ? pTex->texture : nullptr;
            }
            else if (m_depthTexture)
            {
                depthTexture = m_depthTexture;
            }

            if (depthTexture)
            {
                SDL_GPUDepthStencilTargetInfo depthTarget{};
                depthTarget.texture = depthTexture;
                depthTarget.clear_depth = descriptor.DepthAttachment.ClearDepth;

                switch (descriptor.DepthAttachment.LoadOp)
                {
                case LoadOp::Clear:
                    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
                    break;
                case LoadOp::Load:
                    depthTarget.load_op = SDL_GPU_LOADOP_LOAD;
                    break;
                case LoadOp::DontCare:
                    depthTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
                    break;
                }

                switch (descriptor.DepthAttachment.StoreOp)
                {
                case StoreOp::Store:
                    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
                    break;
                case StoreOp::DontCare:
                    depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
                    break;
                }

                depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

                m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, colourTargets.data(), numTargets, &depthTarget);
            }
            else
            {
                WAYFINDER_ERROR(LogRenderer, "BeginRenderPass '{}': depth attachment enabled but no depth texture available (depthTarget={}, m_depthTexture={})", descriptor.DebugName, descriptor.DepthTarget.IsValid(),
                    m_depthTexture != nullptr);
                return false;
            }
        }
        else
        {
            m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, colourTargets.data(), numTargets, nullptr);
        }

        return m_renderPass != nullptr;
    }

    void SDLGPUDevice::EndRenderPass()
    {
        if (m_renderPass)
        {
            SDL_EndGPURenderPass(m_renderPass);
            m_renderPass = nullptr;
        }
    }

    // ── Shader and Pipeline ──────────────────────────────────

    GPUShaderHandle SDLGPUDevice::CreateShader(const ShaderCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateShader: No GPU device");
            return GPUShaderHandle::Invalid();
        }

        SDL_GPUShaderCreateInfo info{};
        info.code = desc.Code;
        info.code_size = desc.CodeSize;
        info.entrypoint = desc.EntryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateShader: Device does not support SPIR-V");
            return GPUShaderHandle::Invalid();
        }
        info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
        info.stage = (desc.Stage == ShaderStage::Vertex) ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = desc.Samplers;
        info.num_storage_textures = desc.StorageTextures;
        info.num_storage_buffers = desc.StorageBuffers;
        info.num_uniform_buffers = desc.UniformBuffers;

        SDL_GPUShader* shader = SDL_CreateGPUShader(m_device, &info);
        if (!shader)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateShader: Failed — {}", SDL_GetError());
            return GPUShaderHandle::Invalid();
        }

        return m_shaderPool.Acquire(shader);
    }

    void SDLGPUDevice::DestroyShader(GPUShaderHandle shader)
    {
        if (!m_device)
        {
            return;
        }
        auto* pShader = m_shaderPool.Get(shader);
        if (pShader)
        {
            SDL_ReleaseGPUShader(m_device, *pShader);
            m_shaderPool.Release(shader);
        }
    }

    GPUPipelineHandle SDLGPUDevice::CreatePipeline(const PipelineCreateDesc& desc)
    {
        if (!m_device || !m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: No GPU device or window");
            return GPUPipelineHandle::Invalid();
        }

        if (desc.ColourTargets == 0 || desc.ColourTargets > MAX_COLOUR_TARGETS)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: numColourTargets={} is out of range [1, {}]", desc.ColourTargets, MAX_COLOUR_TARGETS);
            return GPUPipelineHandle::Invalid();
        }

        // Build vertex attributes
        std::vector<SDL_GPUVertexAttribute> vertexAttribs;
        vertexAttribs.reserve(desc.VertexLayout.AttributeCount);
        for (uint32_t i = 0; i < desc.VertexLayout.AttributeCount; ++i)
        {
            const auto& a = desc.VertexLayout.Attributes[i];
            SDL_GPUVertexAttribute attr{};
            attr.location = a.Location;
            attr.buffer_slot = 0;
            attr.format = ToSDLVertexFormat(a.Format);
            attr.offset = a.Offset;
            vertexAttribs.push_back(attr);
        }

        SDL_GPUVertexInputState vertexInput{};

        SDL_GPUVertexBufferDescription vbDesc{};

        if (desc.VertexLayout.AttributeCount > 0)
        {
            vbDesc.slot = 0;
            vbDesc.pitch = desc.VertexLayout.Stride;
            vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            vbDesc.instance_step_rate = 0;

            vertexInput.vertex_buffer_descriptions = &vbDesc;
            vertexInput.num_vertex_buffers = 1;
            vertexInput.vertex_attributes = vertexAttribs.data();
            vertexInput.num_vertex_attributes = static_cast<Uint32>(vertexAttribs.size());
        }

        // Rasterizer
        SDL_GPURasterizerState rasterizer{};
        switch (desc.FillMode)
        {
        case FillMode::Fill:
            rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
            break;
        case FillMode::Line:
            rasterizer.fill_mode = SDL_GPU_FILLMODE_LINE;
            break;
        }
        switch (desc.CullMode)
        {
        case CullMode::None:
            rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
            break;
        case CullMode::Front:
            rasterizer.cull_mode = SDL_GPU_CULLMODE_FRONT;
            break;
        case CullMode::Back:
            rasterizer.cull_mode = SDL_GPU_CULLMODE_BACK;
            break;
        }
        switch (desc.FrontFace)
        {
        case FrontFace::CounterClockwise:
            rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            break;
        case FrontFace::Clockwise:
            rasterizer.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
            break;
        }

        // Primitive type
        SDL_GPUPrimitiveType primType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        switch (desc.PrimitiveType)
        {
        case PrimitiveType::TriangleList:
            primType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            break;
        case PrimitiveType::TriangleStrip:
            primType = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
            break;
        case PrimitiveType::LineList:
            primType = SDL_GPU_PRIMITIVETYPE_LINELIST;
            break;
        case PrimitiveType::LineStrip:
            primType = SDL_GPU_PRIMITIVETYPE_LINESTRIP;
            break;
        case PrimitiveType::PointList:
            primType = SDL_GPU_PRIMITIVETYPE_POINTLIST;
            break;
        }

        // Validate blend state
        uint32_t colourTargetIndex = 0;
        for (const BlendState& blend : desc.ColourTargetBlends | std::views::take(desc.ColourTargets))
        {
            if (!blend.Enabled && blend.ColourWriteMask == 0)
            {
                WAYFINDER_WARN(LogRenderer, "CreatePipeline: colour target {} has blending disabled and ColourWriteMask=0 — nothing will be written", colourTargetIndex);
            }
            if (blend.Enabled && blend.ColourWriteMask == 0)
            {
                WAYFINDER_WARN(LogRenderer, "CreatePipeline: colour target {} has blending enabled but ColourWriteMask=0 — blended result will be discarded", colourTargetIndex);
            }
            ++colourTargetIndex;
        }

        // Colour targets — one per MRT attachment.
        // Per-target formats allow deferred rendering with mixed formats (e.g. RGBA8 albedo + RGBA16F normals).
        // SwapchainFormat (value 0, the default for zero-initialised arrays) resolves to the actual swapchain format.
        const SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
        std::array<SDL_GPUColorTargetDescription, MAX_COLOUR_TARGETS> colourTargetDescs{};
        auto targetSlices = std::views::zip(colourTargetDescs, desc.ColourTargetFormats, desc.ColourTargetBlends) | std::views::take(desc.ColourTargets);
        for (auto&& [colourTargetDesc, format, blend] : targetSlices)
        {
            colourTargetDesc.format = (format == TextureFormat::SwapchainFormat) ? swapchainFormat : ToSDLTextureFormat(format);

            if (blend.Enabled)
            {
                colourTargetDesc.blend_state.enable_blend = true;
                colourTargetDesc.blend_state.color_write_mask = blend.ColourWriteMask;

                colourTargetDesc.blend_state.src_color_blendfactor = ToSDLBlendFactor(blend.SrcColourFactor);
                colourTargetDesc.blend_state.dst_color_blendfactor = ToSDLBlendFactor(blend.DstColourFactor);
                colourTargetDesc.blend_state.color_blend_op = ToSDLBlendOp(blend.ColourOp);
                colourTargetDesc.blend_state.src_alpha_blendfactor = ToSDLBlendFactor(blend.SrcAlphaFactor);
                colourTargetDesc.blend_state.dst_alpha_blendfactor = ToSDLBlendFactor(blend.DstAlphaFactor);
                colourTargetDesc.blend_state.alpha_blend_op = ToSDLBlendOp(blend.AlphaOp);
            }
        }

        SDL_GPUGraphicsPipelineTargetInfo targetInfo{};
        targetInfo.color_target_descriptions = colourTargetDescs.data();
        targetInfo.num_color_targets = desc.ColourTargets;
        targetInfo.has_depth_stencil_target = desc.DepthTestEnabled || desc.DepthWriteEnabled;
        if (targetInfo.has_depth_stencil_target)
        {
            targetInfo.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        }

        // Depth-stencil
        SDL_GPUDepthStencilState depthStencil{};
        depthStencil.enable_depth_test = desc.DepthTestEnabled;
        depthStencil.enable_depth_write = desc.DepthWriteEnabled;
        depthStencil.compare_op = SDL_GPU_COMPAREOP_LESS;

        // Resolve shader handles to raw SDL pointers
        auto* pVS = m_shaderPool.Get(desc.VertexShader);
        auto* pFS = m_shaderPool.Get(desc.FragmentShader);
        if (!pVS || !pFS)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: Invalid shader handle(s)");
            return GPUPipelineHandle::Invalid();
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = *pVS;
        pipelineInfo.fragment_shader = *pFS;
        pipelineInfo.vertex_input_state = vertexInput;
        pipelineInfo.primitive_type = primType;
        pipelineInfo.rasterizer_state = rasterizer;
        pipelineInfo.depth_stencil_state = depthStencil;
        pipelineInfo.target_info = targetInfo;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);
        if (!pipeline)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: Failed — {}", SDL_GetError());
            return GPUPipelineHandle::Invalid();
        }

        return m_pipelinePool.Acquire(pipeline);
    }

    void SDLGPUDevice::DestroyPipeline(GPUPipelineHandle pipeline)
    {
        if (!m_device)
        {
            return;
        }
        auto* pPipeline = m_pipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_ReleaseGPUGraphicsPipeline(m_device, *pPipeline);
            m_pipelinePool.Release(pipeline);
        }
    }

    void SDLGPUDevice::BindPipeline(GPUPipelineHandle pipeline)
    {
        if (!m_renderPass)
        {
            return;
        }
        auto* pPipeline = m_pipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_BindGPUGraphicsPipeline(m_renderPass, *pPipeline);
        }
    }

    // ── Buffers ──────────────────────────────────────────────

    GPUBufferHandle SDLGPUDevice::CreateBuffer(const BufferCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateBuffer: No GPU device");
            return GPUBufferHandle::Invalid();
        }

        SDL_GPUBufferCreateInfo info{};
        info.usage = (desc.Usage == BufferUsage::Vertex) ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX;
        info.size = desc.SizeInBytes;

        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(m_device, &info);
        if (!buffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateBuffer: Failed — {}", SDL_GetError());
            return GPUBufferHandle::Invalid();
        }

        return m_bufferPool.Acquire(buffer);
    }

    void SDLGPUDevice::DestroyBuffer(GPUBufferHandle buffer)
    {
        if (!m_device)
        {
            return;
        }
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (pBuffer)
        {
            SDL_ReleaseGPUBuffer(m_device, *pBuffer);
            m_bufferPool.Release(buffer);
        }
    }

    std::optional<uint32_t> SDLGPUDevice::TryStageToRing(const void* data, uint32_t sizeInBytes)
    {
        if (!m_stagingMapped || sizeInBytes > STAGING_RING_CAPACITY)
        {
            return std::nullopt;
        }

        /// @todo 4-byte alignment is sufficient for buffer copies but may not
        /// satisfy driver requirements for all texture formats (e.g. RGBA16_FLOAT
        /// = 8 bytes/texel). Consider format-aware alignment if transfer artefacts
        /// appear on some drivers.
        auto tryAppend = [&]() -> std::optional<uint32_t>
        {
            const uint32_t aligned = (m_stagingCursor + 3u) & ~3u;
            if (aligned + sizeInBytes > STAGING_RING_CAPACITY)
            {
                return std::nullopt;
            }
            std::memcpy(static_cast<uint8_t*>(m_stagingMapped) + aligned, data, sizeInBytes);
            m_stagingCursor = aligned + sizeInBytes;
            return aligned;
        };

        if (auto offset = tryAppend())
        {
            return offset;
        }

        // Ring is full — flush what we have and try once more
        FlushUploads();
        return tryAppend();
    }

    void SDLGPUDevice::UploadToBuffer(GPUBufferHandle buffer, const void* data, BufferUploadRegion region)
    {
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!m_device || !pBuffer || !data || region.SizeInBytes == 0)
        {
            return;
        }

        // Try to stage into the ring buffer
        if (const auto offset = TryStageToRing(data, region.SizeInBytes))
        {
            m_pendingBufferCopies.push_back({
                .ringOffset = *offset,
                .size = region.SizeInBytes,
                .dstBuffer = *pBuffer,
                .dstOffset = region.DstOffsetInBytes,
            });
            return;
        }

        // Oversized or no ring — use dedicated transfer
        UploadToBufferDedicated(*pBuffer, data, region);
    }

    void SDLGPUDevice::UploadToBufferDedicated(SDL_GPUBuffer* buffer, const void* data, BufferUploadRegion region)
    {
        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = region.SizeInBytes;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
        if (!transferBuffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToBuffer: Failed to create transfer buffer — {}", SDL_GetError());
            return;
        }

        void* mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
        if (!mapped)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToBuffer: Failed to map transfer buffer — {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
            return;
        }

        std::memcpy(mapped, data, region.SizeInBytes);
        SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
        if (!cmdBuf)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToBuffer: Failed to acquire command buffer — {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
            return;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTransferBufferLocation src{};
        src.transfer_buffer = transferBuffer;
        src.offset = 0;

        SDL_GPUBufferRegion dst{};
        dst.buffer = buffer;
        dst.offset = region.DstOffsetInBytes;
        dst.size = region.SizeInBytes;

        SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
        SDL_EndGPUCopyPass(copyPass);

        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    }

    // ── Draw Commands ────────────────────────────────────────

    void SDLGPUDevice::BindVertexBuffer(GPUBufferHandle buffer, VertexBufferBindingDesc bindingDesc)
    {
        if (!m_renderPass)
        {
            return;
        }
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!pBuffer)
        {
            return;
        }

        SDL_GPUBufferBinding binding{};
        binding.buffer = *pBuffer;
        binding.offset = bindingDesc.OffsetInBytes;

        SDL_BindGPUVertexBuffers(m_renderPass, bindingDesc.Slot, &binding, 1);
    }

    void SDLGPUDevice::BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes)
    {
        if (!m_renderPass)
        {
            return;
        }
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!pBuffer)
        {
            return;
        }

        SDL_GPUBufferBinding binding{};
        binding.buffer = *pBuffer;
        binding.offset = offsetInBytes;

        const SDL_GPUIndexElementSize sdlIndexSize = (indexSize == IndexElementSize::Uint16) ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT;

        SDL_BindGPUIndexBuffer(m_renderPass, &binding, sdlIndexSize);
    }

    void SDLGPUDevice::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        if (!m_renderPass)
        {
            return;
        }

        SDL_DrawGPUIndexedPrimitives(m_renderPass, indexCount, instanceCount, firstIndex, vertexOffset, 0);
    }

    void SDLGPUDevice::PushVertexUniform(uint32_t slot, const void* data, uint32_t sizeInBytes)
    {
        if (!m_commandBuffer || !data || sizeInBytes == 0)
        {
            return;
        }

        SDL_PushGPUVertexUniformData(m_commandBuffer, slot, data, sizeInBytes);
    }

    void SDLGPUDevice::PushFragmentUniform(uint32_t slot, const void* data, uint32_t sizeInBytes)
    {
        if (!m_commandBuffer || !data || sizeInBytes == 0)
        {
            return;
        }

        SDL_PushGPUFragmentUniformData(m_commandBuffer, slot, data, sizeInBytes);
    }

    void SDLGPUDevice::DrawPrimitives(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        if (!m_renderPass)
        {
            return;
        }

        SDL_DrawGPUPrimitives(m_renderPass, vertexCount, instanceCount, firstVertex, 0);
    }

    // ── Compute ──────────────────────────────────────────────

    GPUComputePipelineHandle SDLGPUDevice::CreateComputePipeline(const ComputePipelineCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateComputePipeline: No GPU device");
            return GPUComputePipelineHandle::Invalid();
        }

        SDL_GPUComputePipelineCreateInfo info{};
        info.code = desc.Code;
        info.code_size = desc.CodeSize;
        info.entrypoint = desc.EntryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateComputePipeline: Device does not support SPIR-V");
            return GPUComputePipelineHandle::Invalid();
        }
        info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
        info.num_samplers = desc.Samplers;
        info.num_readonly_storage_textures = desc.ReadOnlyStorageTextures;
        info.num_readonly_storage_buffers = desc.ReadOnlyStorageBuffers;
        info.num_readwrite_storage_textures = desc.ReadWriteStorageTextures;
        info.num_readwrite_storage_buffers = desc.ReadWriteStorageBuffers;
        info.num_uniform_buffers = desc.UniformBuffers;
        info.threadcount_x = desc.ThreadCountX;
        info.threadcount_y = desc.ThreadCountY;
        info.threadcount_z = desc.ThreadCountZ;

        SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(m_device, &info);
        if (!pipeline)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateComputePipeline: Failed — {}", SDL_GetError());
            return GPUComputePipelineHandle::Invalid();
        }

        return m_computePipelinePool.Acquire(pipeline);
    }

    void SDLGPUDevice::DestroyComputePipeline(GPUComputePipelineHandle pipeline)
    {
        if (!m_device)
        {
            return;
        }
        auto* pPipeline = m_computePipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_ReleaseGPUComputePipeline(m_device, *pPipeline);
            m_computePipelinePool.Release(pipeline);
        }
    }

    void SDLGPUDevice::BeginComputePass()
    {
        if (!m_commandBuffer)
        {
            return;
        }

        m_computePass = SDL_BeginGPUComputePass(m_commandBuffer, nullptr, 0, nullptr, 0);
    }

    void SDLGPUDevice::EndComputePass()
    {
        if (m_computePass)
        {
            SDL_EndGPUComputePass(m_computePass);
            m_computePass = nullptr;
        }
    }

    void SDLGPUDevice::BindComputePipeline(GPUComputePipelineHandle pipeline)
    {
        if (!m_computePass)
        {
            return;
        }
        auto* pPipeline = m_computePipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_BindGPUComputePipeline(m_computePass, *pPipeline);
        }
    }

    void SDLGPUDevice::DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        if (m_computePass)
        {
            SDL_DispatchGPUCompute(m_computePass, groupCountX, groupCountY, groupCountZ);
        }
    }

    // ── Textures ─────────────────────────────────────────────

    GPUTextureHandle SDLGPUDevice::CreateTexture(const TextureCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateTexture: No GPU device");
            return GPUTextureHandle::Invalid();
        }

        const uint32_t maxMips = CalculateMipLevels(desc.Width, desc.Height);
        const uint32_t resolvedMips = (desc.MipLevels == 0) ? maxMips : std::min(desc.MipLevels, maxMips);

        if (desc.MipLevels > maxMips)
        {
            WAYFINDER_WARN(LogRenderer,
                "SDLGPUDevice::CreateTexture: Requested {} mip levels for {}x{} texture, "
                "clamped to maximum of {}",
                desc.MipLevels, desc.Width, desc.Height, maxMips);
        }

        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = ToSDLTextureFormat(desc.Format);
        info.width = desc.Width;
        info.height = desc.Height;
        info.layer_count_or_depth = 1;
        info.num_levels = resolvedMips;

        auto usage = desc.Usage;
        if (resolvedMips > 1 && !HasFlag(usage, TextureUsage::DepthTarget))
        {
            /// Blit-based mip generation requires ColourTarget usage.
            usage = usage | TextureUsage::ColourTarget;
        }
        info.usage = ToSDLTextureUsage(usage);

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &info);
        if (!texture)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateTexture: Failed ({}x{}, {} mips) — {}", desc.Width, desc.Height, resolvedMips, SDL_GetError());
            return GPUTextureHandle::Invalid();
        }

        return m_texturePool.Acquire({.texture = texture, .width = desc.Width, .height = desc.Height, .mipLevels = resolvedMips});
    }

    void SDLGPUDevice::DestroyTexture(GPUTextureHandle texture)
    {
        if (!m_device)
        {
            return;
        }
        auto* pEntry = m_texturePool.Get(texture);
        if (pEntry)
        {
            SDL_ReleaseGPUTexture(m_device, pEntry->texture);
            m_texturePool.Release(texture);
        }
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void SDLGPUDevice::UploadToTexture(GPUTextureHandle texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel)
    {
        auto* pEntry = m_texturePool.Get(texture);
        if (!m_device || !pEntry || !pixelData || width == 0 || height == 0)
        {
            return;
        }

        const uint32_t totalBytes = bytesPerRow * height;

        // Try to stage into the ring buffer
        if (const auto offset = TryStageToRing(pixelData, totalBytes))
        {
            m_pendingTextureCopies.push_back({
                .ringOffset = *offset,
                .size = totalBytes,
                .dstTexture = pEntry->texture,
                .width = width,
                .height = height,
                .pixelsPerRow = width,
                .rowsPerLayer = height,
                .mipLevel = mipLevel,
            });
            return;
        }

        // Oversized or no ring — use dedicated transfer
        UploadToTextureDedicated(pEntry->texture, pixelData, width, height, bytesPerRow, mipLevel);
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void SDLGPUDevice::UploadToTextureDedicated(SDL_GPUTexture* texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel)
    {
        const uint32_t totalBytes = bytesPerRow * height;

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = totalBytes;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
        if (!transferBuffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToTexture: Failed to create transfer buffer — {}", SDL_GetError());
            return;
        }

        void* mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
        if (!mapped)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToTexture: Failed to map transfer buffer — {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
            return;
        }

        std::memcpy(mapped, pixelData, totalBytes);
        SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
        if (!cmdBuf)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToTexture: Failed to acquire command buffer — {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
            return;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = transferBuffer;
        src.offset = 0;
        src.pixels_per_row = width;
        src.rows_per_layer = height;

        SDL_GPUTextureRegion dst{};
        dst.texture = texture;
        dst.mip_level = mipLevel;
        dst.w = width;
        dst.h = height;
        dst.d = 1;

        SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
        SDL_EndGPUCopyPass(copyPass);

        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    }

    void SDLGPUDevice::FlushUploads()
    {
        if (!m_device || (m_pendingBufferCopies.empty() && m_pendingTextureCopies.empty()))
        {
            return;
        }

        // Unmap before issuing the copy pass
        if (m_stagingMapped)
        {
            SDL_UnmapGPUTransferBuffer(m_device, m_stagingRing);
            m_stagingMapped = nullptr;
        }

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
        if (!cmdBuf)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::FlushUploads: Failed to acquire command buffer — {}", SDL_GetError());
            m_pendingBufferCopies.clear();
            m_pendingTextureCopies.clear();
            return;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        for (const auto& copy : m_pendingBufferCopies)
        {
            SDL_GPUTransferBufferLocation src{};
            src.transfer_buffer = m_stagingRing;
            src.offset = copy.ringOffset;

            SDL_GPUBufferRegion dst{};
            dst.buffer = copy.dstBuffer;
            dst.offset = copy.dstOffset;
            dst.size = copy.size;

            SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
        }

        for (const auto& copy : m_pendingTextureCopies)
        {
            SDL_GPUTextureTransferInfo src{};
            src.transfer_buffer = m_stagingRing;
            src.offset = copy.ringOffset;
            src.pixels_per_row = copy.pixelsPerRow;
            src.rows_per_layer = copy.rowsPerLayer;

            SDL_GPUTextureRegion dst{};
            dst.texture = copy.dstTexture;
            dst.mip_level = copy.mipLevel;
            dst.w = copy.width;
            dst.h = copy.height;
            dst.d = 1;

            SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
        }

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmdBuf);

        m_pendingBufferCopies.clear();
        m_pendingTextureCopies.clear();

        // Re-map for any subsequent uploads this frame.
        // cycle=true tells SDL to internally provide a fresh backing allocation so
        // we don't stall on the copy pass we just submitted. If a driver's cycle
        // implementation doesn't actually double-buffer, this could cause a GPU
        // read-after-write hazard.
        /// @todo Confirm SDL_GPU cycle=true guarantees a new backing allocation
        /// for transfer buffers on all target backends (Vulkan, D3D12, Metal).
        if (m_stagingRing)
        {
            m_stagingMapped = SDL_MapGPUTransferBuffer(m_device, m_stagingRing, true);
            m_stagingCursor = 0;
        }
    }

    void SDLGPUDevice::GenerateMipmaps(GPUTextureHandle texture)
    {
        auto* pEntry = m_texturePool.Get(texture);
        if (!m_device || !pEntry || pEntry->mipLevels <= 1)
        {
            return;
        }

        // Flush pending uploads so texture data is committed before mip generation
        FlushUploads();

        if (m_mipGenerator)
        {
            m_mipGenerator->Generate(m_device, pEntry->texture, pEntry->mipLevels, pEntry->width, pEntry->height);
        }
    }

    // ── Samplers ─────────────────────────────────────────────

    GPUSamplerHandle SDLGPUDevice::CreateSampler(const SamplerCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateSampler: No GPU device");
            return GPUSamplerHandle::Invalid();
        }

        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = (desc.MinFilter == SamplerFilter::Nearest) ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        info.mag_filter = (desc.MagFilter == SamplerFilter::Nearest) ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = (desc.MipmapMode == SamplerMipmapMode::Linear) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.min_lod = desc.MinLod;
        info.max_lod = desc.MaxLod;
        info.mip_lod_bias = desc.MipLodBias;
        info.enable_anisotropy = desc.EnableAnisotropy;
        info.max_anisotropy = desc.MaxAnisotropy;

        auto toAddressMode = [](SamplerAddressMode mode) -> SDL_GPUSamplerAddressMode
        {
            switch (mode)
            {
            case SamplerAddressMode::Repeat:
                return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            case SamplerAddressMode::ClampToEdge:
                return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            case SamplerAddressMode::MirroredRepeat:
                return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            }
            return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        };

        info.address_mode_u = toAddressMode(desc.AddressModeU);
        info.address_mode_v = toAddressMode(desc.AddressModeV);
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        SDL_GPUSampler* sampler = SDL_CreateGPUSampler(m_device, &info);
        if (!sampler)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateSampler: Failed — {}", SDL_GetError());
            return GPUSamplerHandle::Invalid();
        }

        return m_samplerPool.Acquire(sampler);
    }

    void SDLGPUDevice::DestroySampler(GPUSamplerHandle sampler)
    {
        if (!m_device)
        {
            return;
        }
        auto* pSampler = m_samplerPool.Get(sampler);
        if (pSampler)
        {
            SDL_ReleaseGPUSampler(m_device, *pSampler);
            m_samplerPool.Release(sampler);
        }
    }

    void SDLGPUDevice::BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler)
    {
        if (!m_renderPass)
        {
            return;
        }
        auto* pEntry = m_texturePool.Get(texture);
        auto* pSampler = m_samplerPool.Get(sampler);
        if (!pEntry || !pSampler)
        {
            return;
        }

        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = pEntry->texture;
        binding.sampler = *pSampler;

        SDL_BindGPUFragmentSamplers(m_renderPass, slot, &binding, 1);
    }

    Extent2D SDLGPUDevice::GetSwapchainDimensions() const
    {
        return {.Width = m_swapchainWidth, .Height = m_swapchainHeight};
    }

} // namespace Wayfinder
