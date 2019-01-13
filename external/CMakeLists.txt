EXECUTE_PROCESS(COMMAND git submodule update --init
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                OUTPUT_QUIET
        )

# Add Catch
if(NOT TARGET Catch2)
    add_subdirectory(Catch2)
endif()
