cmake_minimum_required(VERSION 3.31)
project(StatusServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_WIN32_WINNT=0x0A00)

#set(CMAKE_FIND_DEBUG_MODE TRUE)
file(GLOB Headers "E:/Clion_Code/StatusServer/headers/*.h")
file(GLOB Sources "E:/Clion_Code/StatusServer/sources/*.cpp")
file(GLOB Sources_cc "E:/Clion_Code/StatusServer/sources/*.cc")

find_package(grpc CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)

if (MSVC)
    add_compile_options(/utf-8)
endif()
add_executable(StatusServer  # 这个是StatusServer的启动项目
        ${Headers}
        ${Sources}
        ${Sources_cc}
)

# Boost 路径
target_include_directories(StatusServer PRIVATE
        "E:/CLion 2024.3.1.1/Boost_MSVC"
        "E:/Clion_Code/Socket/libjson/include"
        "E:/MySQL-connector-c++/mysql-connector-c++-9.6.0-winx64/include"
)

target_link_directories(StatusServer PRIVATE
        "E:/CLion 2024.3.1.1/Boost_MSVC/stage/lib"
        "E:/Clion_Code/Socket/libjson/lib"
        "E:/MySQL-connector-c++/mysql-connector-c++-9.6.0-winx64/lib64/vs14"
)

target_link_libraries(StatusServer PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
        redis++::redis++
        rapidjson
        "E:/Clion_Code/Socket/libjson/lib/jsoncpp.lib"
        "E:/MySQL-connector-c++/mysql-connector-c++-9.6.0-winx64/lib64/vs14/mysqlcppconn.lib"
)



if (WIN32)

    add_custom_command(TARGET StatusServer
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.ini
            $<TARGET_FILE_DIR:StatusServer>
    )

    add_custom_command(TARGET StatusServer
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "E:/MySQL-connector-c++/mysql-connector-c++-9.6.0-winx64/lib64/mysqlcppconn-10-vs14.dll"
            $<TARGET_FILE_DIR:StatusServer>
    )

endif()