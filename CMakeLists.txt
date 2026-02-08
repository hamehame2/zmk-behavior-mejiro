if (CONFIG_NAGINATA AND ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
  target_sources(app PRIVATE src/behaviors/behavior_naginata.c)
  target_sources(app PRIVATE src/naginata_func.c)
  target_sources(app PRIVATE src/nglist.c)
  target_sources(app PRIVATE src/nglistarray.c)
endif()

zephyr_include_directories(include)
