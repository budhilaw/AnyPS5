#ifndef CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP
#define CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <memory>
#include <vector>

namespace ShaderRecompiler {

struct ResourceSnapshot;
struct ResourceSpecialization;

enum class ShaderStage {
    Compute,
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Local,
    Mesh
};

struct MemoryRegion {
    std::uint64_t guestAddress;
    std::span<const std::byte> bytes;
};

struct ShaderBinary {
    ShaderStage stage;
    std::uint64_t codeAddress;
    std::span<const std::uint32_t> code;
    std::uint64_t headerAddress;
    std::span<const std::byte> header;
    // Nonzero: a hash of `code` the caller computed once, which cache keys use in place of the
    // code itself (it runs to the end of the registered binary: kilobytes per lookup otherwise).
    std::uint64_t codeHash = 0;
};

struct ShaderComputeStageInfo {
    std::array<std::uint32_t, 3> numThreads;
    std::uint32_t ldsSizeDwords;
    std::array<bool, 3> groupIdEnable;
    bool tgSizeEnable;
    std::uint32_t threadIdComponentCount;
};

struct ShaderPixelStageInfo {
    std::uint32_t interpolatorCount;
    std::array<std::uint32_t, 32> interpolatorSettings;
    bool wave32;
    std::uint32_t perspectiveCenterVgpr;
    bool hasPerspectiveCenterVgpr;
    bool posX;
    bool posY;
    bool posZ;
    bool posW;
    bool frontFace;
    bool ancillary;
    bool sampleShading;
    bool noPerspective;
    bool pixelKillEnable;
    bool depthExportEnable;
    bool sampleMaskExportEnable;
    bool earlyZ;
    bool executeOnNoop;
    std::array<std::uint8_t, 8> targetOutputMode;
    std::array<std::uint8_t, 8> targetExportMapping;
    bool dualSourceBlend = false;  // DB_SHADER_CONTROL.DUAL_EXPORT_ENABLE: MRT1 is blend source 1
};

struct ShaderVertexBufferResource {
    std::array<std::uint32_t, 4> fields;
};

struct ShaderVertexResourceDestination {
    std::int32_t registerStart;
    std::int32_t registersNum;
    std::int32_t attrId;
    std::uint32_t fetchIndex;
};

struct ShaderVertexStageInfo {
    static constexpr std::uint32_t MaxResources = 32;
    std::array<ShaderVertexBufferResource, MaxResources> resources;
    std::array<ShaderVertexResourceDestination, MaxResources> resourcesDst;
    std::uint32_t resourcesNum;
    std::uint32_t fetchAttribReg;
    std::uint32_t fetchBufferReg;
    bool fetchEmbedded;
};

struct GuestContext {
    std::uint32_t waveSize;
    std::uint32_t userDataBaseRegister;
    std::span<const std::uint32_t> userData;
    std::optional<ShaderComputeStageInfo> compute;
    std::optional<ShaderPixelStageInfo> pixel;
    std::optional<ShaderVertexStageInfo> vertex;
    std::span<const MemoryRegion> memory;
};

struct MeshTargetLimits {
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxSharedMemoryBytes;
    std::uint32_t maxOutputVertices;
    std::uint32_t maxOutputPrimitives;
    std::uint32_t maxOutputComponents;
    std::uint32_t maxOutputMemoryBytes;
    std::uint32_t outputPerVertexGranularity;
    std::uint32_t outputPerPrimitiveGranularity;
};

struct TessellationTargetLimits {
    std::uint32_t maxPatchSize;
    std::uint32_t maxControlPerVertexInputComponents;
    std::uint32_t maxControlPerVertexOutputComponents;
    std::uint32_t maxControlPerPatchOutputComponents;
    std::uint32_t maxControlTotalOutputComponents;
    std::uint32_t maxEvaluationInputComponents;
    std::uint32_t maxEvaluationOutputComponents;
};

struct SpirvTarget {
    std::uint32_t vulkanVersion;
    std::uint32_t spirvVersion;
    std::uint32_t subgroupSize;
    std::uint32_t bdaAbiVersion;
    std::span<const std::uint32_t> supportedCapabilities;
    std::span<const std::string_view> supportedExtensions;
    bool fragmentShaderBarycentricEnabled;
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxWorkgroupSharedMemoryBytes;
    std::optional<MeshTargetLimits> mesh;
    std::optional<TessellationTargetLimits> tessellation;
    // Bit (1 << ShaderStage) set for each stage the device supports subgroup operations in; a
    // stage without them is emitted as single-lane waves (no cross-lane communication).
    std::uint32_t subgroupStageMask = 0xffffffffu;
};

struct BindingLayout {
    std::uint32_t descriptorSet;
    std::uint32_t firstBinding;
    std::uint32_t pushConstantOffsetBytes;
    std::uint32_t pushConstantSizeBytes;
};

enum class ProgramRole {
    Main,
    GeometryBack,
    Local,
    Hull,
    Domain,
    Fragment
};

struct LinkedProgram {
    ProgramRole role;
    ShaderBinary binary;
    std::uint32_t userDataBaseRegister;
    std::uint32_t firstUserSgpr;
    std::span<const std::uint32_t> userData;
};

struct MeshConfiguration {
    std::uint32_t inputPrimitive;
    std::uint32_t primitivesPerGroup;
    std::uint32_t verticesPerGroup;
    std::uint32_t maxVertices;
    std::uint32_t maxPrimitives;
    std::uint32_t threadsPerGroup;
    std::uint32_t ldsSizeDwords;
    std::uint32_t provokingVertex;
};

struct TessellationConfiguration {
    std::uint32_t inputControlPoints;
    std::uint32_t outputControlPoints;
    std::uint32_t domain;
    std::uint32_t partitioning;
    std::uint32_t outputTopology;
};

struct GraphicsDrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexElementBytes;
    std::uint32_t instanceCount;
};

