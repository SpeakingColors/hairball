cmake_minimum_required(VERSION 3.22)
project(Hairball VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core tests (no JUCE needed): ctest, or run tests/run.sh
add_executable(hairball_tests tests/test_core.cpp)
target_include_directories(hairball_tests PRIVATE src)
enable_testing()
add_test(NAME core COMMAND hairball_tests)

# Plugin. Pass -DHAIRBALL_JUCE_DIR=/path/to/JUCE to use a local JUCE, otherwise JUCE is fetched.
option(HAIRBALL_BUILD_PLUGIN "Build the VST3 plugins" ON)
if(HAIRBALL_BUILD_PLUGIN)
  if(HAIRBALL_JUCE_DIR)
    add_subdirectory(${HAIRBALL_JUCE_DIR} juce)
  else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()
  include(FetchContent)
  # shared look: PurrOne wood, cat head, render gate (pinned to a commit so builds do not change under us)
  if(HAIRBALL_CATUI_DIR)
    add_subdirectory(${HAIRBALL_CATUI_DIR} cat_ui)
  else()
    FetchContent_Declare(cat_ui GIT_REPOSITORY https://github.com/SpeakingColors/cat-ui GIT_TAG 7d72ca7449234157989d76eb544775c067788872)
    FetchContent_MakeAvailable(cat_ui)
  endif()

  # Two plugins from the same code. Hosts treat instruments and effects differently (an effect cannot be loaded on a MIDI track
  # and an instrument cannot sit on an audio track in some hosts), so each gets its own entry. Hairball FX starts in FX mode.
  function(hairball_plugin target product code is_synth categories formats fx)
    juce_add_plugin(${target}
      COMPANY_NAME "Studio 87"
      PLUGIN_MANUFACTURER_CODE Spkc
      PLUGIN_CODE ${code}
      FORMATS ${formats}
      PRODUCT_NAME "${product}"
      BUNDLE_ID io.github.speakingcolors.${target}
      IS_SYNTH ${is_synth}
      NEEDS_MIDI_INPUT TRUE
      NEEDS_MIDI_OUTPUT FALSE
      VST3_CATEGORIES ${categories}
      COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${target} PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(${target} PRIVATE src)
    target_compile_definitions(${target} PUBLIC
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 HB_FX_BUILD=${fx})
    target_link_libraries(${target} PRIVATE cat_ui juce::juce_audio_utils
      PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  endfunction()

  hairball_plugin(Hairball "Hairball" HaiB TRUE "Instrument Synth" "VST3;Standalone" 0)
  hairball_plugin(HairballFX "Hairball FX" HaiF FALSE "Fx" "VST3" 1)
endif()
