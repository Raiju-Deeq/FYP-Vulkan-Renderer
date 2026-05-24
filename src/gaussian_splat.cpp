/**
 * @file gaussian_splat.cpp
 * @brief Implementation of the minimal `.ply` Gaussian splat stretch renderer.
 */

#include "gaussian_splat.hpp"

#include "vulkan_context.hpp"

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
/**
 * @struct PlyProperty
 * @brief One property column from the PLY vertex element.
 */
struct PlyProperty
{
    std::string type; ///< Scalar data type, for example float or uchar.
    std::string name; ///< Property name, for example x, red or scale_0.
};

/**
 * @enum PlyFormat
 * @brief PLY data encoding supported by this loader.
 */
enum class PlyFormat
{
    Ascii,              ///< Human-readable rows after the header.
    BinaryLittleEndian  ///< Binary scalar rows after the header.
};

/**
 * @struct SplatPushConstants
 * @brief Small per-draw data shared with the splat shaders.
 */
struct SplatPushConstants
{
    glm::mat4 mvp{1.0f}; ///< Transform from world space to clip space.
    glm::vec4 controls{1.0f, 1.0f, 0.0f, 0.0f}; ///< X radius scale, Y opacity scale.
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
 * @brief Reads a float property with a default fallback.
 * @param values Parsed numeric values from one vertex row.
 * @param index Property index, or -1 if missing.
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
 * @brief Converts a PLY colour channel into 0..1.
 * @param value Colour value from file.
 * @return Normalised colour.
 */
float normaliseColour(float value)
{
    if (value > 1.0f) {
        return std::clamp(value / 255.0f, 0.0f, 1.0f);
    }
    return std::clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Converts a Gaussian SH DC coefficient into a visible colour channel.
 *
 * Postshot/3DGS PLY files often store colour as degree-0 spherical harmonic
 * coefficients named `f_dc_0..2` instead of `red/green/blue`.  A full 3DGS
 * renderer evaluates SH per view direction; this stretch renderer only needs
 * a stable preview colour, so I recover the common DC colour approximation.
 *
 * @param value Raw `f_dc_*` value from the PLY file.
 * @return Colour channel in 0..1.
 */
float normaliseDcColour(float value)
{
    constexpr float SH_C0 = 0.28209479177f;
    return std::clamp(0.5f + SH_C0 * value, 0.0f, 1.0f);
}

/**
 * @brief Converts common Gaussian PLY opacity values into alpha.
 * @param value Raw opacity value.
 * @param hasOpacity true if the file supplied opacity.
 * @return Alpha in 0..1.
 */
float normaliseOpacity(float value, bool hasOpacity)
{
    if (!hasOpacity) {
        return 1.0f;
    }

    // 3DGS files often store opacity as a logit.  Ordinary point clouds may
    // store alpha directly.  I treat values outside 0..1 as logits and pass
    // direct values through unchanged.
    const float opacity = (value < 0.0f || value > 1.0f)
        ? 1.0f / (1.0f + std::exp(-value))
        : std::clamp(value, 0.0f, 1.0f);

    // Postshot splats can have very low learned opacity.  A full 3DGS renderer
    // handles this with sorted compositing over many layers; my preview path is
    // simpler, so I keep a small opacity floor to make the loaded colour visible.
    return std::clamp(std::max(opacity, 0.12f), 0.0f, 1.0f);
}

/**
 * @brief Converts optional scale properties into a simple billboard radius.
 *
 * Full 3DGS projects covariance into screen space.  For this stretch version I
 * collapse the three scale values into one conservative radius, which is enough
 * to make points appear as soft splats instead of square pixels.
 */
float makeRadius(float scale0, float scale1, float scale2, bool hasScale)
{
    if (!hasScale) {
        return 0.025f;
    }

    const float averageScale = (scale0 + scale1 + scale2) / 3.0f;
    const float radius = std::exp(averageScale);
    return std::clamp(radius, 0.002f, 0.15f);
}

/**
 * @brief Returns the byte size for a scalar PLY property type.
 * @param type PLY scalar type name.
 * @return Size in bytes, or 0 if the type is unsupported.
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
 * @brief Reads one binary little-endian scalar value and converts it to float.
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
 * @brief Re-centres and scales the loaded point cloud into the camera view.
 *
 * Different splat exporters use different scene units.  For this FYP stretch
 * feature I normalise the cloud into a predictable demo-sized volume so loading
 * a new `.ply` behaves like hot-swapping an OBJ: it appears on screen without
 * hand-editing camera values.
 *
 * @param points Loaded splats to modify in-place.
 */
void normalisePointCloudForDemo(std::vector<GaussianPoint>& points)
{
    if (points.empty()) {
        return;
    }

    glm::vec3 minBounds(points.front().positionOpacity);
    glm::vec3 maxBounds(points.front().positionOpacity);
    for (const GaussianPoint& point : points) {
        const glm::vec3 position(point.positionOpacity);
        minBounds = glm::min(minBounds, position);
        maxBounds = glm::max(maxBounds, position);
    }

    const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    const glm::vec3 extent = maxBounds - minBounds;
    const float maxExtent = std::max({extent.x, extent.y, extent.z, 0.0001f});
    const float sceneScale = 1.8f / maxExtent;

    for (GaussianPoint& point : points) {
        const glm::vec3 position(point.positionOpacity);
        glm::vec3 normalisedPosition = (position - center) * sceneScale;
        // The Postshot sample is upside down relative to my renderer's current
        // camera/up convention, so I flip Y once during import instead of
        // adding another special case to the render loop.
        normalisedPosition.y *= -1.0f;
        point.positionOpacity.x = normalisedPosition.x;
        point.positionOpacity.y = normalisedPosition.y;
        point.positionOpacity.z = normalisedPosition.z;
        point.colorRadius.w *= sceneScale;
    }
}

/**
 * @brief Expands and slightly saturates loaded preview colours.
 *
 * Full Gaussian splatting evaluates spherical harmonics and composites many
 * translucent layers in depth order.  This prototype only renders soft
 * billboards, so I stretch the DC colour range after loading to make the
 * Postshot colour data readable on screen instead of appearing nearly grey.
 *
 * @param points Loaded splats to modify in-place.
 */
void enhancePreviewColours(std::vector<GaussianPoint>& points)
{
    if (points.empty()) {
        return;
    }

    glm::vec3 minColour(points.front().colorRadius);
    glm::vec3 maxColour(points.front().colorRadius);
    for (const GaussianPoint& point : points) {
        const glm::vec3 colour(point.colorRadius);
        minColour = glm::min(minColour, colour);
        maxColour = glm::max(maxColour, colour);
    }

    const glm::vec3 range = glm::max(maxColour - minColour, glm::vec3(0.001f));
    for (GaussianPoint& point : points) {
        glm::vec3 colour(point.colorRadius);
        colour = (colour - minColour) / range;

        const float luminance = glm::dot(colour, glm::vec3(0.2126f, 0.7152f, 0.0722f));
        colour = glm::mix(glm::vec3(luminance), colour, 1.35f);
        colour = glm::clamp(colour, glm::vec3(0.0f), glm::vec3(1.0f));

        point.colorRadius.x = colour.r;
        point.colorRadius.y = colour.g;
        point.colorRadius.z = colour.b;
    }
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

    VkDescriptorSetLayoutBinding pointBinding{};
    pointBinding.binding = 0;
    pointBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pointBinding.descriptorCount = 1;
    pointBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = 1;
    setLayoutInfo.pBindings = &pointBinding;
    if (vkCreateDescriptorSetLayout(ctx.device(), &setLayoutInfo, nullptr,
                                    &m_descriptorSetLayout) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create descriptor set layout");
        vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
        vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
        return false;
    }

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(SplatPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create pipeline layout");
        destroyPipeline(ctx);
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
    colourAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
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
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

bool GaussianSplat::loadFromPly(const VulkanContext& ctx, const std::string& path)
{
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
    bool readingVertexProperties = false;
    size_t vertexCount = 0;
    std::vector<PlyProperty> properties;

    while (std::getline(file, line)) {
        if (line == "end_header") {
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
                spdlog::error("GaussianSplat: '{}' uses list vertex properties; unsupported for C3 preview", path);
                return false;
            }
            if (plyScalarSize(type) == 0) {
                spdlog::error("GaussianSplat: '{}' uses unsupported PLY property type '{}'", path, type);
                return false;
            }
            properties.push_back(PlyProperty{type, name});
        }
    }

    if (!recognisedFormat) {
        spdlog::error("GaussianSplat: '{}' has an unsupported PLY format", path);
        return false;
    }
    if (vertexCount == 0 || properties.empty()) {
        spdlog::error("GaussianSplat: '{}' has no vertex data", path);
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
    const bool hasScale = scale0Index >= 0 && scale1Index >= 0 && scale2Index >= 0;
    const bool hasRgb = redIndex >= 0 && greenIndex >= 0 && blueIndex >= 0;
    const bool hasDcColour = dc0Index >= 0 && dc1Index >= 0 && dc2Index >= 0;

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
        point.positionOpacity = {
            readValue(values, xIndex, 0.0f),
            readValue(values, yIndex, 0.0f),
            readValue(values, zIndex, 0.0f),
            normaliseOpacity(readValue(values, opacityIndex, 1.0f), opacityIndex >= 0)
        };

        const float red = hasRgb
            ? normaliseColour(readValue(values, redIndex, 255.0f))
            : (hasDcColour ? normaliseDcColour(readValue(values, dc0Index, 0.0f)) : 1.0f);
        const float green = hasRgb
            ? normaliseColour(readValue(values, greenIndex, 255.0f))
            : (hasDcColour ? normaliseDcColour(readValue(values, dc1Index, 0.0f)) : 1.0f);
        const float blue = hasRgb
            ? normaliseColour(readValue(values, blueIndex, 255.0f))
            : (hasDcColour ? normaliseDcColour(readValue(values, dc2Index, 0.0f)) : 1.0f);

        point.colorRadius = {
            red,
            green,
            blue,
            makeRadius(readValue(values, scale0Index, 0.0f),
                       readValue(values, scale1Index, 0.0f),
                       readValue(values, scale2Index, 0.0f),
                       hasScale)
        };
        points.push_back(point);
    }

    if (points.empty()) {
        spdlog::error("GaussianSplat: '{}' did not contain drawable points", path);
        return false;
    }

    if (hasDcColour) {
        enhancePreviewColours(points);
    }
    normalisePointCloudForDemo(points);

    GpuBuffer::BufferResource replacement{};
    if (!GpuBuffer::uploadBuffer(ctx,
                                 points.data(),
                                 static_cast<VkDeviceSize>(points.size() * sizeof(GaussianPoint)),
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 replacement)) {
        spdlog::error("GaussianSplat: failed to upload point buffer");
        return false;
    }

    VkDescriptorPool replacementPool = VK_NULL_HANDLE;
    VkDescriptorSet replacementSet = VK_NULL_HANDLE;

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &replacementPool) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to create descriptor pool");
        GpuBuffer::destroyBuffer(ctx, replacement);
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = replacementPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &allocInfo, &replacementSet) != VK_SUCCESS) {
        spdlog::error("GaussianSplat: failed to allocate descriptor set");
        vkDestroyDescriptorPool(ctx.device(), replacementPool, nullptr);
        GpuBuffer::destroyBuffer(ctx, replacement);
        return false;
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = replacement.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = replacement.size;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = replacementSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }
    GpuBuffer::destroyBuffer(ctx, m_pointBuffer);

