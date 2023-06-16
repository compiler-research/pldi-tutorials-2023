add_subdirectory(fast_modmul_adder)
add_subdirectory(modopt)

add_custom_target(p1-ex4)
add_dependencies(p1-ex4 modopt fast_modmul_adder)