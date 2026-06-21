# ---- richc source tree ----

add_subdirectory(core)

if(NOT RICHC_CORE_ONLY)
    add_subdirectory(app)
endif()
