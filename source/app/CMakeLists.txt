SetTarget(
    MODE EXE
    TARGET_NAME app
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        scene
        renderer
)

set_property(TARGET app PROPERTY FOLDER "LSRViewer")
