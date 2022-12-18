
set(JSON_FILE "${CMAKE_SOURCE_DIR}/dynamic_calls.json")
set(PYPROGRAM "${CMAKE_CURRENT_LIST_DIR}/generate.py")
set(GEN_FILES "${CMAKE_CURRENT_BINARY_DIR}/dyncall_api")

add_custom_command(
    OUTPUT ${GEN_FILES}.h ${GEN_FILES}.c ${GEN_FILES}.cpp
    COMMAND python3 ${PYPROGRAM} --verbose --cpp -j ${JSON_FILE} -o ${GEN_FILES}
    DEPENDS ${PYPROGRAM} ${JSON_FILE}
)
add_custom_target(generate_dyncalls ALL
    DEPENDS ${GEN_FILES}.h ${GEN_FILES}.c ${GEN_FILES}.cpp
)
