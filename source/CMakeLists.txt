SetTarget(
    MODE EXE
    TARGET_NAME LSRViewer
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        base
        glm
        ktx
        imgui_lib
        entt
        tinygltf
        vulkan
        ctpl
)

set_property(TARGET LSRViewer PROPERTY FOLDER )
