
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_link_libraries(app ELMFAT)

	zephyr_compile_definitions(
			-DFLASH_AREA_STORAGE_OFFSET=0
			-DFLASH_AREA_STORAGE_SIZE=1048576
			)

target_sources(app PRIVATE src/main.c)
