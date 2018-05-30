if(CONFIG_WIFI_UWP360)
  zephyr_include_directories(./)

  zephyr_sources(
	cmdevt.c
	wifi_txrx.c
	wifi_ipc.c
    wifi_uwp360.c
    )
endif()
