
add_definitions(-DUSE_WINE_TODOS)

list(APPEND SOURCE
    advpack.c
    files.c
    install.c
    testlist.c)

add_executable(advpack_winetest ${SOURCE})
target_link_libraries(advpack_winetest)
set_module_type(advpack_winetest win32cui)
add_importlibs(advpack_winetest advpack cabinet advapi32 msvcrt kernel32)
add_cd_file(TARGET advpack_winetest DESTINATION reactos/bin FOR all)
