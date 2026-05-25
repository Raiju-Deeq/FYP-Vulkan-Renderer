/**
 * @file gaussian_splat.cpp
 * @brief GPU-projected Gaussian splat implementation for the C3 stretch goal.
 *
 * This file owns the stretch renderer's CPU-side import work. I parse the PLY
 * properties, activate the common 3DGS values, precompute covariance once, and
 * upload the compact records that `splat.vert` projects on the GPU every frame.
 */

#include "gaussian_splat.hpp"

#include "vulkan_context.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace
{
/**
 * @struct PlyProperty
 * @brief One scalar property column from the PLY vertex element.
 */
struct PlyProperty
{
    std::string type; ///< Scalar data type, for example float or uchar.
    std::string name; ///< Property name, for example x, f_dc_0 or rot_3.
};

/**
 * @enum PlyFormat
 * @brief PLY data encodings supported by this loader.
 */
enum class PlyFormat
{
    Ascii,             ///< Human-readable rows after the header.
    BinaryLittleEndian ///< Binary scalar rows after the header.
};

/**
 * @struct SplatPushConstants
 * @brief Small values the vertex shader needs while projecting GPU splats.
 *
 * Vulkan guarantees at least 128 bytes of push constants.  This struct is 112
 * bytes, so I can pass the camera and splat controls directly at draw time
 * without adding a separate uniform buffer just for this stretch path.
 */
struct SplatPushConstants
{
    glm::mat4 modelView{1.0f}; ///< Model-to-view matrix for the rotating splat model.
    glm::vec4 viewportFocal{1.0f}; ///< width, height, focalX, focalY.
    glm::vec4 projectionParams{1.0f}; ///< tanHalfFovX, tanHalfFovY, near plane, far plane.
    glm::vec4 controls{1.0f}; ///< radius scale, opacity scale, unused, unused.
};

/**
 * @brief Finds the index of a named PLY property.
 * @param properties Header property list.
 * @param name Property name to search for.
 * @return Zero-based property index, or -1 when missing.
 */
int findProperty(const std::vector<PlyProperty>& properties, const std::string& name)
{
    for (size_t i = 0; i < properties.size(); ++i) {
        if (properties[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/**
 * @brief Reads a float property with a fallback for missing optional columns.
 * @param values Parsed numeric values from one vertex row.
 * @param index Property index, or -1 if the property is absent.
 * @param fallback Value used when the property is absent.
 * @return Parsed value or fallback.
 */
float readValue(const std::vector<float>& values, int index, float fallback)
{
    if (index < 0 || static_cast<size_t>(index) >= values.size()) {
        return fallback;
    }
    return values[static_cast<size_t>(index)];
}

/**
 * @brief Converts ordinary RGB PLY colour channels into 0..1.
 * @param value Colour value from the file.
 * @return Normalised colour channel.
 */
float normaliseColour(float value)
{
    if (value > 1.0f) {
        return std::clamp(value / 255.0f, 0.0f, 1.0f);
    }
    return std::clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Converts a degree-zero spherical harmonic coefficient into colour.
 *
 * 3DGS PLY files usually store base colour as `f_dc_0..2`.  For this preview I
 * use the common degree-zero spherical harmonics reconstruction:
 *
 *     colour = 0.5 + SH_C0 * f_dc
 *
 * @param value Raw `f_dc_*` value from the PLY file.
 * @return Colour channel in 0..1.
 */
float normaliseDcColour(float value)
{
    constexpr float SH_C0 = 0.28209479177387814f;
    return std::clamp(0.5f + SH_C0 * value, 0.0f, 1.0f);
}

/**
 * @brief Sigmoid activation used by Gaussian opacity logits.
 * @param value Raw opacity/logit value.
 * @return Activated opacity in 0..1.
 */
float sigmoid(float value)
{
    return 1.0f / (1.0f + std::exp(-value));
}

/**
 * @brief Converts the PLY opacity property into the alpha value I composite.
 * @param value Raw opacity value.
 * @param hasOpacity true when the file supplied an opacity column.
 * @return Activated opacity in 0..1.
 */
float normaliseOpacity(float value, bool hasOpacity)
{
    if (!hasOpacity) {
        return 1.0f;
    }

    // 3DGS exports usually store opacity as a logit, so I apply the same
    // sigmoid activation used by the training/rendering papers.
    return std::clamp(sigmoid(value), 0.0f, 1.0f);
}

/**
 * @brief Exponential activation used by Gaussian scale values.
 * @param value Raw scale property.
 * @return Activated positive scale.
 */
float activateScale(float value)
{
    return std::exp(value);
}

/**
 * @brief Returns the byte size for a scalar PLY property type.
 * @param type PLY scalar type name.
 * @return Size in bytes, or 0 if unsupported.
 */
size_t plyScalarSize(const std::string& type)
{
    if (type == "char" || type == "uchar" ||
        type == "int8" || type == "uint8") {
        return 1;
    }
    if (type == "short" || type == "ushort" ||
        type == "int16" || type == "uint16") {
        return 2;
    }
    if (type == "int" || type == "uint" ||
        type == "int32" || type == "uint32" ||
        type == "float" || type == "float32") {
        return 4;
    }
    if (type == "double" || type == "float64") {
        return 8;
    }
    return 0;
}

/**
 * @brief Reads one binary little-endian scalar and converts it to float.
 * @param file Binary stream positioned at the property value.
 * @param type PLY scalar type.
 * @param outValue Receives the converted value.
 * @return true if the bytes were read successfully.
 */
bool readBinaryScalar(std::ifstream& file, const std::string& type, float& outValue)
{
    if (type == "float" || type == "float32") {
        float value = 0.0f;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = value;
        return static_cast<bool>(file);
    }
    if (type == "double" || type == "float64") {
        double value = 0.0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "uchar" || type == "uint8") {
        uint8_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "char" || type == "int8") {
        int8_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "ushort" || type == "uint16") {
        uint16_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "short" || type == "int16") {
        int16_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "uint" || type == "uint32") {
        uint32_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }
    if (type == "int" || type == "int32") {
        int32_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        outValue = static_cast<float>(value);
        return static_cast<bool>(file);
    }

    return false;
}

/**
 * @brief Re-centres and scales the loaded point cloud into a predictable view.
 *
 * PLY splats can be exported in arbitrary scene units.  For this FYP renderer
 * I normalise the cloud once at import so a new file appears in front of the
 * existing camera without hand-editing transforms.
 *
 * @param points Loaded splats to modify in-place.
 */
void normalisePointCloudForDemo(std::vector<GaussianPoint>& points)
{
    if (points.empty()) {
        return;
    }

    glm::vec3 minBounds = points.front().position;
    glm::vec3 maxBounds = points.front().position;
    for (const GaussianPoint& point : points) {
        minBounds = glm::min(minBounds, point.position);
        maxBounds = glm::max(maxBounds, point.position);
    }

    const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    const glm::vec3 extent = maxBounds - minBounds;
    const float maxExtent = std::max({extent.x, extent.y, extent.z, 0.0001f});
    const float sceneScale = 1.8f / maxExtent;

    for (GaussianPoint& point : points) {
        point.position = (point.position - center) * sceneScale;

        // The Postshot sample is upside down relative to the renderer's Y-up
        // camera convention, so I fix that once during import.  This keeps the
        // render loop free from file-specific orientation hacks.
        point.position.y *= -1.0f;
        point.scale *= sceneScale;
    }
}

/**
 * @brief Builds the six unique covariance values from Gaussian scale/rotation.
 *
 * The covariance tells the vertex shader how wide and directional the splat is
 * before projection. I build the quaternion rotation matrix, apply the Gaussian
 * scale, then store the symmetric `M * M^T` result as six unique floats.
 *
 * @param scale Activated Gaussian scale.
 * @param rotation Normalised Gaussian rotation.
 * @return Upper triangle of the symmetric 3x3 covariance matrix.
 */
std::array<float, 6> computeCovariance3D(const glm::vec3& scale,
                                         const glm::quat& rotation)
{
    const float qw = rotation.w;
    const float qx = rotation.x;
    const float qy = rotation.y;
    const float qz = rotation.z;

    const std::array<float, 9> r = {
        1.0f - 2.0f * (qy * qy + qz * qz),
        2.0f * (qx * qy - qw * qz),
        2.0f * (qx * qz + qw * qy),
        2.0f * (qx * qy + qw * qz),
        1.0f - 2.0f * (qx * qx + qz * qz),
        2.0f * (qy * qz - qw * qx),
        2.0f * (qx * qz - qw * qy),
        2.0f * (qy * qz + qw * qx),
        1.0f - 2.0f * (qx * qx + qy * qy)
    };

    const std::array<float, 9> m = {
        r[0] * scale.x, r[1] * scale.x, r[2] * scale.x,
        r[3] * scale.y, r[4] * scale.y, r[5] * scale.y,
        r[6] * scale.z, r[7] * scale.z, r[8] * scale.z
    };

    return {
        m[0] * m[0] + m[3] * m[3] + m[6] * m[6],
        m[0] * m[1] + m[3] * m[4] + m[6] * m[7],
        m[0] * m[2] + m[3] * m[5] + m[6] * m[8],
        m[1] * m[1] + m[4] * m[4] + m[7] * m[7],
        m[1] * m[2] + m[4] * m[5] + m[7] * m[8],
        m[2] * m[2] + m[5] * m[5] + m[8] * m[8]
    };
}

} // namespace

std::vector<uint32_t> GaussianSplat::loadSpv(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("GaussianSplat: cannot open shader '{}'", path);
        return {};
    }

    const std::streamsize byteSize = file.tellg();
    if (byteSize <= 0 || byteSize % 4 != 0) {
        spdlog::error("GaussianSplat: shader '{}' has invalid byte size {}", path, byteSize);
        return {};
    }

    file.seekg(0);
    std::vector<uint32_t> code(static_cast<size_t>(byteSize) / 4);
    file.read(reinterpret_cast<char*>(code.data()), byteSize);
    return code;
}

VkShaderModule GaussianSplat::createShaderModule(VkDevice device,
                                                  const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create shader module");
        return VK_NULL_HANDLE;
    }
    return module;
}

bool GaussianSplat::initPipeline(const VulkanContext& ctx,
                                 const std::string& vertSpvPath,
                                 const std::string& fragSpvPath,
                                 VkFormat colourFormat,
                                 VkFormat depthFormat)
{
    destroyPipeline(ctx);

    if (m_descriptorSetLayout == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setLayoutInfo.bindingCount = 1;
        setLayoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(ctx.device(), &setLayoutInfo, nullptr,
                                        &m_descriptorSetLayout) != VK_SUCCESS) {
            spdlog::error("GaussianSplat: failed to create descriptor set layout");
            return false;
        }
    }

    const auto vertCode = loadSpv(vertSpvPath);
    const auto fragCode = loadSpv(fragSpvPath);
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    VkShaderModule vertModule = createShaderModule(ctx.device(), vertCode);
    VkShaderModule fragModule = createShaderModule(ctx.device(), fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(SplatPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create pipeline layout");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colourAttachment{};
    colourAttachment.blendEnable = VK_TRUE;
    // The fragment shader outputs pre-multiplied colour, so colour uses
    // src=ONE instead of SRC_ALPHA.
    colourAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colourAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colourAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colourAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colourAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colourAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colourAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colourBlend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colourBlend.attachmentCount = 1;
    colourBlend.pAttachments = &colourAttachment;

    VkPipelineRenderingCreateInfo renderingInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colourFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colourBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1,
                                  &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create graphics pipeline");
        destroyPipeline(ctx);
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
    vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
    spdlog::info("GaussianSplat: pipeline ready");
    return true;
}

void GaussianSplat::destroyPipeline(const VulkanContext& ctx)
{
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

bool GaussianSplat::loadFromPly(const VulkanContext& ctx, const std::string& path)
{
    if (m_descriptorSetLayout == VK_NULL_HANDLE) {
        spdlog::error("GaussianSplat: pipeline layout must be initialised before loading '{}'", path);
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("GaussianSplat: failed to open '{}'", path);
        return false;
    }

    std::string line;
    std::getline(file, line);
    if (line != "ply") {
        spdlog::error("GaussianSplat: '{}' is not a PLY file", path);
        return false;
    }

    PlyFormat plyFormat = PlyFormat::Ascii;
    bool recognisedFormat = false;
    bool foundHeaderEnd = false;
    bool readingVertexProperties = false;
    size_t vertexCount = 0;
    std::vector<PlyProperty> properties;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "end_header") {
            foundHeaderEnd = true;
            break;
        }

        std::istringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (keyword == "format") {
            std::string format;
            stream >> format;
            if (format == "ascii") {
                plyFormat = PlyFormat::Ascii;
                recognisedFormat = true;
            } else if (format == "binary_little_endian") {
                plyFormat = PlyFormat::BinaryLittleEndian;
                recognisedFormat = true;
            }
        } else if (keyword == "element") {
            std::string elementName;
            stream >> elementName;
            readingVertexProperties = (elementName == "vertex");
            if (readingVertexProperties) {
                stream >> vertexCount;
            }
        } else if (keyword == "property" && readingVertexProperties) {
            std::string type;
            std::string name;
            stream >> type >> name;
            if (type == "list") {
                spdlog::error("GaussianSplat: '{}' uses list vertex properties; unsupported for C3", path);
                return false;
            }
            if (plyScalarSize(type) == 0) {
                spdlog::error("GaussianSplat: '{}' uses unsupported PLY property type '{}'", path, type);
                return false;
            }
            properties.push_back(PlyProperty{type, name});
        }
    }

    if (!foundHeaderEnd) {
        spdlog::error("GaussianSplat: '{}' is missing end_header", path);
        return false;
    }
    if (!recognisedFormat) {
        spdlog::error("GaussianSplat: '{}' has an unsupported PLY format", path);
        return false;
    }
    if (vertexCount == 0 || properties.empty()) {
        spdlog::error("GaussianSplat: '{}' has no vertex data", path);
        return false;
    }
    if (vertexCount > std::numeric_limits<uint32_t>::max()) {
        spdlog::error("GaussianSplat: '{}' has too many vertices for this prototype", path);
        return false;
    }

    const int xIndex = findProperty(properties, "x");
    const int yIndex = findProperty(properties, "y");
    const int zIndex = findProperty(properties, "z");
    if (xIndex < 0 || yIndex < 0 || zIndex < 0) {
        spdlog::error("GaussianSplat: '{}' is missing x/y/z properties", path);
        return false;
    }

    const int redIndex = findProperty(properties, "red");
    const int greenIndex = findProperty(properties, "green");
    const int blueIndex = findProperty(properties, "blue");
    const int dc0Index = findProperty(properties, "f_dc_0");
    const int dc1Index = findProperty(properties, "f_dc_1");
    const int dc2Index = findProperty(properties, "f_dc_2");
    const int opacityIndex = findProperty(properties, "opacity");
    const int scale0Index = findProperty(properties, "scale_0");
    const int scale1Index = findProperty(properties, "scale_1");
    const int scale2Index = findProperty(properties, "scale_2");
    const int rot0Index = findProperty(properties, "rot_0");
    const int rot1Index = findProperty(properties, "rot_1");
    const int rot2Index = findProperty(properties, "rot_2");
    const int rot3Index = findProperty(properties, "rot_3");

    const bool hasRgb = redIndex >= 0 && greenIndex >= 0 && blueIndex >= 0;
    const bool hasDcColour = dc0Index >= 0 && dc1Index >= 0 && dc2Index >= 0;
    const bool hasScale = scale0Index >= 0 && scale1Index >= 0 && scale2Index >= 0;
    const bool hasRotation = rot0Index >= 0 && rot1Index >= 0 && rot2Index >= 0 && rot3Index >= 0;

    std::vector<GaussianPoint> points;
    points.reserve(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        std::vector<float> values(properties.size(), 0.0f);

        if (plyFormat == PlyFormat::Ascii) {
            if (!std::getline(file, line)) {
                break;
            }

            std::istringstream stream(line);
            for (float& value : values) {
                stream >> value;
            }
            if (!stream && !stream.eof()) {
                spdlog::warn("GaussianSplat: skipped malformed vertex row {}", i);
                continue;
            }
        } else {
            for (size_t propertyIndex = 0; propertyIndex < properties.size(); ++propertyIndex) {
                if (!readBinaryScalar(file,
                                      properties[propertyIndex].type,
                                      values[propertyIndex])) {
                    spdlog::error("GaussianSplat: failed while reading binary vertex row {}", i);
                    return false;
                }
            }
        }

        GaussianPoint point{};
        point.position = {
            readValue(values, xIndex, 0.0f),
            readValue(values, yIndex, 0.0f),
            readValue(values, zIndex, 0.0f)
        };

        point.color = {
            hasRgb ? normaliseColour(readValue(values, redIndex, 255.0f))
                   : (hasDcColour ? normaliseDcColour(readValue(values, dc0Index, 0.0f)) : 1.0f),
            hasRgb ? normaliseColour(readValue(values, greenIndex, 255.0f))
                   : (hasDcColour ? normaliseDcColour(readValue(values, dc1Index, 0.0f)) : 1.0f),
            hasRgb ? normaliseColour(readValue(values, blueIndex, 255.0f))
                   : (hasDcColour ? normaliseDcColour(readValue(values, dc2Index, 0.0f)) : 1.0f)
        };

        point.opacity = normaliseOpacity(readValue(values, opacityIndex, 1.0f),
                                         opacityIndex >= 0);

        point.scale = hasScale
            ? glm::vec3(activateScale(readValue(values, scale0Index, -4.6f)),
                        activateScale(readValue(values, scale1Index, -4.6f)),
                        activateScale(readValue(values, scale2Index, -4.6f)))
            : glm::vec3(0.01f);

        if (hasRotation) {
            const glm::quat rotation(readValue(values, rot0Index, 1.0f),
                                     readValue(values, rot1Index, 0.0f),
                                     readValue(values, rot2Index, 0.0f),
                                     readValue(values, rot3Index, 0.0f));
            const float length = glm::length(rotation);
            point.rotation = length > 0.000001f
                ? glm::normalize(rotation)
                : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

        points.push_back(point);
    }

    if (points.empty()) {
        spdlog::error("GaussianSplat: '{}' did not contain drawable points", path);
        return false;
    }

    normalisePointCloudForDemo(points);

    for (GaussianPoint& point : points) {
        // Scale is changed during normalisation, so I precompute covariance
        // after that step.  This removes quaternion/scale matrix work from the
        // shader and keeps each GPU vertex invocation focused on projection.
        point.covariance = computeCovariance3D(point.scale, point.rotation);
    }

    std::vector<GaussianGpuPoint> gpuPoints;
    gpuPoints.reserve(points.size());
    for (const GaussianPoint& point : points) {
        const auto& cov = point.covariance;
        gpuPoints.push_back(GaussianGpuPoint{
            .positionOpacity = glm::vec4(point.position, point.opacity),
            .color = glm::vec4(point.color, 0.0f),
            .covarianceA = glm::vec4(cov[0], cov[1], cov[2], cov[3]),
            .covarianceB = glm::vec4(cov[4], cov[5], 0.0f, 0.0f)
        });
    }

    GpuBuffer::BufferResource replacementBuffer{};
    VkDescriptorPool replacementPool = VK_NULL_HANDLE;
    VkDescriptorSet replacementSet = VK_NULL_HANDLE;

    const VkDeviceSize pointBufferSize =
        static_cast<VkDeviceSize>(gpuPoints.size() * sizeof(GaussianGpuPoint));
    if (!GpuBuffer::uploadBuffer(ctx,
                                 gpuPoints.data(),
                                 pointBufferSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 replacementBuffer)) {
        spdlog::error("GaussianSplat: failed to upload GPU splat buffer");
        return false;
    }

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &replacementPool) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create descriptor pool");
        GpuBuffer::destroyBuffer(ctx, replacementBuffer);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = replacementPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &allocInfo, &replacementSet) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to allocate descriptor set");
        vkDestroyDescriptorPool(ctx.device(), replacementPool, nullptr);
        GpuBuffer::destroyBuffer(ctx, replacementBuffer);
        return false;
    }

    VkDescriptorBufferInfo pointInfo{};
    pointInfo.buffer = replacementBuffer.buffer;
    pointInfo.offset = 0;
    pointInfo.range = replacementBuffer.size;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = replacementSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &pointInfo;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);

    // I only replace the active splat resources after every part of the new
    // upload succeeds. If a user picks a bad PLY file, the current cloud
    // stays alive instead of leaving the renderer half-empty.
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
    }
    GpuBuffer::destroyBuffer(ctx, m_pointBuffer);

    m_pointBuffer = replacementBuffer;
    m_descriptorPool = replacementPool;
    m_descriptorSet = replacementSet;
    m_pointCount = static_cast<uint32_t>(gpuPoints.size());
    spdlog::info("GaussianSplat: loaded '{}' ({} gaussians)", path, m_pointCount);
    return true;
}

