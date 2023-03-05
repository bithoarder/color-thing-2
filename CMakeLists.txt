cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(color-thing-2)

pico_sdk_init()

add_executable(color-thing-2)
target_sources(color-thing-2 PUBLIC main.cpp light-vm.cpp usb_descriptors.c)

target_include_directories(color-thing-2 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

#pico_enable_stdio_uart(color-thing-2 1)
#pico_enable_stdio_usb(color-thing-2 0)

target_link_libraries(color-thing-2 PUBLIC pico_stdlib tinyusb_device tinyusb_board)

#pico_add_extra_outputs(color-thing-2)

#pico_set_binary_type(color-thing-2 no_flash)
#add_custom_target(run
#        COMMAND openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "init" -c "reset" -c "reset halt" -c "load_image $<TARGET_FILE:color-thing-2>" -c "resume 0x20000000" -c "exit"
#        )

add_custom_target(run
        COMMAND openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "init" -c "program $<TARGET_FILE:color-thing-2>" -c "reset" -c "exit"
        )
