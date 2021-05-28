idf_component_register(
    SRCS "pms5003.cpp"
    INCLUDE_DIRS "."
    REQUIRES sensor serial
    PRIV_REQUIRES 
)