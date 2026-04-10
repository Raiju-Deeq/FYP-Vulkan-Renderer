/**
 * @file Pipeline.cpp
 * @brief Implementation of Pipeline - SPIR-V loading and Dynamic Rendering pipeline creation.
 *
 * The key design point of this class is that it uses Vulkan 1.3 Dynamic Rendering:
 * the colour attachment format is described at pipeline creation time via
 * VkPipelineRenderingCreateInfo (chained through pNext), and NO VkRenderPass
 * or VkFramebuffer objects are created anywhere in this codebase.
 *
 * For Milestone 1 the pipeline has:
 *   - No vertex input bindings (positions are hardcoded in triangle.vert)
 *   - No descriptor sets (no UBOs, no samplers)
 *   - No push constants
 *   - Dynamic viewport and scissor (set at draw time, not baked in)
 *   - Back-face culling disabled (single-sided triangle, both faces visible)
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 *
 * @see Pipeline.h for the class interface.
 */

#include "Pipeline.h"
#include "VulkanContext.h"

#include <spdlog/spdlog.h>
#include <fstream>

// ─── Private helpers ──────────────────────────────────────────────────────────

/// @details
/// Opens the file in binary mode, seeks to end to measure size, then reads
/// the entire contents into a uint32_t vector.  SPIR-V is guaranteed to be
/// 4-byte aligned by the spec, so reinterpret_cast from char* is safe here.
/// Returns an empty vector if the file cannot be opened or is empty.
std::vector<uint32_t> Pipeline::loadSpv(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("Pipeline: cannot open shader file '{}'", path);
        return {};
    }

    const std::streamsize byteSize = file.tellg();
    if (byteSize <= 0 || byteSize % 4 != 0) {
        spdlog::error("Pipeline: shader '{}' has invalid size {}", path, byteSize);
        return {};
    }

    file.seekg(0);
    std::vector<uint32_t> code(static_cast<size_t>(byteSize) / 4);
    file.read(reinterpret_cast<char*>(code.data()), byteSize);
    return code;
}

/// @details
/// Wraps raw SPIR-V bytecode in a VkShaderModule.  The module is a thin driver
/// object — it just holds the bytecode until the pipeline is compiled.  Once
/// the pipeline is built it can be destroyed, which is done at the end of init().
VkShaderModule Pipeline::createShaderModule(VkDevice device,
                                             const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        spdlog::error("Pipeline: vkCreateShaderModule failed");
        return VK_NULL_HANDLE;
    }
    return module;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

/// @details
/// Build sequence:
///   1.  Load SPIR-V bytecode for both shaders.
///   2.  Create VkShaderModules (temporary — destroyed after pipeline compilation).
///   3.  Describe the two shader stages (vertex + fragment).
///   4.  Configure fixed-function state: vertex input, input assembly, rasterizer,
///       multisampling, colour blending, dynamic state.
///   5.  Create an empty VkPipelineLayout (no descriptors or push constants in M1).
///   6.  Chain VkPipelineRenderingCreateInfo into pNext — this replaces
///       VkRenderPass and tells the driver the attachment formats at compile time.
///   7.  Call vkCreateGraphicsPipelines.
///   8.  Destroy the shader modules (no longer needed).
bool Pipeline::init(const VulkanContext& ctx,
                    const std::string&   vertSpvPath,
                    const std::string&   fragSpvPath,
                    VkFormat             colourFormat)
{
    // ── 1 & 2: Load SPIR-V and create shader modules ──────────────────────
    auto vertCode = loadSpv(vertSpvPath);
    if (vertCode.empty()) return false;

    auto fragCode = loadSpv(fragSpvPath);
    if (fragCode.empty()) return false;

    VkShaderModule vertModule = createShaderModule(ctx.device(), vertCode);
    VkShaderModule fragModule = createShaderModule(ctx.device(), fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 3: Shader stage descriptors ───────────────────────────────────────
    // Each stage names its entry point ("main") and the pipeline stage it feeds.
    VkPipelineShaderStageCreateInfo shaderStages[2]{};

    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName  = "main";

    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName  = "main";

    // ── 4a: Vertex input ──────────────────────────────────────────────────
    // M1 triangle.vert has positions baked in as a constant array indexed by
    // gl_VertexIndex.  No VkBuffer is bound, so this struct is fully empty.
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    // ── 4b: Input assembly ────────────────────────────────────────────────
    // TRIANGLE_LIST: every 3 vertices form an independent triangle.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ── 4c: Dynamic viewport and scissor ─────────────────────────────────
    // Marking these as dynamic means we set them with vkCmdSetViewport /
    // vkCmdSetScissor at draw time rather than baking the window size into
    // the pipeline.  This is required for swapchain resize support (M4).
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // Viewport/scissor counts must be declared even though the actual values
    // are set dynamically at command recording time.
    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ── 4d: Rasterizer ────────────────────────────────────────────────────
    // FILL mode: draw solid triangles.
    // CULL_MODE_NONE: the M1 triangle is not a closed mesh, so we skip culling.
    // Depth clamp / depth bias are off — not needed until M2.
    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    // ── 4e: Multisampling ─────────────────────────────────────────────────
    // 1 sample per pixel (no MSAA) for M1.
    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── 4f: Colour blend attachment ───────────────────────────────────────
    // Write all four channels, no blending — output colour replaces the
    // destination directly.  Alpha blending is added in M5 for Gaussian splats.
    VkPipelineColorBlendAttachmentState colourAttachment{};
    colourAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colourAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colourBlend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments    = &colourAttachment;

    // ── 5: Pipeline layout ────────────────────────────────────────────────
    // Empty for M1 — no push constants, no descriptor set layouts.
    // Descriptor layouts are added in M3 (texture sampler) and M4 (UBO).
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        spdlog::error("Pipeline: failed to create pipeline layout");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 6: Dynamic Rendering attachment info (replaces VkRenderPass) ──────
    // VkPipelineRenderingCreateInfo is chained into pNext of the pipeline
    // create info.  It tells the driver which attachment formats this pipeline
    // will render to, so it can specialise its internal shader compilation.
    // There is no VkRenderPass and no VkFramebuffer anywhere in this codebase.
    VkPipelineRenderingCreateInfo renderingInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingInfo.colorAttachmentCount    = 1;
    renderingInfo.pColorAttachmentFormats = &colourFormat;

    // ── 7: Graphics pipeline ──────────────────────────────────────────────
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.pNext               = &renderingInfo; // ← Dynamic Rendering hook
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colourBlend;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_layout;
    pipelineInfo.renderPass          = VK_NULL_HANDLE; // no render pass — ever

    if (vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1,
                                   &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        spdlog::error("Pipeline: vkCreateGraphicsPipelines failed");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 8: Destroy shader modules (pipeline keeps its own compiled copy) ──
    vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
    vkDestroyShaderModule(ctx.device(), fragModule, nullptr);

    spdlog::info("Pipeline: created successfully (Dynamic Rendering, no VkRenderPass)");
    return true;
}

/// @details
/// Destroys pipeline, layout and descriptor set layout (if any was created).
/// Each handle is guarded with a null-check for safe partial-init cleanup.
void Pipeline::destroy(const VulkanContext& ctx)
{
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}