void GaussianSplat::draw(VkCommandBuffer cmd,
                         const glm::mat4& modelView,
                         const glm::mat4& projection,
                         VkExtent2D extent,
                         float radiusScale,
                         float opacityScale) const
{
    if (!isDrawable()) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout,
                            0,
                            1,
                            &m_descriptorSet,
                            0,
                            nullptr);

    const float width = static_cast<float>(extent.width);
    const float height = static_cast<float>(extent.height);
    const float tanHalfFovX = 1.0f / std::abs(projection[0][0]);
    const float tanHalfFovY = 1.0f / std::abs(projection[1][1]);
    const float focalX = width / (2.0f * tanHalfFovX);
    const float focalY = height / (2.0f * tanHalfFovY);
    const float nearPlane = projection[3][2] / projection[2][2];
    const float farPlane = projection[3][2] / (projection[2][2] + 1.0f);

    const SplatPushConstants pushConstants{
        .modelView = modelView,
        .viewportFocal = glm::vec4(width, height, focalX, focalY),
        .projectionParams = glm::vec4(tanHalfFovX, tanHalfFovY, nearPlane, farPlane),
        .controls = glm::vec4(radiusScale, opacityScale, 0.0f, 0.0f)
    };
    vkCmdPushConstants(cmd,
                       m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(SplatPushConstants),
                       &pushConstants);

    // Six vertices form two triangles per splat. gl_InstanceIndex selects the
    // source Gaussian, and the vertex shader handles the camera-dependent maths.
    vkCmdDraw(cmd, 6, m_pointCount, 0, 0);
}

void GaussianSplat::destroy(const VulkanContext& ctx)
{
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    GpuBuffer::destroyBuffer(ctx, m_pointBuffer);
    m_descriptorSet = VK_NULL_HANDLE;
    m_pointCount = 0;
    destroyPipeline(ctx);

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

bool GaussianSplat::isDrawable() const
{
    return m_pipeline != VK_NULL_HANDLE &&
           m_pipelineLayout != VK_NULL_HANDLE &&
           m_descriptorSetLayout != VK_NULL_HANDLE &&
           m_descriptorSet != VK_NULL_HANDLE &&
           m_pointBuffer.buffer != VK_NULL_HANDLE &&
           m_pointCount > 0;
}