struct GraphicsCompileContext {
    std::uint32_t firstUserSgpr;
    std::span<const LinkedProgram> linkedPrograms;
    std::optional<MeshConfiguration> mesh;
    std::optional<TessellationConfiguration> tessellation;
    GraphicsDrawParameters draw;
};

struct RecompileRequest {
    ShaderBinary shader;
    GuestContext context;
    SpirvTarget target;
    BindingLayout layout;
    std::optional<GraphicsCompileContext> graphics;
    bool useCache = true;
    // The request's resources as the caller materialized them from the same memory (reading
    // them is how it captured that memory): a cached compile skips evaluating its plan again.
    const ResourceSnapshot* materializedSnapshot = nullptr;
    const ResourceSpecialization* materializedSpecialization = nullptr;
};

enum class DescriptorKind {
    UniformBuffer,
    StorageBuffer,
    UniformTexelBuffer,
    StorageTexelBuffer,
    SampledImage,
    StorageImage,
    Sampler
};

enum class DescriptorImageShape {
    Image1D,
    Image2D,
    Image2DArray,
    ImageCube,
    Image3D
};

enum class DescriptorRole {
    GuestBuffers,
    GuestImages,
    GuestSamplers,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData
};

struct DescriptorBinding {
    DescriptorKind kind;
    DescriptorRole role;
    std::uint32_t descriptorSet;
    std::uint32_t binding;
    std::uint32_t count;
    std::vector<std::uint32_t> guestDescriptor;
    bool readOnly = false;
    // GuestBuffers: whether the shader stores to (or atomically updates) each element.
    std::vector<bool> elementWritten;
    // GuestBuffers: whether the shader loads from (or atomically updates) each element.
    std::vector<bool> elementRead;
    std::optional<DescriptorImageShape> imageShape;
    std::vector<bool> samplerDepthCompare;
};

struct VertexAttribute {
    std::uint32_t location;
    std::uint32_t components;
    ShaderVertexBufferResource resource;
    std::uint32_t fetchIndex;
};

struct FragmentParameter {
    std::uint32_t location;
    std::uint32_t sourceLocation;
    bool flat;
    bool perVertex;
};

// SPIR-V words shared by the copies of a compiled result: a cache hit copies one per draw.
class SpirvCode {
public:
    SpirvCode() = default;
    SpirvCode(std::vector<std::uint32_t> words) : words(std::make_shared<std::vector<std::uint32_t>>(std::move(words))) {}
    const std::vector<std::uint32_t>& Words() const {
        static const std::vector<std::uint32_t> none;
        return words ? *words : none;
    }
    operator const std::vector<std::uint32_t>&() const { return Words(); }
    // Words of this copy alone, to change.
    std::vector<std::uint32_t>& Edit() {
        if (!words) words = std::make_shared<std::vector<std::uint32_t>>();
        else if (words.use_count() > 1) words = std::make_shared<std::vector<std::uint32_t>>(*words);
        return *words;
    }
    std::size_t size() const { return Words().size(); }
    bool empty() const { return Words().empty(); }
    const std::uint32_t* data() const { return Words().data(); }
    const std::uint32_t* begin() const { return data(); }
    const std::uint32_t* end() const { return data() + size(); }
    std::uint32_t operator[](std::size_t index) const { return Words()[index]; }
    bool operator==(const SpirvCode& other) const { return Words() == other.Words(); }

private:
    std::shared_ptr<std::vector<std::uint32_t>> words;
};

struct RecompileResult {
    SpirvCode spirv;
    // Nonzero: identifies `spirv` (with its size) for host caches, computed once per compiled
    // variant so draws need not hash the module again.
    std::uint64_t spirvHash = 0;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    std::uint32_t bdaAbiVersion = 0;
    std::vector<VertexAttribute> vertexAttributes;
    std::int32_t vertexOffsetSgpr = -1;
    std::int32_t instanceOffsetSgpr = -1;
    std::vector<std::uint32_t> parameterExports;
    std::vector<FragmentParameter> fragmentParameters;
    bool cacheHit = false;
};

[[nodiscard]] RecompileResult Recompile(const RecompileRequest& request);

struct RectListShaders {
    RecompileResult control;
    RecompileResult evaluation;
};

[[nodiscard]] RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target);

}

#endif
