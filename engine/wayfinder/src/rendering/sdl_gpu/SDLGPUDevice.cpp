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

    bool SDLGPUDevice::Initialize(Window& window)
    {
        m_window = static_cast<SDL_Window*>(window.GetNativeHandle());
        if (!m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Window has no valid native handle");
            return false;
        }

        m_device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
            true, // debug mode
            nullptr);

        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to create GPU device — {}", SDL_GetError());
            return false;
        }

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

        WAYFINDER_INFO(LogRenderer, "SDLGPUDevice: Initialized (driver: {})", m_info.DriverInfo);
        return true;
    }

    void SDLGPUDevice::Shutdown()
    {
        if (m_device)
        {
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
            // Window is minimized or occluded — submit empty command buffer and skip rendering
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
        if (!m_commandBuffer || !m_swapchainTexture)
        {
            return;
        }

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = m_swapchainTexture;
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

        if (descriptor.depthAttachment.enabled && m_depthTexture)
        {
            SDL_GPUDepthStencilTargetInfo depthTarget{};
            depthTarget.texture = m_depthTexture;
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
            return nullptr;
        }

        SDL_GPUShaderCreateInfo info{};
        info.code = desc.code;
        info.code_size = desc.codeSize;
        info.entrypoint = desc.entryPoint;
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
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
            return nullptr;
        }

        return static_cast<GPUShaderHandle>(shader);
    }

    void SDLGPUDevice::DestroyShader(GPUShaderHandle shader)
    {
        if (m_device && shader)
        {
            SDL_ReleaseGPUShader(m_device, static_cast<SDL_GPUShader*>(shader));
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
            return nullptr;
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

        SDL_GPUVertexBufferDescription vbDesc{};
        vbDesc.slot = 0;
        vbDesc.pitch = desc.vertexLayout.stride;
        vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbDesc.instance_step_rate = 0;

        SDL_GPUVertexInputState vertexInput{};
        vertexInput.vertex_buffer_descriptions = &vbDesc;
        vertexInput.num_vertex_buffers = 1;
        vertexInput.vertex_attributes = vertexAttribs.data();
        vertexInput.num_vertex_attributes = static_cast<Uint32>(vertexAttribs.size());

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

        // Color target — match swapchain format
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

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = static_cast<SDL_GPUShader*>(desc.vertexShader);
        pipelineInfo.fragment_shader = static_cast<SDL_GPUShader*>(desc.fragmentShader);
        pipelineInfo.vertex_input_state = vertexInput;
        pipelineInfo.primitive_type = primType;
        pipelineInfo.rasterizer_state = rasterizer;
        pipelineInfo.depth_stencil_state = depthStencil;
        pipelineInfo.target_info = targetInfo;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);
        if (!pipeline)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreatePipeline: Failed — {}", SDL_GetError());
            return nullptr;
        }

        return static_cast<GPUPipelineHandle>(pipeline);
    }

    void SDLGPUDevice::DestroyPipeline(GPUPipelineHandle pipeline)
    {
        if (m_device && pipeline)
        {
            SDL_ReleaseGPUGraphicsPipeline(m_device, static_cast<SDL_GPUGraphicsPipeline*>(pipeline));
        }
    }

    void SDLGPUDevice::BindPipeline(GPUPipelineHandle pipeline)
    {
        if (m_renderPass && pipeline)
        {
            SDL_BindGPUGraphicsPipeline(m_renderPass, static_cast<SDL_GPUGraphicsPipeline*>(pipeline));
        }
    }

    // ── Buffers ──────────────────────────────────────────────

    GPUBufferHandle SDLGPUDevice::CreateBuffer(const BufferCreateDesc& desc)
    {
        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice::CreateBuffer: No GPU device");
            return nullptr;
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
            return nullptr;
        }

        return static_cast<GPUBufferHandle>(buffer);
    }

    void SDLGPUDevice::DestroyBuffer(GPUBufferHandle buffer)
    {
        if (m_device && buffer)
        {
            SDL_ReleaseGPUBuffer(m_device, static_cast<SDL_GPUBuffer*>(buffer));
        }
    }

    void SDLGPUDevice::UploadToBuffer(GPUBufferHandle buffer, const void* data, uint32_t sizeInBytes, uint32_t dstOffsetInBytes)
    {
        if (!m_device || !buffer || !data || sizeInBytes == 0)
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
        dst.buffer = static_cast<SDL_GPUBuffer*>(buffer);
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
        if (!m_renderPass || !buffer)
        {
            return;
        }

        SDL_GPUBufferBinding binding{};
        binding.buffer = static_cast<SDL_GPUBuffer*>(buffer);
        binding.offset = offsetInBytes;

        SDL_BindGPUVertexBuffers(m_renderPass, slot, &binding, 1);
    }

    void SDLGPUDevice::BindIndexBuffer(GPUBufferHandle buffer, IndexElementSize indexSize, uint32_t offsetInBytes)
    {
        if (!m_renderPass || !buffer)
        {
            return;
        }

        SDL_GPUBufferBinding binding{};
        binding.buffer = static_cast<SDL_GPUBuffer*>(buffer);
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
            return nullptr;
        }

        SDL_GPUComputePipelineCreateInfo info{};
        info.code = desc.code;
        info.code_size = desc.codeSize;
        info.entrypoint = desc.entryPoint;
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
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
            return nullptr;
        }

        return static_cast<GPUComputePipelineHandle>(pipeline);
    }

    void SDLGPUDevice::DestroyComputePipeline(GPUComputePipelineHandle pipeline)
    {
        if (m_device && pipeline)
        {
            SDL_ReleaseGPUComputePipeline(m_device, static_cast<SDL_GPUComputePipeline*>(pipeline));
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
        if (m_computePass && pipeline)
        {
            SDL_BindGPUComputePipeline(m_computePass, static_cast<SDL_GPUComputePipeline*>(pipeline));
        }
    }

    void SDLGPUDevice::DispatchCompute(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        if (m_computePass)
        {
            SDL_DispatchGPUCompute(m_computePass, groupCountX, groupCountY, groupCountZ);
        }
    }

} // namespace Wayfinder
