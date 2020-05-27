# Electronic Arts Standard Template Library
#add_subdirectory(EASTL)
#target_compile_definitions(EASTL PUBLIC EA_PLATFORM_LINUX=1)
#target_compile_definitions(EASTL PUBLIC EA_HAVE_CPP11_MUTEX=1)

# C++ frozen constexpr containers
add_subdirectory(frozen)

add_subdirectory(strf)
