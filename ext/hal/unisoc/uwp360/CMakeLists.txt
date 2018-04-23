zephyr_include_directories(
		drivers/include
		)
zephyr_sources(soc/system.c)
zephyr_sources_ifdef(CONFIG_USE_UWP360_HAL_UART drivers/src/hal_uart.c)
