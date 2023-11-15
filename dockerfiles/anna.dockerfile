#  Copyright 2018 U.C. Berkeley RISE Lab
# 
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

#FROM hydroproject/base:latest
FROM anna-kvs-gcc13

#MAINTAINER Vikram Sreekanti <vsreekanti@gmail..com> version: 0.1

ARG repo_org=rmadrid24
ARG source_branch=anna-test
ARG build_branch=docker_build

USER root

# Check out to the appropriate branch on the appropriate fork of the repository
# and build Anna.
RUN update-alternatives --install /usr/bin/gcc gcc /usr/local/gcc-10/bin/gcc 10 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/local/gcc-10/bin/g++ 10 && \
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/gcc-10/lib:/usr/local/gcc-10/lib64 && \
    git clone https://gitlab.kitware.com/cmake/cmake.git && cd cmake && \
    git checkout tags/v3.13.0 && ./bootstrap && make && make install && hash -r && cd .. && \
    git clone https://github.com/oneapi-src/oneTBB.git && cd oneTBB && \
    mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ../ && make && make install && cd ..

RUN git clone https://github.com/protocolbuffers/protobuf.git && \
    cd protobuf/ && git checkout v3.9.1 && git submodule update --init --recursive && \
    ./autogen.sh && ./configure && make && make install && ldconfig

#WORKDIR $HYDRO_HOME/anna
#RUN git remote remove origin && git remote add origin https://github.com/$repo_org/anna
#RUN git fetch origin && git checkout -b $build_branch origin/$source_branch
#RUN git submodule sync && git submodule update
#RUN bash scripts/build.sh -j4 -bRelease
#RUN apt install -y iproute2
WORKDIR /

#COPY start-anna.sh /
#CMD bash start-anna.sh $SERVER_TYPE
