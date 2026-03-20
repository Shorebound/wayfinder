#include "SDLGPUDevice.h"
#include "../null/NullDevice.h"
#include "../../platform/Window.h"

#include "../../core/Log.h"

#include <SDL3/SDL.h>
#include <vector>

namespace Wayfinder
{
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

    SDLGPUDevice::~SDLGPUDevice()
    {
        Shutdown();
    }

    bool SDLGPUDevice::Initialise(Window& window)
    {
        m_window = static_cast<SDL_Window*>(window.GetNativeHandle());
        if (!m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Window has no valid native handle");
            return false;
        }

        // Only request formats we can actually provide.
        // Currently all shaders are compiled to SPIR-V.
        m_device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV,
            true, // debug mode
            nullptr);

        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to create GPU device — {}", SDL_GetError());
            return false;
        }

        m_shaderFormats = SDL_GetGPUShaderFormats(m_device);

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to claim window for GPU device — {}", SDL_GetError());
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }

        m_info.BackendName = "SDL_GPU";

        const char* driver = SDL_GetGPUDeviceDriver(m_device);
        m_info.DriverInfo = driver ? driver : "unknown";

        WAYFINDER_INFO(LogRenderer, "SDLGPUDevice: Initialised (driver: {})", m_info.DriverInfo);
        return true;
    }

    void SDLGPUDevice::Shutdown()
    {
        if (m_device)
        {
            // Release all pooled GPU resources before destroying the device.
            m_shaderPool.ForEachAlive([&](SDL_GPUShader* s) { SDL_ReleaseGPUShader(m_device, s); });
            m_shaderPool.Clear();

            m_pipelinePool.ForEachAlive([&](SDL_GPUGraphicsPipeline* p) { SDL_ReleaseGPUGraphicsPipeline(m_device, p); });
            m_pipelinePool.Clear();

            m_computePipelinePool.ForEachAlive([&](SDL_GPUComputePipeline* p) { SDL_ReleaseGPUComputePipeline(m_device, p); });
            m_computePipelinePool.Clear();

            m_bufferPool.ForEachAlive([&](SDL_GPUBuffer* b) { SDL_ReleaseGPUBuffer(m_device, b); });
            m_bufferPool.Clear();

            m_samplerPool.ForEachAlive([&](SDL_GPUSampler* s) { SDL_ReleaseGPUSampler(m_device, s); });
            m_samplerPool.Clear();

            m_texturePool.ForEachAlive([&](SDL_GPUTexture* t) { SDL_ReleaseGPUTexture(m_device, t); });
            m_texturePool.Clear();

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
        if (m_commandBuffer)
        {
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
        }

        m_swapchainTexture = nullptr;
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

    void SDLGPUDevice::BeginRenderPass(const RenderPassDescriptor& descriptor)
    {
        if (!m_commandBuffer)
        {
            return;
        }

        // Determine colour target texture
        SDL_GPUTexture* colorTexture = nullptr;
        if (descriptor.targetSwapchain)
        {
            if (!m_swapchainTexture) return;
            colorTexture = m_swapchainTexture;
        }
        else if (descriptor.colorTarget.IsValid())
        {
            auto* pTex = m_texturePool.Get(descriptor.colorTarget);
            colorTexture = pTex ? *pTex : nullptr;
        }

        if (!colorTexture) return;

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = colorTexture;
        colorTarget.clear_color.r = descriptor.colorAttachment.clearValue.r;
        colorTarget.clear_color.g = descriptor.colorAttachment.clearValue.g;
        colorTarget.clear_color.b = descriptor.colorAttachment.clearValue.b;
        colorTarget.clear_color.a = descriptor.colorAttachment.clearValue.a;

        switch (descriptor.colorAttachment.loadOp)
        {
        case LoadOp::Clear:    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR; break;
        case LoadOp::Load:     colorTarget.load_op = SDL_GPU_LOADOP_LOAD; break;
        case LoadOp::DontCare: colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
        }

        switch (descriptor.colorAttachment.storeOp)
        {
        case StoreOp::Store:    colorTarget.store_op = SDL_GPU_STOREOP_STORE; break;
        case StoreOp::DontCare: colorTarget.store_op = SDL_GPU_STOREOP_DONT_CARE; break;
        }

        if (descriptor.depthAttachment.enabled)
        {
            SDL_GPUTexture* depthTexture = nullptr;
            if (descriptor.depthTarget.IsValid())
            {
                auto* pTex = m_texturePool.Get(descriptor.depthTarget);
                depthTexture = pTex ? *pTex : nullptr;
            }
            else if (m_depthTexture)
            {
                depthTexture = m_depthTexture;
            }

            if (depthTexture)
            {
                SDL_GPUDepthStencilTargetInfo depthTarget{};
                depthTarget.texture = depthTexture;
                depthTarget.clear_depth = descriptor.depthAttachment.clearDepth;

                switch (descriptor.depthAttachment.loadOp)
                {
                case LoadOp::Clear:    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR; break;
                case LoadOp::Load:     depthTarget.load_op = SDL_GPU_LOADOP_LOAD; break;
                case LoadOp::DontCare: depthTarget.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
                }

                switch (descriptor.depthAttachment.storeOp)
                {
                case StoreOp::Store:    depthTarget.store_op = SDL_GPU_STOREOP_STORE; break;
                case StoreOp::DontCare: depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE; break;
                }

                depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

                m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, &depthTarget);
            }
            else
            {
                WAYFINDER_WARNING(LogRenderer, "Depth attachment enabled but no depth texture available (depthTarget={}, m_depthTexture={})",
                    descriptor.depthTarget.IsValid(), m_depthTexture != nullptr);
                m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, nullptr);
            }
        }
        else
        {
            m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, nullptr);
        }
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
        info.code = desc.code;
        info.code_size = desc.codeSize;
        info.entrypoint = desc.entryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateShader: Device does not support SPIR-V");
            return GPUShaderHandle::Invalid();
        }
        info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
        info.stage = (desc.stage == ShaderStage::Vertex)
            ? SDL_GPU_SHADERSTAGE_VERTEX
            : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = desc.numSamplers;
        info.num_storage_textures = desc.numStorageTextures;
        info.num_storage_buffers = desc.numStorageBuffers;
        info.num_uniform_buffers = desc.numUniformBuffers;

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
        if (!m_device) return;
        auto* pShader = m_shaderPool.Get(shader);
        if (pShader)
        {
            SDL_ReleaseGPUShader(m_device, *pShader);
            m_shaderPool.Release(shader);
        }
    }

    static SDL_GPUVertexElementFormat ToSDLVertexFormat(VertexAttribFormat fmt)
    {
        switch (fmt)
        {
        case VertexAttribFormat::Float2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        case VertexAttribFormat::Float3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case VertexAttribFormat::Float4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        }
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    }

    GPUPipelineHandle SDLGPUDevice::CreatePipeline(const PipelineCreateDesc& desc)
    {
        if (!m_device || !m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: No GPU device or window");
            return GPUPipelineHandle::Invalid();
        }

        // Build vertex attributes
        std::vector<SDL_GPUVertexAttribute> vertexAttribs;
        vertexAttribs.reserve(desc.vertexLayout.attribCount);
        for (uint32_t i = 0; i < desc.vertexLayout.attribCount; ++i)
        {
            const auto& a = desc.vertexLayout.attribs[i];
            SDL_GPUVertexAttribute attr{};
            attr.location = a.location;
            attr.buffer_slot = 0;
            attr.format = ToSDLVertexFormat(a.format);
            attr.offset = a.offset;
            vertexAttribs.push_back(attr);
        }

        SDL_GPUVertexInputState vertexInput{};

        SDL_GPUVertexBufferDescription vbDesc{};

        if (desc.vertexLayout.attribCount > 0)
        {
            vbDesc.slot = 0;
            vbDesc.pitch = desc.vertexLayout.stride;
            vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            vbDesc.instance_step_rate = 0;

            vertexInput.vertex_buffer_descriptions = &vbDesc;
            vertexInput.num_vertex_buffers = 1;
            vertexInput.vertex_attributes = vertexAttribs.data();
            vertexInput.num_vertex_attributes = static_cast<Uint32>(vertexAttribs.size());
        }

        // Rasterizer
        SDL_GPURasterizerState rasterizer{};
        switch (desc.fillMode)
        {
        case FillMode::Fill: rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL; break;
        case FillMode::Line: rasterizer.fill_mode = SDL_GPU_FILLMODE_LINE; break;
        }
        switch (desc.cullMode)
        {
        case CullMode::None:  rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE; break;
        case CullMode::Front: rasterizer.cull_mode = SDL_GPU_CULLMODE_FRONT; break;
        case CullMode::Back:  rasterizer.cull_mode = SDL_GPU_CULLMODE_BACK; break;
        }
        switch (desc.frontFace)
        {
        case FrontFace::CounterClockwise: rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE; break;
        case FrontFace::Clockwise:        rasterizer.front_face = SDL_GPU_FRONTFACE_CLOCKWISE; break;
        }

        // Primitive type
        SDL_GPUPrimitiveType primType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        switch (desc.primitiveType)
        {
        case PrimitiveType::TriangleList:  primType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; break;
        case PrimitiveType::TriangleStrip: primType = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; break;
        case PrimitiveType::LineList:      primType = SDL_GPU_PRIMITIVETYPE_LINELIST; break;
        case PrimitiveType::LineStrip:     primType = SDL_GPU_PRIMITIVETYPE_LINESTRIP; break;
        case PrimitiveType::PointList:     primType = SDL_GPU_PRIMITIVETYPE_POINTLIST; break;
        }

        // Colour target — match swapchain format
        SDL_GPUColorTargetDescription colorTargetDesc{};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);

        SDL_GPUGraphicsPipelineTargetInfo targetInfo{};
        targetInfo.color_target_descriptions = &colorTargetDesc;
        targetInfo.num_color_targets = 1;
        targetInfo.has_depth_stencil_target = desc.depthTestEnabled || desc.depthWriteEnabled;
        if (targetInfo.has_depth_stencil_target)
        {
            targetInfo.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        }

        // Depth-stencil
        SDL_GPUDepthStencilState depthStencil{};
        depthStencil.enable_depth_test = desc.depthTestEnabled;
        depthStencil.enable_depth_write = desc.depthWriteEnabled;
        depthStencil.compare_op = SDL_GPU_COMPAREOP_LESS;

        // Resolve shader handles to raw SDL pointers
        auto* pVS = m_shaderPool.Get(desc.vertexShader);
        auto* pFS = m_shaderPool.Get(desc.fragmentShader);
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
        if (!m_device) return;
        auto* pPipeline = m_pipelinePool.Get(pipeline);
        if (pPipeline)
        {
            SDL_ReleaseGPUGraphicsPipeline(m_device, *pPipeline);
            m_pipelinePool.Release(pipeline);
        }
    }

    void SDLGPUDevice::BindPipeline(GPUPipelineHandle pipeline)
    {
        if (!m_renderPass) return;
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
        info.usage = (desc.usage == BufferUsage::Vertex)
            ? SDL_GPU_BUFFERUSAGE_VERTEX
            : SDL_GPU_BUFFERUSAGE_INDEX;
        info.size = desc.sizeInBytes;

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
        if (!m_device) return;
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (pBuffer)
        {
            SDL_ReleaseGPUBuffer(m_device, *pBuffer);
            m_bufferPool.Release(buffer);
        }
    }

    void SDLGPUDevice::UploadToBuffer(GPUBufferHandle buffer, const void* data, uint32_t sizeInBytes, uint32_t dstOffsetInBytes)
    {
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!m_device || !pBuffer || !data || sizeInBytes == 0)
        {
            return;
        }

        // Create a staging transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = sizeInBytes;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
        if (!transferBuffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToBuffer: Failed to create transfer buffer — {}", SDL_GetError());
            return;
        }

        // Map, copy, unmap
        void* mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
        if (!mapped)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::UploadToBuffer: Failed to map transfer buffer — {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
            return;
        }

        std::memcpy(mapped, data, sizeInBytes);
        SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

        // Upload via a dedicated command buffer + copy pass
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
        dst.buffer = *pBuffer;
        dst.offset = dstOffsetInBytes;
        dst.size = sizeInBytes;

        SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
        SDL_EndGPUCopyPass(copyPass);

        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    }

    // ── Draw Commands ────────────────────────────────────────

    void SDLGPUDevice::BindVertexBuffer(GPUBufferHandle buffer, uint32_t slot, uint32_t offsetInBytes)
    {
        if (!m_renderPass) return;
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!pBuffer) return;

        SDL_GPUBufferBinding binding{};
        binding.buffer = *pBuffer;
        binding.offset = offsetInBytes;

        SDL_BindGPUVertexBuffers(m_renderPass, slot, &binding, 1);
    }

    void SDLGPUDevice::BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes)
    {
        if (!m_renderPass) return;
        auto* pBuffer = m_bufferPool.Get(buffer);
        if (!pBuffer) return;

        SDL_GPUBufferBinding binding{};
        binding.buffer = *pBuffer;
        binding.offset = offsetInBytes;

        SDL_GPUIndexElementSize sdlIndexSize = (indexSize == IndexElementSize::Uint16)
            ? SDL_GPU_INDEXELEMENTSIZE_16BIT
            : SDL_GPU_INDEXELEMENTSIZE_32BIT;

        SDL_BindGPUIndexBuffer(m_renderPass, &binding, sdlIndexSize);
    }

    void SDLGPUDevice::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                   uint32_t firstIndex, int32_t vertexOffset)
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
        info.code = desc.code;
        info.code_size = desc.codeSize;
        info.entrypoint = desc.entryPoint;

        if (!(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateComputePipeline: Device does not support SPIR-V");
            return GPUComputePipelineHandle::Invalid();
        }
        info.format = static_cast<SDL_GPUShaderFormat>(m_shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV);
        info.num_samplers = desc.numSamplers;
        info.num_readonly_storage_textures = desc.numReadOnlyStorageTextures;
        info.num_readonly_storage_buffers = desc.numReadOnlyStorageBuffers;
        info.num_readwrite_storage_textures = desc.numReadWriteStorageTextures;
        info.num_readwrite_storage_buffers = desc.numReadWriteStorageBuffers;
        info.num_uniform_buffers = desc.numUniformBuffers;
        info.threadcount_x = desc.threadCountX;
        info.threadcount_y = desc.threadCountY;
        info.threadcount_z = desc.threadCountZ;

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
        if (!m_device) return;
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
        if (!m_computePass) return;
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

    static SDL_GPUTextureFormat ToSDLTextureFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RGBA8_UNORM:   return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGRA8_UNORM:   return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case TextureFormat::R16_FLOAT:     return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
        case TextureFormat::RGBA16_FLOAT:  return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::R32_FLOAT:     return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        case TextureFormat::D32_FLOAT:     return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        case TextureFormat::D24_UNORM_S8:  return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        }
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    }

    static SDL_GPUTextureUsageFlags ToSDLTextureUsage(TextureUsage usage)
    {
        SDL_GPUTextureUsageFlags flags = 0;
        if (HasFlag(usage, TextureUsage::Sampler))     flags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (HasFlag(usage, TextureUsage::ColourTarget)) flags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        if (HasFlag(usage, TextureUsage::DepthTarget)) flags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        return flags;
    }

    GPUTextureHandle SDLGPUDevice::CreateTexture(const TextureCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateTexture: No GPU device");
            return GPUTextureHandle::Invalid();
        }

        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = ToSDLTextureFormat(desc.format);
        info.width = desc.width;
        info.height = desc.height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = ToSDLTextureUsage(desc.usage);

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &info);
        if (!texture)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateTexture: Failed ({}x{}) — {}", desc.width, desc.height, SDL_GetError());
            return GPUTextureHandle::Invalid();
        }

        return m_texturePool.Acquire(texture);
    }

    void SDLGPUDevice::DestroyTexture(GPUTextureHandle texture)
    {
        if (!m_device) return;
        auto* pTexture = m_texturePool.Get(texture);
        if (pTexture)
        {
            SDL_ReleaseGPUTexture(m_device, *pTexture);
            m_texturePool.Release(texture);
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
        info.min_filter = (desc.minFilter == SamplerFilter::Nearest) ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        info.mag_filter = (desc.magFilter == SamplerFilter::Nearest) ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

        auto toAddressMode = [](SamplerAddressMode mode) -> SDL_GPUSamplerAddressMode {
            switch (mode)
            {
            case SamplerAddressMode::Repeat:         return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            case SamplerAddressMode::ClampToEdge:    return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            case SamplerAddressMode::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            }
            return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        };

        info.address_mode_u = toAddressMode(desc.addressModeU);
        info.address_mode_v = toAddressMode(desc.addressModeV);
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
        if (!m_device) return;
        auto* pSampler = m_samplerPool.Get(sampler);
        if (pSampler)
        {
            SDL_ReleaseGPUSampler(m_device, *pSampler);
            m_samplerPool.Release(sampler);
        }
    }

    void SDLGPUDevice::BindFragmentSampler(uint32_t slot, GPUTextureHandle texture, GPUSamplerHandle sampler)
    {
        if (!m_renderPass) return;
        auto* pTexture = m_texturePool.Get(texture);
        auto* pSampler = m_samplerPool.Get(sampler);
        if (!pTexture || !pSampler) return;

        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = *pTexture;
        binding.sampler = *pSampler;

        SDL_BindGPUFragmentSamplers(m_renderPass, slot, &binding, 1);
    }

    void SDLGPUDevice::GetSwapchainDimensions(uint32_t& width, uint32_t& height) const
    {
        width = m_swapchainWidth;
        height = m_swapchainHeight;
    }

} // namespace Wayfinder
