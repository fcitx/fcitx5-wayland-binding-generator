add_library(templateResources OBJECT templates.qrc)

set_target_properties(templateResources PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    AUTORCC TRUE)
