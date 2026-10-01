set(agcColorTransferSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/ColorTransfer.comp)
set(agcColorTransferSpv ${agcTextureDetileGeneratedDir}/ColorTransfer.spv)
set(agcColorTransferHeader ${agcTextureDetileGeneratedDir}/ColorTransfer_spv.h)
add_custom_command(
    OUTPUT "${agcColorTransferHeader}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
    COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcColorTransferSpv}" "${agcColorTransferSource}"
    COMMAND ${CMAKE_COMMAND} -DINPUT=${agcColorTransferSpv} -DOUTPUT=${agcColorTransferHeader} -DSYMBOL=COLOR_TRANSFER_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    DEPENDS "${agcColorTransferSource}" glslang-standalone
    VERBATIM
)

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/BufferPool.cpp Graphics/src/CommandCompletion.cpp Graphics/src/ReleaseQueue.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Execution/src/GuestGpuRange.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/TextureDetilerDescriptors.cpp Graphics/src/TextureCache.cpp Graphics/src/RegisteredGpuMemory.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/GpuColorTransfer.cpp Graphics/src/ColorMemoryWriteback.cpp ${agcColorTransferHeader})
    endif()
endforeach()

target_sources(agc_driver_bda_device_tests PRIVATE tests/ColorTransferTests.cpp Graphics/src/ColorTargetLayout.cpp Execution/src/GuestMemory.cpp Graphics/src/GuestBufferCache.cpp)
target_include_directories(agc_driver_bda_device_tests PRIVATE ${agcTextureDetileGeneratedRoot})

if(TARGET agc_driver_visual_test)
    target_sources(agc_driver_visual_test PRIVATE Execution/src/DisplayBuffer.cpp)
endif()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/RenderCache.cpp Graphics/src/RenderMemoryOwnership.cpp Graphics/src/DrawQueue.cpp Graphics/src/DrawCompletion.cpp Graphics/src/RenderTexture.cpp Graphics/src/DescriptorCache.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Execution/src/PresentationImage.cpp Execution/src/DeviceSynchronization.cpp Graphics/src/GraphicsPipelineCache.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/GpuTimestamps.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/CaptureFormat.cpp Graphics/src/CaptureObjects.cpp Graphics/src/DispatchRecorder.cpp Execution/src/DispatchCapture.cpp)
    endif()
endforeach()

set(agcCaptureReplaySource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/CaptureReplay.comp)
set(agcCaptureReplayDir ${agcTextureDetileGeneratedRoot}/prx/libSceAgcDriver/tests/shaders)
set(agcCaptureReplaySpv ${agcCaptureReplayDir}/CaptureReplay.spv)
set(agcCaptureReplayHeader ${agcCaptureReplayDir}/CaptureReplay_spv.h)
add_custom_command(
    OUTPUT "${agcCaptureReplayHeader}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${agcCaptureReplayDir}"
    COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcCaptureReplaySpv}" "${agcCaptureReplaySource}"
    COMMAND ${CMAKE_COMMAND} -DINPUT=${agcCaptureReplaySpv} -DOUTPUT=${agcCaptureReplayHeader} -DSYMBOL=CAPTURE_REPLAY_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    DEPENDS "${agcCaptureReplaySource}" glslang-standalone
    VERBATIM
)
target_sources(agc_driver_bda_device_tests PRIVATE tests/DispatchCaptureTests.cpp Replay/src/DispatchReplay.cpp ${agcCaptureReplayHeader})

add_executable(dispatch_replay EXCLUDE_FROM_ALL
        Replay/src/DispatchReplayMain.cpp
        Replay/src/DispatchReplay.cpp
        Graphics/src/CaptureFormat.cpp
        Execution/src/BdaFeatures.cpp
        Execution/src/VulkanLibrary.cpp
)
target_include_directories(dispatch_replay PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(dispatch_replay PRIVATE shader_recompiler_agc_driver ${ANYPS5_SDL_TARGET} libc)
set_target_properties(dispatch_replay PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(dispatch_replay)
