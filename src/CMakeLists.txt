if (NOT TARGET TJpg_Decoder)
    add_library(TJpg_Decoder INTERFACE)

    target_sources(TJpg_Decoder INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/TJpg_Decoder.cpp
            ${CMAKE_CURRENT_LIST_DIR}/tjpgd.c
    )
endif()
