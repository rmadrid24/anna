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

USER root

# Check out to the appropriate branch on the appropriate fork of the repository
# and build Anna.
RUN echo "/usr/local/gcc-10/lib" >> /etc/ld.so.conf.d/libc.conf
RUN echo "/usr/local/gcc-10/lib64" >> /etc/ld.so.conf.d/libc.conf && ldconfig
WORKDIR $HYDRO_HOME
RUN rm -rf anna/
RUN mkdir anna
WORKDIR $HYDRO_HOME/anna
COPY client/ client/
COPY common/ common/
COPY conf/ conf/
COPY include/ include/
COPY scripts/ scripts/
COPY src/ src/
COPY tests/ tests/
COPY middleware/ middleware/
COPY HdrHistogram_c/ HdrHistogram_c/
COPY CMakeLists.txt CMakeLists.txt
#COPY ../  anna/
RUN ls
RUN bash scripts/build.sh -g -j4 -bRelease
#RUN apt install -y iproute2
WORKDIR /

COPY dockerfiles/start-anna-local.sh /start-anna.sh
CMD bash start-anna.sh $SERVER_TYPE
