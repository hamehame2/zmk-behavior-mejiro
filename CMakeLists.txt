cmake_minimum_required(VERSION 3.20.0)

zephyr_library()
zephyr_library_sources(
  src/behavior_mejiro.c
  src/mejiro_engine.c
  src/mejiro_commands_zmk.c
  src/mejiro_transform.c
  src/mejiro_verb.c
  src/mejiro_abbreviations.c
)
zephyr_library_include_directories(
  include
  src
)
