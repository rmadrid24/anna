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
RUN apt install -y asciidoctor glib-2.0 libfabric-dev pandoc libkmod-dev libudev-dev uuid-dev libjson-c-dev libkeyutils-dev bash-completion bc systemd
RUN git clone https://github.com/pmem/ndctl.git && cd ndctl && git checkout origin/ndctl-70.y \
    && ./autogen.sh \
    && ./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib64 \
    && make && make install && cd ../
RUN git clone https://github.com/pmem/pmdk && cd pmdk && git checkout tags/1.10.1 \
    && export PKG_CONFIG_PATH=/usr/lib64/pkgconfig \
    && make && make install && cd ..
