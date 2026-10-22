add_compile_definitions(MM_TESTING)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include_directories(SYSTEM ${GLM_INCLUDE_DIR})
include_directories(SYSTEM ${IMMER_INCLUDE_DIR})

if(MMAPPER_IS_DEBUG)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wno-extra-semi-stmt
            -Wno-missing-noreturn
            -Wno-unreachable-code-return
            -Wno-unused-const-variable
            -Wno-unused-function
            -Wno-unused-parameter
            -Wno-unused-template
            -Wno-unused-variable
            -Werror=unused-result
        )
    endif()
endif()

# Glyph atlas
set(font_SRCS
    ../src/font/GlyphAtlas.cpp
    ../src/font/GlyphAtlas.h
)
set(TestFont_SRCS TestFont.cpp)
add_executable(TestFont ${TestFont_SRCS} ${font_SRCS})
add_dependencies(TestFont mm_test mm_global)
target_link_libraries(TestFont
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestFont PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestFont COMMAND TestFont)

# Common test library
add_library(mm_test STATIC
    ../src/configuration/configuration.cpp
    ../src/configuration/configuration.h
    ../src/configuration/GroupConfig.cpp
    ../src/configuration/GroupConfig.h
    ../src/observer/gameobserver.cpp
    ../src/observer/gameobserver.h
    ../src/proxy/GmcpMessage.cpp
    ../src/proxy/GmcpMessage.h
)
set_target_properties(mm_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_FLAGS "${WARNING_FLAGS}"
)
target_link_libraries(mm_test
    mm_global
    Qt6::Core
    Qt6::Network
    coverage_config
)

# Clock
set(clock_SRCS
    ../src/clock/ClockStrings.cpp
    ../src/clock/ClockStrings.h
    ../src/clock/mumeclock.cpp
    ../src/clock/mumeclock.h
    ../src/clock/mumemoment.cpp
    ../src/clock/mumemoment.h
    )
set(TestClock_SRCS testclock.cpp)
add_executable(TestClock ${TestClock_SRCS} ${clock_SRCS})
add_dependencies(TestClock mm_test mm_global)
target_link_libraries(TestClock
        mm_test
        mm_global
        Qt6::Network
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestClock PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestClock COMMAND TestClock)

# Expandora
set(expandoracommon_SRCS
    ../src/parser/Abbrev.cpp
    )
set(TestExpandoraCommon_SRCS testexpandoracommon.cpp)
add_executable(TestExpandoraCommon ${TestExpandoraCommon_SRCS} ${expandoracommon_SRCS})
add_dependencies(TestExpandoraCommon mm_test mm_global mm_map)
target_link_libraries(TestExpandoraCommon
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestExpandoraCommon PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestExpandoraCommon COMMAND TestExpandoraCommon)

# Parser
set(TestParser_SRCS testparser.cpp)
add_executable(TestParser ${TestParser_SRCS})
add_dependencies(TestParser mm_test mm_global mm_map)
target_link_libraries(TestParser
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestParser PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestParser COMMAND TestParser)

# Proxy
set(proxy_SRCS
    ../src/proxy/GmcpModule.cpp
    ../src/proxy/GmcpModule.h
    ../src/proxy/GmcpUtils.cpp
    ../src/proxy/GmcpUtils.h
    ../src/proxy/telnetfilter.cpp
    ../src/proxy/telnetfilter.h
     )
set(TestProxy_SRCS TestProxy.cpp)
add_executable(TestProxy ${TestProxy_SRCS} ${proxy_SRCS})
add_dependencies(TestProxy mm_test mm_global)
target_link_libraries(TestProxy mm_test mm_global Qt6::Test coverage_config)
set_target_properties(
  TestProxy PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestProxy COMMAND TestProxy)

