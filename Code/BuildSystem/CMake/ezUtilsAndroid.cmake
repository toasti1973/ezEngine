######################################
### ez_list_subdirs(result curdir)
######################################

macro(ez_list_subdirs result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

######################################
### ez_android_add_default_content(<target>)
######################################
function(ez_android_add_default_content TARGET_NAME)

  ez_pull_all_vars()

  if (NOT EZ_CMAKE_PLATFORM_ANDROID)
    return()
  endif()
    
  get_property(EZ_SUBMODULE_PREFIX_PATH GLOBAL PROPERTY EZ_SUBMODULE_PREFIX_PATH)

  set(CONTENT_DIRECTORY_DST "${CMAKE_CURRENT_BINARY_DIR}/package")
  set(CONTENT_DIRECTORY_SRC "${CMAKE_SOURCE_DIR}/${EZ_SUBMODULE_PREFIX_PATH}/Data/Platform/Android")

  # Copy content files.
  set(ANDROID_ASSET_NAMES
      "res/mipmap-hdpi/ic_launcher.png"
      "res/mipmap-mdpi/ic_launcher.png"
      "res/mipmap-xhdpi/ic_launcher.png"
      "res/mipmap-xxhdpi/ic_launcher.png")

  foreach(contentFile ${ANDROID_ASSET_NAMES})
    configure_file(${CONTENT_DIRECTORY_SRC}/${contentFile} ${CONTENT_DIRECTORY_DST}/${contentFile} COPYONLY)
  endforeach(contentFile)

  # Create Manifest from template.
  configure_file(${CONTENT_DIRECTORY_SRC}/AndroidManifest.xml ${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml)
  configure_file(${CONTENT_DIRECTORY_SRC}/res/values/strings.xml ${CONTENT_DIRECTORY_DST}/res/values/strings.xml)

  if(NOT EXISTS "$ENV{ANDROID_NDK}")
	if(NOT EXISTS "$ENV{ANDROID_NDK_HOME}")
	  message(FATAL_ERROR "Could not find ANDROID_NDK or ANDROID_NDK_HOME environment variables")
	else()
	  set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
	endif()
  else()
    set(ANDROID_NDK $ENV{ANDROID_NDK})
  endif()


  if(NOT EXISTS "$ENV{ANDROID_SDK}")
    if(NOT EXISTS "$ENV{ANDROID_HOME}")
      message(FATAL_ERROR "Could not find ANDROID_SDK or ANDROID_HOME environment variables")
	else()
	  set(ANDROID_SDK $ENV{ANDROID_HOME})
	endif()
  else()
    set(ANDROID_SDK $ENV{ANDROID_SDK})
  endif()

  if(NOT EXISTS "$ENV{ANDROID_BUILD_TOOLS}")
    get_filename_component(ANDROID_BUILD_TOOLS_ROOT "${ANDROID_SDK}/build-tools" ABSOLUTE)
	ez_list_subdirs(AVAILABLE_BUILD_TOOLS ${ANDROID_BUILD_TOOLS_ROOT})
	list(LENGTH AVAILABLE_BUILD_TOOLS NUM_AVAILABLE_BUILD_TOOLS)
	if(NUM_AVAILABLE_BUILD_TOOLS EQUAL 0)
	  message(FATAL_ERROR "Could not find ANDROID_BUILD_TOOLS environment variable and automatic detection failed. (should contain aapt.exe)")
	else()
	  list(SORT AVAILABLE_BUILD_TOOLS)
	  list(REVERSE AVAILABLE_BUILD_TOOLS)
	  list(GET AVAILABLE_BUILD_TOOLS 0 ANDROID_BUILD_TOOLS)
	  set(ANDROID_BUILD_TOOLS "${ANDROID_BUILD_TOOLS_ROOT}/${ANDROID_BUILD_TOOLS}")
	endif()
  else()
    set(ANDROID_BUILD_TOOLS $ENV{ANDROID_BUILD_TOOLS})
  endif()
  
  string(FIND "${ANDROID_PLATFORM}" "android" out)
  if(${out} EQUAL 0)
    set(ANDROID_PLATFORM_ROOT ${ANDROID_SDK}/platforms/${ANDROID_PLATFORM})
  else()
    set(ANDROID_PLATFORM_ROOT ${ANDROID_SDK}/platforms/android-${ANDROID_PLATFORM})
  endif()
  
  if(NOT EXISTS "${ANDROID_PLATFORM_ROOT}")
    message(FATAL_ERROR "Android platform '${ANDROID_PLATFORM}' is not installed. Please use the sdk manager to install it to '${ANDROID_PLATFORM_ROOT}'")
  endif()

  set(_AAPT ${ANDROID_BUILD_TOOLS}/aapt.exe)
  set(_APKSIGNER ${ANDROID_BUILD_TOOLS}/apksigner.bat)
  set(_ZIPALIGN ${ANDROID_BUILD_TOOLS}/zipalign.exe)
  set(_ADB ${ANDROID_SDK}/platform-tools/adb.exe)

  # We can't use generator expressions for BYPRODUCTS so we need to get the default LIBRARY_OUTPUT_DIRECTORY.
  # As the ninja generator does not use generator expressions this is not an issue as LIBRARY_OUTPUT_DIRECTORY
  # matches the build type set in CMAKE_BUILD_TYPE.
  get_target_property(APK_OUTPUT_DIR ${TARGET_NAME} LIBRARY_OUTPUT_DIRECTORY)
 
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    BYPRODUCTS ${APK_OUTPUT_DIR}/${TARGET_NAME}.apk ${APK_OUTPUT_DIR}/${TARGET_NAME}.unaligned.apk
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${CONTENT_DIRECTORY_DST}/lib/${ANDROID_ABI}/lib${TARGET_NAME}.so
    COMMAND ${_AAPT} package --debug-mode -f -M ${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml -S ${CONTENT_DIRECTORY_DST}/res -I "${ANDROID_PLATFORM_ROOT}/android.jar" -F $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.unaligned.apk ${CONTENT_DIRECTORY_DST}
    COMMAND ${_ZIPALIGN} -f 4 $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.unaligned.apk $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.apk
    COMMAND ${_APKSIGNER} sign --ks ${CONTENT_DIRECTORY_SRC}/debug.keystore --ks-pass "pass:android" $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.apk
    )

endfunction()
