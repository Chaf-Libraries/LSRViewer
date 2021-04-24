SetTarget(
    MODE STATIC
    TARGET_NAME renderer
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        scene
)

set_property(TARGET renderer PROPERTY FOLDER "LSRViewer")
