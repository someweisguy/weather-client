idf_component_register(
    SRCS 
        "max17043.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
    PRIV_REQUIRES
        serial
)