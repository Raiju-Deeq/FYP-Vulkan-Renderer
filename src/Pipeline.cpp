/**
 * @file Pipeline.cpp
 * @brief Implementation of Pipeline — SPIR-V loading and Dynamic Rendering pipeline creation.
 *
 * ## Key design point: no VkRenderPass
 * This renderer uses Vulkan 1.3 Dynamic Rendering throughout.  Rather than
 * pre-baking a `VkRenderPass` object that describes all attachments in
 * advance, I chain a `VkPipelineRenderingCreateInfo` struct into the
 * pipeline's `pNext` chain at creation time.  This struct declares which
 * attachment *formats* the pipeline will render to.  The actual image views
 * are then provided at command-recording time via `vkCmdBeginRendering`.
 *
 * This is cleaner for a renderer that rebuilds pipelines on resize (I just
 * pass the new swapchain format — no RenderPass object to update).
 *
 * @author Mohamed Deeq Mohamed (P2884884)
 * @date   2026-04-10
 * @see    Pipeline.h for the class interface.
 */

#include "Pipeline.h"
#include "VulkanContext.h"

#include <spdlog/spdlog.h>
#include <fstream>

// =============================================================================
// loadSpv()  — private static helper
// =============================================================================

/// @details
/// **Why `uint32_t` instead of `char`?**
/// The Vulkan spec requires SPIR-V to be 4-byte aligned.  Storing the bytes
/// in a `vector<uint32_t>` guarantees the correct alignment for
/// `VkShaderModuleCreateInfo::pCode`.
///
/// I open the file in binary mode and read it in one shot.  I seek to the
/// end first to get the size, then seek back to the start to read — this
/// avoids allocating a temporary char buffer and then copying.
std::vector<uint32_t> Pipeline::loadSpv(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("Pipeline: cannot open shader file '{}'", path);
        return {};
    }

    const std::streamsize byteSize = file.tellg();
    // SPIR-V is always a multiple of 4 bytes.  A size that isn't a multiple
    // of 4 means the file is corrupt or not a valid .spv.
    if (byteSize <= 0 || byteSize % 4 != 0) {
        spdlog::error("Pipeline: shader '{}' has invalid byte size {}", path, byteSize);
        return {};
    }

    file.seekg(0);
    std::vector<uint32_t> code(static_cast<size_t>(byteSize) / 4);
    file.read(reinterpret_cast<char*>(code.data()), byteSize);
    return code;
}

// =============================================================================
// createShaderModule()  — private static helper
// =============================================================================

/// @details
/// A `VkShaderModule` is a thin driver wrapper around the SPIR-V bytecode.
/// It lives only until the pipeline is compiled — after that, the driver
/// keeps its own internal representation of the compiled shader and the
/// module can be destroyed.
///
/// Best practice: destroy the module in the same function that calls
/// `vkCreateGraphicsPipelines`, immediately after the pipeline is created.
VkShaderModule Pipeline::createShaderModule(VkDevice device,
                                             const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(uint32_t); // byte count, not element count
    info.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        spdlog::error("Pipeline: vkCreateShaderModule failed");
        return VK_NULL_HANDLE;
    }
    return module;
}

// =============================================================================
// init()
// =============================================================================

