# Split state sync: shared layer/modifier state broadcast from the central to
# peripherals via GLOBAL behaviours (no ZMK core changes).

zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR})

zephyr_library_sources(src/sync_state.c)
zephyr_library_sources(src/behavior_sync_mods.c)
zephyr_library_sources(src/behavior_sync_layer.c)

if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(src/central_sync.c)
endif()