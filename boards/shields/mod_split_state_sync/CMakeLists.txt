# Split state sync: layer/modifier broadcasts and targeted battery snapshots
# over ZMK's existing behaviour transport (no ZMK core changes).

zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR})

zephyr_library_sources(src/sync_state.c)
zephyr_library_sources(src/device_identity.c)
zephyr_library_sources(src/behavior_sync_mods.c)
zephyr_library_sources(src/behavior_sync_layer.c)
zephyr_library_sources(src/sync_battery.c)
zephyr_library_sources(src/behavior_sync_battery.c)

if(CONFIG_SPLIT_STATE_SYNC_DEVICE_DONGLE)
  zephyr_library_sources(src/central_battery.c)
endif()

if(CONFIG_SPLIT_STATE_SYNC_DEVICE_RIGHT)
  zephyr_library_sources(src/battery_service.c)
endif()

if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(src/central_sync.c)
endif()
