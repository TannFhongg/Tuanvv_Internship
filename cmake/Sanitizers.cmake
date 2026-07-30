function(tuanvv_enable_sanitizers target)
    if(MSVC)
        target_compile_options(${target} INTERFACE /fsanitize=address)
        target_link_options(${target} INTERFACE /fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target}
            INTERFACE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(
            ${target}
            INTERFACE
                -fsanitize=address,undefined
        )
    endif()
endfunction()
