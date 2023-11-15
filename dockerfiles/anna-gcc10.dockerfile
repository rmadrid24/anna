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

FROM hydroproject/base:latest

USER root

# Check out to the appropriate branch on the appropriate fork of the repository
# and build Anna.
RUN apt update && apt upgrade -y && \
    apt install -y build-essential libgmp-dev libmpfr-dev libmpc-dev flex bison && \
    wget https://ftp.gnu.org/gnu/gcc/gcc-10.4.0/gcc-10.4.0.tar.gz && \
    tar xzf gcc-10.4.0.tar.gz && mkdir build-gcc && cd build-gcc && \
    ../gcc-10.4.0/configure --prefix=/usr/local/gcc-10 --enable-languages=c,c++ --disable-multilib && \
    make -j$(nproc) && make install && \
    cd .. && rm -rf build-gcc
