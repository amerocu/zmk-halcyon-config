if(CONFIG_ZMK_DISPLAY AND CONFIG_HALCYON_EPAPER_WIDGET_STATUS)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  # Shared split_state.h from the mod_split_state_sync shield.
  zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../mod_split_state_sync)

  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(widgets/bolt.c)
  zephyr_library_sources(widgets/icons.c)
  zephyr_library_sources(widgets/mod_lock.c)
  zephyr_library_sources(widgets/util.c)

  if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_library_sources(widgets/status.c)
  else()
    zephyr_library_sources(widgets/peripheral_status.c)
  endif()
endif()