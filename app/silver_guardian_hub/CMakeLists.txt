# Silver Guardian Hub - CMakeLists.txt
#
# Licensed under the Apache License, Version 2.0

if(CONFIG_LVX_USE_DEMO_CONTEST2026_325_SILVER_GUARDIAN_HUB)
  set(SGH_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  set(INCDIR ${SGH_DIR})
  set(PROGNAME silver_guardian_hub)
  set(PRIORITY 100)
  set(STACKSIZE 32768)
  set(MODULE ${CONFIG_LVX_USE_DEMO_CONTEST2026_325_SILVER_GUARDIAN_HUB})
  set(MAINSRC src/main.c)

  file(GLOB_RECURSE CSRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.c")
  list(REMOVE_ITEM CSRCS src/main.c)

  nuttx_add_application(
    NAME ${PROGNAME}
    PRIORITY ${PRIORITY}
    STACKSIZE ${STACKSIZE}
    MODULE ${MODULE}
    INCLUDE_DIRECTORIES ${INCDIR}
    SRCS ${MAINSRC} ${CSRCS}
  )
endif()
