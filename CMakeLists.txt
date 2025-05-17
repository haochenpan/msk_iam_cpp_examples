cmake_minimum_required(VERSION 3.16)
project(msk_iam_examples)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKA REQUIRED rdkafka++ rdkafka)

find_package(AWSSDK REQUIRED COMPONENTS core)
find_package(OpenSSL REQUIRED)

add_executable(producer
    src/producer.cpp
    src/AwsMskIamSigner.cpp
)

target_include_directories(producer PRIVATE src)
message(STATUS "RDKAFKA_INCLUDE_DIRS: ${RDKAFKA_INCLUDE_DIRS}")

target_include_directories(producer PRIVATE ${RDKAFKA_INCLUDE_DIRS} /usr/include/librdkafka)

target_link_libraries(producer
    PRIVATE
        aws-cpp-sdk-core
        OpenSSL::SSL
        OpenSSL::Crypto
        ${RDKAFKA_LIBRARIES}
)

add_executable(consumer
    src/consumer.cpp
    src/AwsMskIamSigner.cpp
)

target_include_directories(consumer PRIVATE src ${RDKAFKA_INCLUDE_DIRS} /usr/include/librdkafka)

target_link_libraries(consumer
    PRIVATE
        aws-cpp-sdk-core
        OpenSSL::SSL
        OpenSSL::Crypto
        ${RDKAFKA_LIBRARIES}
)
