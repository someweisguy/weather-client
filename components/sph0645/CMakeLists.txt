idf_component_register(
    SRCS "sos_iir_filter.c" "sph0645.hpp"
    INCLUDE_DIRS "."
    REQUIRES
    PRIV_REQUIRES serial
)