#include "SDLRenderDeviceSubsystem.h"

#include "SDLWindowSubsystem.h"
#include "app/EngineContext.h"
#include "core/Log.h"
#include "rendering/backend/sdl_gpu/BlitMipGenerator.h"
#include "rendering/backend/sdl_gpu/MipGenerator.h"

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
        auto ToSDLVertexFormat(VertexAttributeFormat fmt) -> SDL_GPUVertexElementFormat
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

        auto ToSDLBlendFactor(BlendFactor f) -> SDL_GPUBlendFactor
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

        auto ToSDLBlendOp(BlendOp op) -> SDL_GPUBlendOp
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

        auto ToSDLTextureFormat(TextureFormat format) -> SDL_GPUTextureFormat
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

        auto ToSDLTextureUsage(TextureUsage usage) -> SDL_GPUTextureUsageFlags
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

    // ── Lifecycle ────────────────────────────────────────────

    SDLRenderDeviceSubsystem::SDLRenderDeviceSubsystem() = default;

    SDLRenderDeviceSubsystem::~SDLRenderDeviceSubsystem() noexcept
    {
        try
        {
            Shutdown();
        }
        catch (const std::exception& ex)
        {
            Log::Warn(LogRenderer, "SDLRenderDeviceSubsystem: suppressed exception during Shutdown(): {}", ex.what());
        }
        catch (...)
        {
            Log::Warn(LogRenderer, "SDLRenderDeviceSubsystem: suppressed unknown exception during Shutdown().");
        }
    }

    auto SDLRenderDeviceSubsystem::Initialise(EngineContext& context) -> Result<void>
    {
        Log::Info(LogRenderer, "SDLRenderDeviceSubsystem: Initialising");

        auto& windowSubsystem = context.GetAppSubsystem<SDLWindowSubsystem>();
        m_window = windowSubsystem.GetNativeWindow();
        if (!m_window)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: SDLWindowSubsystem has no valid native window");
            return MakeError("SDLRenderDeviceSubsystem: SDLWindowSubsystem has no valid native window");
        }

        m_gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,
            true, // debug mode
            nullptr);

        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: Failed to create GPU device -- {}", SDL_GetError());
            return MakeError(std::format("SDLRenderDeviceSubsystem: Failed to create GPU device -- {}", SDL_GetError()));
        }

        m_shaderFormats = SDL_GetGPUShaderFormats(m_gpuDevice);

        if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice, m_window))
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: Failed to claim window for GPU device -- {}", SDL_GetError());
            auto error = MakeError(std::format("SDLRenderDeviceSubsystem: Failed to claim window -- {}", SDL_GetError()));
            SDL_DestroyGPUDevice(m_gpuDevice);
            m_gpuDevice = nullptr;
            return error;
        }

        m_info.BackendName = "SDL_GPU";

        const char* driver = SDL_GetGPUDeviceDriver(m_gpuDevice);
        m_info.DriverInfo = driver ? driver : "unknown";

        Log::Info(LogRenderer, "SDLRenderDeviceSubsystem: Initialised (driver: {})", m_info.DriverInfo);

        m_mipGenerator = std::make_unique<BlitMipGenerator>();

        // Create persistent staging ring for batched uploads
        SDL_GPUTransferBufferCreateInfo stagingInfo{};
        stagingInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        stagingInfo.size = STAGING_RING_CAPACITY;

        m_stagingRing = SDL_CreateGPUTransferBuffer(m_gpuDevice, &stagingInfo);
        if (!m_stagingRing)
        {
            Log::Warn(LogRenderer, "SDLRenderDeviceSubsystem: Failed to create staging ring -- uploads will use dedicated buffers");
        }

        return {};
    }

    void SDLRenderDeviceSubsystem::Shutdown()
    {
        if (m_gpuDevice)
        {
            Log::Info(LogRenderer, "SDLRenderDeviceSubsystem: Shutting down");

            m_shaderPool.ForEachAlive([&](SDL_GPUShader* s)
            {
                SDL_ReleaseGPUShader(m_gpuDevice, s);
            });
            m_shaderPool.Clear();

            m_pipelinePool.ForEachAlive([&](SDL_GPUGraphicsPipeline* p)
            {
                SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, p);
            });
            m_pipelinePool.Clear();

            m_computePipelinePool.ForEachAlive([&](SDL_GPUComputePipeline* p)
            {
                SDL_ReleaseGPUComputePipeline(m_gpuDevice, p);
            });
            m_computePipelinePool.Clear();

            m_bufferPool.ForEachAlive([&](SDL_GPUBuffer* b)
            {
                SDL_ReleaseGPUBuffer(m_gpuDevice, b);
            });
            m_bufferPool.Clear();

            m_samplerPool.ForEachAlive([&](SDL_GPUSampler* s)
            {
                SDL_ReleaseGPUSampler(m_gpuDevice, s);
            });
            m_samplerPool.Clear();

            m_texturePool.ForEachAlive([&](SDLTextureEntry& entry)
            {
                SDL_ReleaseGPUTexture(m_gpuDevice, entry.Texture);
            });
            m_texturePool.Clear();

            if (m_stagingRing)
            {
                if (m_stagingMapped)
                {
                    SDL_UnmapGPUTransferBuffer(m_gpuDevice, m_stagingRing);
                    m_stagingMapped = nullptr;
                }
                SDL_ReleaseGPUTransferBuffer(m_gpuDevice, m_stagingRing);
                m_stagingRing = nullptr;
                m_stagingCursor = 0;
            }
            m_pendingBufferCopies.clear();
            m_pendingTextureCopies.clear();

            if (m_depthTexture)
            {
                SDL_ReleaseGPUTexture(m_gpuDevice, m_depthTexture);
                m_depthTexture = nullptr;
            }

            if (m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_gpuDevice, m_window);
            }

            SDL_DestroyGPUDevice(m_gpuDevice);
            m_gpuDevice = nullptr;
            m_window = nullptr;
        }
    }

    // ── Frame Lifecycle ──────────────────────────────────────

    auto SDLRenderDeviceSubsystem::BeginFrame() -> bool
    {
        m_commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        if (!m_commandBuffer)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: Failed to acquire command buffer -- {}", SDL_GetError());
            return false;
        }

        if (m_stagingRing && !m_stagingMapped)
        {
            m_stagingMapped = SDL_MapGPUTransferBuffer(m_gpuDevice, m_stagingRing, true);
            m_stagingCursor = 0;
        }

        if (!SDL_AcquireGPUSwapchainTexture(m_commandBuffer, m_window, &m_swapchainTexture, &m_swapchainWidth, &m_swapchainHeight))
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: Failed to acquire swapchain texture -- {}", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        if (!m_swapchainTexture)
        {
            // Window is minimised or occluded
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        EnsureDepthTexture(m_swapchainWidth, m_swapchainHeight);

        return true;
    }

    void SDLRenderDeviceSubsystem::EndFrame()
    {
        FlushUploads();

        if (m_stagingMapped)
        {
            SDL_UnmapGPUTransferBuffer(m_gpuDevice, m_stagingRing);
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

    void SDLRenderDeviceSubsystem::PushDebugGroup(std::string_view name)
    {
        if (!m_commandBuffer)
        {
            return;
        }

        const std::string label = name.empty() ? "Unnamed GPU Scope" : std::string(name);
        SDL_PushGPUDebugGroup(m_commandBuffer, label.c_str());
    }

    void SDLRenderDeviceSubsystem::PopDebugGroup()
    {
        if (m_commandBuffer)
        {
            SDL_PopGPUDebugGroup(m_commandBuffer);
        }
    }

    void SDLRenderDeviceSubsystem::EnsureDepthTexture(uint32_t width, uint32_t height)
    {
        if (m_depthTexture && m_depthWidth == width && m_depthHeight == height)
        {
            return;
        }

        if (m_depthTexture)
        {
            SDL_ReleaseGPUTexture(m_gpuDevice, m_depthTexture);
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

        m_depthTexture = SDL_CreateGPUTexture(m_gpuDevice, &info);
        if (!m_depthTexture)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem: Failed to create depth texture ({}x{}) -- {}", width, height, SDL_GetError());
            return;
        }

        m_depthWidth = width;
        m_depthHeight = height;
    }

    // ── Render Pass ──────────────────────────────────────────

    auto SDLRenderDeviceSubsystem::BeginRenderPass(const RenderPassDescriptor& descriptor) -> bool
    {
        if (!m_commandBuffer)
        {
            return false;
        }

        if (descriptor.ColourTargetCount > MAX_COLOUR_TARGETS)
        {
            Log::Error(LogRenderer, "BeginRenderPass '{}': ColourTargetCount {} exceeds maximum of {}", descriptor.DebugName, descriptor.ColourTargetCount, MAX_COLOUR_TARGETS);
            return false;
        }

        const uint32_t numTargets = descriptor.ColourTargetCount;
        if (numTargets == 0 && !descriptor.DepthAttachment.Enabled)
        {
            Log::Error(LogRenderer, "BeginRenderPass '{}': no colour targets and no depth attachment - skipping pass", descriptor.DebugName);
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
                texture = pTex ? pTex->Texture : nullptr;
            }

            if (!texture)
            {
                Log::Warn(LogRenderer, "BeginRenderPass '{}': colour target at slot {} is null -- skipping pass", descriptor.DebugName, i);
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
                depthTexture = pTex ? pTex->Texture : nullptr;
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
                Log::Error(LogRenderer, "BeginRenderPass '{}': depth attachment enabled but no depth texture available (depthTarget={}, m_depthTexture={})", descriptor.DebugName, descriptor.DepthTarget.IsValid(),
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

    void SDLRenderDeviceSubsystem::EndRenderPass()
    {
        if (m_renderPass)
        {
            SDL_EndGPURenderPass(m_renderPass);
            m_renderPass = nullptr;
        }
    }

    // ── Shader and Pipeline ──────────────────────────────────

    auto SDLRenderDeviceSubsystem::CreateShader(const ShaderCreateDesc& desc) -> GPUShaderHandle
    {
        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateShader: No GPU device");
            return GPUShaderHandle::Invalid();
        }

        SDL_GPUShaderCreateInfo info{};
        info.code = desc.Code;
        info.code_size = desc.CodeSize;
        info.entrypoint = desc.EntryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateShader: Device does not support SPIR-V");
            return GPUShaderHandle::Invalid();
        }
        info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
        info.stage = (desc.Stage == ShaderStage::Vertex) ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = desc.Samplers;
        info.num_storage_textures = desc.StorageTextures;
        info.num_storage_buffers = desc.StorageBuffers;
        info.num_uniform_buffers = desc.UniformBuffers;

        SDL_GPUShader* shader = SDL_CreateGPUShader(m_gpuDevice, &info);
        if (!shader)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateShader: Failed -- {}", SDL_GetError());
            return GPUShaderHandle::Invalid();
        }

        return m_shaderPool.Acquire(shader);
    }

    void SDLRenderDeviceSubsystem::DestroyShader(GPUShaderHandle shader)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pShader = m_shaderPool.Get(shader);
        if (pShader)
        {
            SDL_ReleaseGPUShader(m_gpuDevice, *pShader);
            m_shaderPool.Release(shader);
        }
    }

    auto SDLRenderDeviceSubsystem::CreatePipeline(const PipelineCreateDesc& desc) -> GPUPipelineHandle
    {
        if (!m_gpuDevice || !m_window)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreatePipeline: No GPU device or window");
            return GPUPipelineHandle::Invalid();
        }

        if (desc.ColourTargetCount == 0 || desc.ColourTargetCount > MAX_COLOUR_TARGETS)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreatePipeline: ColourTargetCount={} is out of range [1, {}]", desc.ColourTargetCount, MAX_COLOUR_TARGETS);
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
        for (const BlendState& blend : desc.ColourTargetBlends | std::views::take(desc.ColourTargetCount))
        {
            if (!blend.Enabled && blend.ColourWriteMask == 0)
            {
                Log::Warn(LogRenderer, "CreatePipeline: colour target {} has blending disabled and ColourWriteMask=0 -- nothing will be written", colourTargetIndex);
            }
            if (blend.Enabled && blend.ColourWriteMask == 0)
            {
                Log::Warn(LogRenderer, "CreatePipeline: colour target {} has blending enabled but ColourWriteMask=0 -- blended result will be discarded", colourTargetIndex);
            }
            ++colourTargetIndex;
        }

        const SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(m_gpuDevice, m_window);
        std::array<SDL_GPUColorTargetDescription, MAX_COLOUR_TARGETS> colourTargetDescs{};
        auto targetSlices = std::views::zip(colourTargetDescs, desc.ColourTargetFormats, desc.ColourTargetBlends) | std::views::take(desc.ColourTargetCount);
        for (auto&& [colourTargetDesc, format, blend] : targetSlices)
        {
            colourTargetDesc.format = (format == TextureFormat::SwapchainFormat) ? swapchainFormat : ToSDLTextureFormat(format);
            colourTargetDesc.blend_state.color_write_mask = blend.ColourWriteMask;

            if (blend.Enabled)
            {
                colourTargetDesc.blend_state.enable_blend = true;
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
        targetInfo.num_color_targets = desc.ColourTargetCount;
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
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreatePipeline: Invalid shader handle(s)");
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

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
        if (!pipeline)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreatePipeline: Failed -- {}", SDL_GetError());
            return GPUPipelineHandle::Invalid();
        }

        return m_pipelinePool.Acquire(pipeline);
    }

    void SDLRenderDeviceSubsystem::DestroyPipeline(GPUPipelineHandle pipeline)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pPipeline = m_pipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, *pPipeline);
            m_pipelinePool.Release(pipeline);
        }
    }

    void SDLRenderDeviceSubsystem::BindPipeline(GPUPipelineHandle pipeline)
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

    auto SDLRenderDeviceSubsystem::CreateBuffer(const BufferCreateDesc& desc) -> GPUBufferHandle
    {
        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateBuffer: No GPU device");
            return GPUBufferHandle::Invalid();
        }

        SDL_GPUBufferCreateInfo info{};
        info.usage = (desc.Usage == BufferUsage::Vertex) ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX;
        info.size = desc.SizeInBytes;

        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(m_gpuDevice, &info);
        if (!buffer)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateBuffer: Failed -- {}", SDL_GetError());
            return GPUBufferHandle::Invalid();
        }

        return m_bufferPool.Acquire(buffer);
    }

    void SDLRenderDeviceSubsystem::DestroyBuffer(GPUBufferHandle buffer)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (pBuffer)
        {
            SDL_ReleaseGPUBuffer(m_gpuDevice, *pBuffer);
            m_bufferPool.Release(buffer);
        }
    }

    auto SDLRenderDeviceSubsystem::TryStageToRing(const void* data, uint32_t sizeInBytes) -> std::optional<uint32_t>
    {
        if (!m_stagingMapped || sizeInBytes > STAGING_RING_CAPACITY)
        {
            return std::nullopt;
        }

        auto tryAppend = [&]() -> std::optional<uint32_t>
        {
            if (!m_stagingMapped)
            {
                return std::nullopt;
            }
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

        // Ring is full - flush what we have and try once more
        FlushUploads();
        return tryAppend();
    }

    void SDLRenderDeviceSubsystem::UploadToBuffer(GPUBufferHandle buffer, const void* data, BufferUploadRegion region)
    {
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!m_gpuDevice || !pBuffer || !data || region.SizeInBytes == 0)
        {
            return;
        }

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

        UploadToBufferDedicated(*pBuffer, data, region);
    }

    void SDLRenderDeviceSubsystem::UploadToBufferDedicated(SDL_GPUBuffer* buffer, const void* data, BufferUploadRegion region)
    {
        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = region.SizeInBytes;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &transferInfo);
        if (!transferBuffer)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToBuffer: Failed to create transfer buffer -- {}", SDL_GetError());
            return;
        }

        void* mapped = SDL_MapGPUTransferBuffer(m_gpuDevice, transferBuffer, false);
        if (!mapped)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToBuffer: Failed to map transfer buffer -- {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
            return;
        }

        std::memcpy(mapped, data, region.SizeInBytes);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice, transferBuffer);

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        if (!cmdBuf)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToBuffer: Failed to acquire command buffer -- {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
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
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
    }

    // ── Draw Commands ────────────────────────────────────────

    void SDLRenderDeviceSubsystem::BindVertexBuffer(GPUBufferHandle buffer, VertexBufferBindingDesc bindingDesc)
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

    void SDLRenderDeviceSubsystem::BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes)
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

    void SDLRenderDeviceSubsystem::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        if (!m_renderPass)
        {
            return;
        }

        SDL_DrawGPUIndexedPrimitives(m_renderPass, indexCount, instanceCount, firstIndex, vertexOffset, 0);
    }

    void SDLRenderDeviceSubsystem::DrawPrimitives(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        if (!m_renderPass)
        {
            return;
        }

        SDL_DrawGPUPrimitives(m_renderPass, vertexCount, instanceCount, firstVertex, 0);
    }

    void SDLRenderDeviceSubsystem::PushVertexUniform(uint32_t slot, const void* data, uint32_t sizeInBytes)
    {
        if (!m_commandBuffer || !data || sizeInBytes == 0)
        {
            return;
        }

        SDL_PushGPUVertexUniformData(m_commandBuffer, slot, data, sizeInBytes);
    }

    void SDLRenderDeviceSubsystem::PushFragmentUniform(uint32_t slot, const void* data, uint32_t sizeInBytes)
    {
        if (!m_commandBuffer || !data || sizeInBytes == 0)
        {
            return;
        }

        SDL_PushGPUFragmentUniformData(m_commandBuffer, slot, data, sizeInBytes);
    }

    // ── Compute ──────────────────────────────────────────────

    auto SDLRenderDeviceSubsystem::CreateComputePipeline(const ComputePipelineCreateDesc& desc) -> GPUComputePipelineHandle
    {
        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateComputePipeline: No GPU device");
            return GPUComputePipelineHandle::Invalid();
        }

        SDL_GPUComputePipelineCreateInfo info{};
        info.code = desc.Code;
        info.code_size = desc.CodeSize;
        info.entrypoint = desc.EntryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateComputePipeline: Device does not support SPIR-V");
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

        SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(m_gpuDevice, &info);
        if (!pipeline)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateComputePipeline: Failed -- {}", SDL_GetError());
            return GPUComputePipelineHandle::Invalid();
        }

        return m_computePipelinePool.Acquire(pipeline);
    }

    void SDLRenderDeviceSubsystem::DestroyComputePipeline(GPUComputePipelineHandle pipeline)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pPipeline = m_computePipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_ReleaseGPUComputePipeline(m_gpuDevice, *pPipeline);
            m_computePipelinePool.Release(pipeline);
        }
    }

    void SDLRenderDeviceSubsystem::BeginComputePass()
    {
        if (!m_commandBuffer)
        {
            return;
        }

        m_computePass = SDL_BeginGPUComputePass(m_commandBuffer, nullptr, 0, nullptr, 0);
    }

    void SDLRenderDeviceSubsystem::EndComputePass()
    {
        if (m_computePass)
        {
            SDL_EndGPUComputePass(m_computePass);
            m_computePass = nullptr;
        }
    }

    void SDLRenderDeviceSubsystem::BindComputePipeline(GPUComputePipelineHandle pipeline)
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

    void SDLRenderDeviceSubsystem::DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        if (m_computePass)
        {
            SDL_DispatchGPUCompute(m_computePass, groupCountX, groupCountY, groupCountZ);
        }
    }

    // ── Textures ─────────────────────────────────────────────

    auto SDLRenderDeviceSubsystem::CreateTexture(const TextureCreateDesc& desc) -> GPUTextureHandle
    {
        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateTexture: No GPU device");
            return GPUTextureHandle::Invalid();
        }

        const uint32_t maxMips = CalculateMipLevels(desc.Width, desc.Height);
        const uint32_t resolvedMips = (desc.MipLevels == 0) ? maxMips : std::min(desc.MipLevels, maxMips);

        if (desc.MipLevels > maxMips)
        {
            Log::Warn(LogRenderer,
                "SDLRenderDeviceSubsystem::CreateTexture: Requested {} mip levels for {}x{} texture, "
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
            usage = usage | TextureUsage::ColourTarget;
        }
        info.usage = ToSDLTextureUsage(usage);

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_gpuDevice, &info);
        if (!texture)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateTexture: Failed ({}x{}, {} mips) -- {}", desc.Width, desc.Height, resolvedMips, SDL_GetError());
            return GPUTextureHandle::Invalid();
        }

        return m_texturePool.Acquire({.Texture = texture, .Width = desc.Width, .Height = desc.Height, .MipLevels = resolvedMips});
    }

    void SDLRenderDeviceSubsystem::DestroyTexture(GPUTextureHandle texture)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pEntry = m_texturePool.Get(texture);
        if (pEntry)
        {
            SDL_ReleaseGPUTexture(m_gpuDevice, pEntry->Texture);
            m_texturePool.Release(texture);
        }
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void SDLRenderDeviceSubsystem::UploadToTexture(GPUTextureHandle texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel)
    {
        auto* pEntry = m_texturePool.Get(texture);
        if (!m_gpuDevice || !pEntry || !pixelData || width == 0 || height == 0)
        {
            return;
        }

        const uint32_t totalBytes = bytesPerRow * height;

        if (const auto offset = TryStageToRing(pixelData, totalBytes))
        {
            m_pendingTextureCopies.push_back({
                .ringOffset = *offset,
                .size = totalBytes,
                .dstTexture = pEntry->Texture,
                .width = width,
                .height = height,
                .pixelsPerRow = width,
                .rowsPerLayer = height,
                .mipLevel = mipLevel,
            });
            return;
        }

        UploadToTextureDedicated(pEntry->Texture, pixelData, width, height, bytesPerRow, mipLevel);
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void SDLRenderDeviceSubsystem::UploadToTextureDedicated(SDL_GPUTexture* texture, const void* pixelData, uint32_t width, uint32_t height, uint32_t bytesPerRow, uint32_t mipLevel)
    {
        const uint32_t totalBytes = bytesPerRow * height;

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = totalBytes;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &transferInfo);
        if (!transferBuffer)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToTexture: Failed to create transfer buffer -- {}", SDL_GetError());
            return;
        }

        void* mapped = SDL_MapGPUTransferBuffer(m_gpuDevice, transferBuffer, false);
        if (!mapped)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToTexture: Failed to map transfer buffer -- {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
            return;
        }

        std::memcpy(mapped, pixelData, totalBytes);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice, transferBuffer);

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        if (!cmdBuf)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::UploadToTexture: Failed to acquire command buffer -- {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
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
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
    }

    void SDLRenderDeviceSubsystem::FlushUploads()
    {
        if (!m_gpuDevice || (m_pendingBufferCopies.empty() && m_pendingTextureCopies.empty()))
        {
            return;
        }

        if (m_stagingMapped)
        {
            SDL_UnmapGPUTransferBuffer(m_gpuDevice, m_stagingRing);
            m_stagingMapped = nullptr;
        }

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        if (!cmdBuf)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::FlushUploads: Failed to acquire command buffer -- {}", SDL_GetError());
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

        if (m_stagingRing)
        {
            m_stagingMapped = SDL_MapGPUTransferBuffer(m_gpuDevice, m_stagingRing, true);
            m_stagingCursor = 0;
        }
    }

    void SDLRenderDeviceSubsystem::GenerateMipmaps(GPUTextureHandle texture)
    {
        auto* pEntry = m_texturePool.Get(texture);
        if (!m_gpuDevice || !pEntry || pEntry->MipLevels <= 1)
        {
            return;
        }

        FlushUploads();

        if (m_mipGenerator)
        {
            m_mipGenerator->Generate(m_gpuDevice, pEntry->Texture, pEntry->MipLevels, pEntry->Width, pEntry->Height);
        }
    }

    // ── Samplers ─────────────────────────────────────────────

    auto SDLRenderDeviceSubsystem::CreateSampler(const SamplerCreateDesc& desc) -> GPUSamplerHandle
    {
        if (!m_gpuDevice)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateSampler: No GPU device");
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

        SDL_GPUSampler* sampler = SDL_CreateGPUSampler(m_gpuDevice, &info);
        if (!sampler)
        {
            Log::Error(LogRenderer, "SDLRenderDeviceSubsystem::CreateSampler: Failed -- {}", SDL_GetError());
            return GPUSamplerHandle::Invalid();
        }

        return m_samplerPool.Acquire(sampler);
    }

    void SDLRenderDeviceSubsystem::DestroySampler(GPUSamplerHandle sampler)
    {
        if (!m_gpuDevice)
        {
            return;
        }
        auto* pSampler = m_samplerPool.Get(sampler);
        if (pSampler)
        {
            SDL_ReleaseGPUSampler(m_gpuDevice, *pSampler);
            m_samplerPool.Release(sampler);
        }
    }

    void SDLRenderDeviceSubsystem::BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler)
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
        binding.texture = pEntry->Texture;
        binding.sampler = *pSampler;

        SDL_BindGPUFragmentSamplers(m_renderPass, slot, &binding, 1);
    }

    auto SDLRenderDeviceSubsystem::GetSwapchainDimensions() const -> Extent2D
    {
        return {.Width = m_swapchainWidth, .Height = m_swapchainHeight};
    }

} // namespace Wayfinder
