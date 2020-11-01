idf_component_register(
    SRCS 
        "wireless.c"
        "smartconfig.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
    
    PRIV_REQUIRES
        json
        mqtt
)