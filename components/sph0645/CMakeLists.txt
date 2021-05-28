idf_component_register(
    SRCS "sos_iir_filter.c" "sph0645.cpp"
    INCLUDE_DIRS "."
    REQUIRES sensor serial
    PRIV_REQUIRES
)