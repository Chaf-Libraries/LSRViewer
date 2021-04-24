SetTarget(
    MODE STATIC
    TARGET_NAME scene
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        base
        glm
        ktx
        imgui_lib
        entt
        vulkan
        ctpl
        tinygltf
        vma
        stb
        astc
)

set_property(TARGET scene PROPERTY FOLDER "LSRViewer")
