
message(STATUS "TGE building libs ...")

# add_subdirectory(mbedtls)
# add_subdirectory(lwip)

add_subdirectory(json-c)
add_subdirectory(EmonLib)
add_subdirectory(HC-SR04)
add_subdirectory(pico-onewire)

add_subdirectory(ePaper/Config)
add_subdirectory(ePaper/e-Paper)
add_subdirectory(ePaper/Fonts)
add_subdirectory(ePaper/GUI)

add_subdirectory(pico-vfs)

