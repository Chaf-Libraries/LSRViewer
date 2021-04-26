SetTarget(
    MODE STATIC
    TARGET_NAME shader_compiler
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        base
        glslang
        SPIRV
        glslang-default-resource-limits
        spirv-cross-glsl
        vulkan
)

set_property(TARGET shader_compiler PROPERTY FOLDER "LSRViewer")