/// @details
/// The pipeline creation process mirrors assembling a GPU's internal state
/// machine.  Each sub-struct configures one stage or set of fixed-function
/// behaviour.  The final `VkGraphicsPipelineCreateInfo` references all of
/// them, and `vkCreateGraphicsPipelines` compiles everything into one opaque
/// GPU object.
///
/// **Viewport/scissor as dynamic state:**
/// I mark these as dynamic so I don't bake the window size into the pipeline.
/// When the window is resized I only need to rebuild the swapchain and call
/// `vkCmdSetViewport`/`vkCmdSetScissor` with new values — not rebuild the
/// whole pipeline.
///
/// **Dynamic Rendering attachment chain:**
/// `VkPipelineRenderingCreateInfo` is chained into `pNext` and tells the
/// driver the format(s) of the attachment(s) this pipeline will write to.
/// It completely replaces the `renderPass` field, which is left as
/// `VK_NULL_HANDLE`.  The validation layers will confirm this is correct.
bool Pipeline::init(const VulkanContext& ctx,
                    const std::string&   vertSpvPath,
                    const std::string&   fragSpvPath,
                    VkFormat             colourFormat)
{
    // ── 1 & 2: Load SPIR-V and create shader modules ──────────────────────
    // Shader modules are only needed during pipeline compilation.  I create
    // them here and destroy them at the end of this function (step 8).
    auto vertCode = loadSpv(vertSpvPath);
    if (vertCode.empty()) return false;

    auto fragCode = loadSpv(fragSpvPath);
    if (fragCode.empty()) return false;

    VkShaderModule vertModule = createShaderModule(ctx.device(), vertCode);
    VkShaderModule fragModule = createShaderModule(ctx.device(), fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        // Clean up whichever module was created before bailing out.
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 3: Shader stage descriptors ───────────────────────────────────────
    // Each struct wires a VkShaderModule into a specific pipeline stage.
    // "main" is the GLSL entry point name — this is a convention, not a
    // fixed requirement, but virtually all shaders use it.
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
    // For M1, triangle.vert has all three vertex positions as a constant
    // array indexed by gl_VertexIndex.  No VkBuffer is bound — the struct
    // is completely empty (zero binding descriptions, zero attributes).
    // From M2 onwards this will list binding strides and per-vertex attributes
    // matching the Vertex struct layout.
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    // ── 4b: Input assembly ────────────────────────────────────────────────
    // TRIANGLE_LIST: every group of 3 consecutive vertices forms one triangle.
    // primitiveRestartEnable (false) is only needed for strip/fan topologies.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ── 4c: Dynamic viewport and scissor ─────────────────────────────────
    // Listing these in the dynamic state array means their values are NOT
    // baked into the pipeline.  Instead, I must call vkCmdSetViewport and
    // vkCmdSetScissor every frame before the first draw.  The tradeoff is
    // that I can reuse the pipeline without change when the window is resized —
    // only the per-frame dynamic calls change.
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // Even though the values are dynamic, I must tell the pipeline how many
    // viewports/scissors there will be at draw time.
    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ── 4d: Rasteriser ────────────────────────────────────────────────────
    // FILL: draw solid filled triangles (vs. LINE for wireframe).
    // CULL_MODE_NONE: the M1 triangle is not a closed mesh and has no
    //   defined "back" face, so I disable culling entirely.
    // lineWidth must be 1.0f unless the wideLines feature is enabled.
    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    // ── 4e: Multisampling ─────────────────────────────────────────────────
    // 1 sample per pixel = no MSAA.  MSAA is a stretch goal for M5+.
    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── 4f: Colour blend ──────────────────────────────────────────────────
    // One attachment state per colour attachment in the render pass.
    // blendEnable = false means the fragment's output colour replaces the
    // destination pixel directly (no alpha blending).
    // colorWriteMask enables writing to all four channels (RGBA).
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
    // The layout declares what *types* of resources the shaders can access:
    //   - setLayoutCount / pSetLayouts  : descriptor set layouts (UBOs, samplers)
    //   - pushConstantRangeCount        : push constant ranges
    // For M1 the triangle shader has no external resources, so I leave this empty.
    // M2 will add the MVP UBO descriptor set layout here.
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        spdlog::error("Pipeline: failed to create pipeline layout");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 6: Dynamic Rendering attachment (replaces VkRenderPass) ──────────
    // This struct is unique to Dynamic Rendering (Vulkan 1.3 core).
    // It declares the format of each colour attachment (and optionally depth/
    // stencil) that this pipeline will write to.  It goes into pNext of the
    // pipeline create info — the driver uses it during shader specialisation
    // to know what format it's writing into at compilation time.
    //
    // Critically: VkGraphicsPipelineCreateInfo::renderPass is left as
    // VK_NULL_HANDLE.  The validation layer will confirm this is valid when
    // the pipeline has a VkPipelineRenderingCreateInfo in its pNext chain.
    VkPipelineRenderingCreateInfo renderingInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingInfo.colorAttachmentCount    = 1;
    renderingInfo.pColorAttachmentFormats = &colourFormat; // must match acquireNextImage format

    // ── 7: Assemble and compile the pipeline ──────────────────────────────
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.pNext               = &renderingInfo;  // ← Dynamic Rendering hook
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
    pipelineInfo.renderPass          = VK_NULL_HANDLE;  // no VkRenderPass — ever

    if (vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1,
                                   &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        spdlog::error("Pipeline: vkCreateGraphicsPipelines failed");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    // ── 8: Destroy shader modules ─────────────────────────────────────────
    // The pipeline has been compiled.  The driver extracted what it needed
    // from the SPIR-V; I no longer need the VkShaderModule handles and
    // freeing them reduces memory usage.
    vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
    vkDestroyShaderModule(ctx.device(), fragModule, nullptr);

    spdlog::info("Pipeline: created successfully (Dynamic Rendering, no VkRenderPass)");
    return true;
}

// =============================================================================
// destroy()
// =============================================================================

/// @details
/// Pipelines, layouts and descriptor set layouts have no dependency ordering
/// between them — any order is safe.  I destroy the pipeline first (most
/// complex object), then the layout (it was required during pipeline creation
/// but the pipeline keeps its own reference).
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

    // Descriptor set layout is null for M1 — this branch is a no-op until M2.
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}
