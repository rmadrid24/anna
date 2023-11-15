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
FROM anna-kvs-deps

#MAINTAINER Vikram Sreekanti <vsreekanti@gmail..com> version: 0.1

ARG repo_org=rmadrid24
ARG source_branch=anna-test
ARG build_branch=docker_build

USER root

# Check out to the appropriate branch on the appropriate fork of the repository
# and build Anna.
RUN echo "/usr/local/gcc-10/lib" >> /etc/ld.so.conf.d/libc.conf
RUN echo "/usr/local/gcc-10/lib64" >> /etc/ld.so.conf.d/libc.conf && ldconfig
WORKDIR $HYDRO_HOME/anna
RUN gcc --version && g++ --version && c++ --version
RUN git remote remove origin && git remote add origin https://github.com/$repo_org/anna
RUN git fetch origin && git checkout -b $build_branch origin/$source_branch
RUN git submodule sync && git submodule update
RUN bash scripts/build.sh -g -j4 -bRelease
#RUN apt install -y iproute2
WORKDIR /

COPY start-anna.sh /
CMD bash start-anna.sh $SERVER_TYPE