# MapScroller is a plain, rendering-independent QObject (see
# ../src/display/MapScroller.h's file comment: deliberately has no
# dependency on MapCanvas/OpenGL), so it's linked directly into
# TestMainWindow below for mapScrollerScrollMath().
set(map_scroller_SRCS
    ../src/display/MapScroller.cpp
    ../src/display/MapScroller.h
)
# MainWindow
set(mainwindow_SRCS
    ../src/global/Version.h
    ../src/mainwindow/AudioVolumeSlider.cpp
    ../src/mainwindow/AudioVolumeSlider.h
    ../src/mainwindow/UpdateChecker.cpp
    ../src/mainwindow/UpdateChecker.h
    )
set(TestMainWindow_SRCS TestMainWindow.cpp)
add_executable(TestMainWindow ${TestMainWindow_SRCS} ${mainwindow_SRCS} ${map_scroller_SRCS})
add_dependencies(TestMainWindow mm_test mm_global mm_map)
target_link_libraries(TestMainWindow
        mm_map
        mm_test
        mm_global
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        Qt6::OpenGL
        coverage_config)
set_target_properties(
  TestMainWindow PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMainWindow COMMAND TestMainWindow)

# Global
set(TestGlobal_SRCS TestGlobal.cpp)
add_executable(TestGlobal ${TestGlobal_SRCS})
add_dependencies(TestGlobal mm_global)
target_link_libraries(TestGlobal
        mm_global
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestGlobal PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGlobal COMMAND TestGlobal)

# Group
set(group_SRCS
        ../src/group/CGroupChar.cpp
        ../src/group/CGroupChar.h
        ../src/group/GroupModel.cpp
        ../src/group/GroupModel.h
        ../src/group/enums.cpp
        ../src/group/enums.h
        ../src/group/mmapper2character.cpp
        ../src/group/mmapper2character.h
)
set(TestGroup_SRCS ${group_SRCS} TestGroup.cpp)
add_executable(TestGroup ${TestGroup_SRCS})
add_dependencies(TestGroup mm_test mm_global mm_map)
target_link_libraries(TestGroup
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
        TestGroup PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGroup COMMAND TestGroup)

# Map
set(TestMap_SRCS
        TestMap.cpp
)
add_executable(TestMap ${TestMap_SRCS})
add_dependencies(TestMap mm_test mm_global mm_map)
target_link_libraries(TestMap
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
        TestMap PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMap COMMAND TestMap)

# Adventure
set(adventure_SRCS
        ../src/adventure/AdventureLogModel.cpp
        ../src/adventure/AdventureLogModel.h
        ../src/adventure/adventuresession.cpp
        ../src/adventure/adventuresession.h
        ../src/adventure/adventuretracker.cpp
        ../src/adventure/adventuretracker.h
        ../src/adventure/lineparsers.cpp
        ../src/adventure/lineparsers.h
        )
set(TestAdventure_SRCS testadventure.cpp testadventure.h)
add_executable(TestAdventure ${TestAdventure_SRCS} ${adventure_SRCS})
add_dependencies(TestAdventure mm_test mm_global)
target_link_libraries(TestAdventure mm_test mm_global Qt6::Widgets Qt6::Network Qt6::Test coverage_config)
set_target_properties(
        TestAdventure PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestAdventure COMMAND TestAdventure)

# AutoLogger
set(autologger_test
        ../src/logger/LogCleanup.cpp
        ../src/logger/LogCleanup.h
)
set(TestAutoLogger TestAutoLogger.cpp TestAutoLogger.h)
add_executable(TestAutoLogger ${autologger_test} ${TestAutoLogger})
add_dependencies(TestAutoLogger mm_global)
target_link_libraries(TestAutoLogger mm_global Qt6::Test coverage_config)
set_target_properties(
        TestAutoLogger PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestAutoLogger COMMAND TestAutoLogger)

# Timer
set(timer_test
        ../src/timers/CTimers.cpp
        ../src/timers/CTimers.h
        ../src/timers/TimerModel.cpp
        ../src/timers/TimerModel.h
)
set(TestTimer TestCTimers.cpp)
add_executable(TestTimer ${timer_test} ${TestTimer})
add_dependencies(TestTimer mm_global)
target_link_libraries(TestTimer mm_global Qt6::Test coverage_config)
set_target_properties(
        TestTimer PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestTimer COMMAND TestTimer)

