# Use Octopus in C++ on Ubuntu 22.04

This guide walks through building and running a C++ Kafka ([Octopus](https://arxiv.org/pdf/2407.11432)) producer and consumer with MSK IAM authentication using Octopus.

## 1. Prerequisites

Install required system packages:

```bash
sudo apt-get update
sudo apt-get install -y libssl-dev libsasl2-dev build-essential \
                        libcurl4-openssl-dev cmake pkg-config \
                        libboost-all-dev zlib1g-dev g++
```

Check CMake version
```bash
cmake --version
```

Check C++17 support
```bash
g++ -std=c++17 -dM -E - < /dev/null | grep __cplusplus
```


## 2. Install librdkafka from Source

```bash
git clone https://github.com/confluentinc/librdkafka.git
cd librdkafka
git checkout v2.10.0 # the most recent version
./configure
make -j$(nproc)
sudo make install
```

Verify installation:

```bash
ls /usr/local/lib | grep kafka
ls /usr/local/include/librdkafka/rdkafka.h
ls /usr/local/include/librdkafka/rdkafkacpp.h
find /usr/include -name "rdkafka*.h"
ldconfig -p | grep rdkafka
```


## 3. Install AWS SDK for C++

```bash
cd ~
git clone https://github.com/aws/aws-sdk-cpp.git
cd aws-sdk-cpp
git submodule update --init --recursive
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_ONLY="sts;ec2" \
         -DENABLE_UNITY_BUILD=ON \
         -DBUILD_SHARED_LIBS=ON \
         -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

Verify installation:

```bash
ls /usr/local/lib | grep aws-cpp-sdk
```

---

## 4. Environment Setup

Before running the producer or consumer, make sure the following environment variables are set:

```bash
export AWS_ACCESS_KEY_ID="your-access-key-id"
export AWS_SECRET_ACCESS_KEY="your-secret-access-key"
export AWS_REGION="us-east-1"
export OCTOPUS_BOOTSTRAP_SERVERS="b-1-public.diaspora.jdqn8o.c18.kafka.us-east-1.amazonaws.com:9198,b-2-public.diaspora.jdqn8o.c18.kafka.us-east-1.amazonaws.com:9198"
```

---


## 5. Build and Run

### Build the project

```bash
cd /path/to/msk_iam_cpp_examples
mkdir -p build && cd build
cmake ..
make -j
```

### Run the producer

```bash
./producer <topic> "<your-message>"
```

Example:

```bash
./producer tomo-alpha-1 "hello world"
```

### Run the consumer

```bash
./consumer <topic> <group.id> [earliest|latest]
```

If `earliest` or `latest` is not specified, the default is `latest`.

Example:

```bash
./consumer tomo-alpha-1 group-1 earliest
```

Make sure the environment variables listed in section 4 are set before running either binary.

## A. Troubleshooting

To debug MSK IAM token generation, you can enable a debug print in `AwsMskIamSigner.cpp` to inspect the base64-encoded SASL token and the corresponding presigned URL:

```cpp
std::cout << "Encoded token: " << encoded << " (full URL: " << full_url << ")" << std::endl;
```

## B. References

- Based on [Proton's AwsMskIamSigner](https://github.com/timeplus-io/proton/blob/develop/src/IO/Kafka/AwsMskIamSigner.cpp)
- Producer example from [librdkafka](https://github.com/confluentinc/librdkafka/blob/master/examples/producer.c)