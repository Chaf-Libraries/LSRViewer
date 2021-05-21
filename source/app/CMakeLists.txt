SetTarget(
    MODE EXE
    TARGET_NAME app
    DEFINE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}/"
    INC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/extern/vulkan/base
    LIB
        scene
        renderer
)

set_property(TARGET app PROPERTY FOLDER "LSRViewer")