# Room panel mob
set(room_mob
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
)
set(RoomMob roompanel/TestRoomMob.cpp)
add_executable(TestRoomMob ${room_mob} ${RoomMob})
add_dependencies(TestRoomMob mm_global)
target_link_libraries(TestRoomMob mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
set_target_properties(
        TestRoomMob PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomMob COMMAND TestRoomMob)


# Room panel mobs
set(room_mobs
        ../src/roompanel/RoomMobs.h
        ../src/roompanel/RoomMobs.cpp
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
)
set(RoomMobs roompanel/TestRoomMobs.cpp)
add_executable(TestRoomMobs ${room_mobs} ${RoomMobs})
add_dependencies(TestRoomMobs mm_global)
target_link_libraries(TestRoomMobs mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
set_target_properties(
        TestRoomMobs PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomMobs COMMAND TestRoomMobs)

# Room panel manager
set(room_manager
        ../src/roompanel/RoomMobs.h
        ../src/roompanel/RoomMobs.cpp
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
        ../src/roompanel/RoomManager.cpp
        ../src/roompanel/RoomManager.h
        ../src/roompanel/RoomModel.cpp
        ../src/roompanel/RoomModel.h
)
set(RoomManager roompanel/TestRoomManager.cpp)
add_executable(TestRoomManager ${room_manager} ${RoomManager})
add_dependencies(TestRoomManager mm_test mm_global)
target_link_libraries(TestRoomManager mm_test mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
set_target_properties(
        TestRoomManager PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomManager COMMAND TestRoomManager)

# HotkeyManager
set(hotkey_manager_SRCS
        ../src/client/Hotkey.cpp
        ../src/client/Hotkey.h
        ../src/client/HotkeyManager.cpp
        ../src/client/HotkeyManager.h
)
set(TestHotkeyManager_SRCS TestHotkeyManager.cpp TestHotkeyManager.h)
add_executable(TestHotkeyManager ${TestHotkeyManager_SRCS} ${hotkey_manager_SRCS})
add_dependencies(TestHotkeyManager mm_test mm_global mm_map)
target_link_libraries(TestHotkeyManager mm_map mm_test mm_global Qt6::Test Qt6::Network coverage_config)
set_target_properties(
        TestHotkeyManager PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestHotkeyManager COMMAND TestHotkeyManager)

# Media
set(description_panel_SRCS
        ../src/media/StackBlur.cpp
        ../src/media/StackBlur.h
)
set(TestMedia_SRCS TestMedia.cpp TestMedia.h)
add_executable(TestMedia ${TestMedia_SRCS} ${description_panel_SRCS})
add_dependencies(TestMedia mm_test mm_global)
target_link_libraries(TestMedia mm_test mm_global Qt6::Gui Qt6::Test coverage_config)
set_target_properties(
        TestMedia PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMedia COMMAND TestMedia)

# Client
set(TestClient_SRCS
    TestClient.cpp
    TestClient.h
    ../src/client/InputHistory.cpp
    ../src/client/InputHistory.h
    ../src/mpi/RemoteEditDocumentOps.cpp
    ../src/mpi/RemoteEditDocumentOps.h
)
add_executable(TestClient ${TestClient_SRCS})
add_dependencies(TestClient mm_test mm_global)
target_link_libraries(TestClient mm_test mm_global Qt6::Gui Qt6::Test coverage_config)
set_target_properties(
        TestClient PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestClient COMMAND TestClient)

# Terrain placement has no Qt or GL dependency and can run without a display.
add_executable(TestTerrainSprites TestTerrainSprites.cpp)
target_link_libraries(TestTerrainSprites PRIVATE coverage_config)
set_target_properties(TestTerrainSprites PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainSprites COMMAND TestTerrainSprites)

add_executable(TestTerrainEdges TestTerrainEdges.cpp)
target_link_libraries(TestTerrainEdges PRIVATE coverage_config)
set_target_properties(TestTerrainEdges PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainEdges COMMAND TestTerrainEdges)

# Pack selection and cell-local mipmaps use Qt images but require no GL/display.
add_executable(TestTerrainSpritePacks TestTerrainSpritePacks.cpp ../src/display/TerrainSpritePacks.cpp)
target_link_libraries(TestTerrainSpritePacks PRIVATE Qt6::Gui coverage_config)
set_target_properties(TestTerrainSpritePacks PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainSpritePacks COMMAND TestTerrainSpritePacks)

add_executable(TestTerrainWalls TestTerrainWalls.cpp ../src/display/TerrainSpritePacks.cpp)
target_compile_definitions(TestTerrainWalls PRIVATE TERRAIN_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/resources/pixmaps")
target_link_libraries(TestTerrainWalls PRIVATE Qt6::Gui coverage_config)
set_target_properties(TestTerrainWalls PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainWalls COMMAND TestTerrainWalls)

# Static door variants require no map or OpenGL context.
add_executable(TestTerrainDoors TestTerrainDoors.cpp)
target_link_libraries(TestTerrainDoors PRIVATE Qt6::Core coverage_config)
set_target_properties(TestTerrainDoors PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainDoors COMMAND TestTerrainDoors)

add_executable(TestTerrainDoorState TestTerrainDoorState.cpp ../src/display/TerrainDoorState.cpp)
target_link_libraries(TestTerrainDoorState PRIVATE mm_test Qt6::Core coverage_config)
set_target_properties(TestTerrainDoorState PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainDoorState COMMAND TestTerrainDoorState)

add_executable(TestTerrainTwilight TestTerrainTwilight.cpp)
target_link_libraries(TestTerrainTwilight PRIVATE coverage_config)
set_target_properties(TestTerrainTwilight PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainTwilight COMMAND TestTerrainTwilight)

add_executable(TestTerrainPreviewClock TestTerrainPreviewClock.cpp
    ../src/mainwindow/TerrainPreviewClock.cpp ${clock_SRCS})
add_dependencies(TestTerrainPreviewClock mm_test mm_global)
target_link_libraries(TestTerrainPreviewClock PRIVATE mm_test mm_global
    Qt6::Network Qt6::Gui Qt6::Test Qt6::Widgets coverage_config)
set_target_properties(TestTerrainPreviewClock PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainPreviewClock COMMAND TestTerrainPreviewClock)

add_executable(TestTerrainRoomLighting TestTerrainRoomLighting.cpp
    ../src/display/TerrainRoomLightRules.cpp ../src/display/TerrainLightMask.cpp)
target_compile_definitions(TestTerrainRoomLighting PRIVATE TERRAIN_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/resources/pixmaps")
target_link_libraries(TestTerrainRoomLighting PRIVATE Qt6::Gui coverage_config)
set_target_properties(TestTerrainRoomLighting PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainRoomLighting COMMAND TestTerrainRoomLighting)

add_executable(TestTerrainLightVisibility TestTerrainLightVisibility.cpp
    ../src/display/TerrainLightVisibility.cpp ../src/display/TerrainLightMask.cpp
    ../src/display/TerrainRoomLightRules.cpp
    ../src/map/coordinate.cpp ../src/global/utils.cpp)
target_compile_definitions(TestTerrainLightVisibility PRIVATE TERRAIN_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/resources/pixmaps")
target_link_libraries(TestTerrainLightVisibility PRIVATE Qt6::Gui coverage_config)
set_target_properties(TestTerrainLightVisibility PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainLightVisibility COMMAND TestTerrainLightVisibility)

add_executable(TestTerrainMovementDiagnostics TestTerrainMovementDiagnostics.cpp
    ../src/display/TerrainLightingDiagnostics.cpp)
target_link_libraries(TestTerrainMovementDiagnostics PRIVATE Qt6::Core coverage_config)
set_target_properties(TestTerrainMovementDiagnostics PROPERTIES COMPILE_FLAGS "${WARNING_FLAGS}")
add_test(NAME TestTerrainMovementDiagnostics COMMAND TestTerrainMovementDiagnostics)