    m_pointBuffer = replacement;
    m_descriptorPool = replacementPool;
    m_descriptorSet = replacementSet;
    m_pointCount = static_cast<uint32_t>(points.size());

    spdlog::info("GaussianSplat: loaded '{}' ({} points)", path, m_pointCount);
    return true;
}

void GaussianSplat::draw(VkCommandBuffer cmd,
                         const glm::mat4& mvp,
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

    const SplatPushConstants pushConstants{
        .mvp = mvp,
        .controls = glm::vec4(radiusScale, opacityScale, 0.0f, 0.0f)
    };
    vkCmdPushConstants(cmd,
                       m_pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(SplatPushConstants),
                       &pushConstants);

    // Six vertices form two triangles per billboard. gl_InstanceIndex selects
    // the GaussianPoint record in the storage buffer.
    vkCmdDraw(cmd, 6, m_pointCount, 0, 0);
}

void GaussianSplat::destroy(const VulkanContext& ctx)
{
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    GpuBuffer::destroyBuffer(ctx, m_pointBuffer);
    m_pointCount = 0;
    destroyPipeline(ctx);
}

bool GaussianSplat::isDrawable() const
{
    return m_pipeline != VK_NULL_HANDLE &&
           m_pipelineLayout != VK_NULL_HANDLE &&
           m_descriptorSet != VK_NULL_HANDLE &&
           m_pointBuffer.buffer != VK_NULL_HANDLE &&
           m_pointCount > 0;
}
