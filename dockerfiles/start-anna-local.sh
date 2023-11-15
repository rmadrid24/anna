#!/bin/bash

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

cd $HYDRO_HOME/anna

echo "Starting Anna KVS"

# Tailor the config file to have process specific information.
if [ "$1" = "mn" ]; then
#  echo -e "monitoring:" >> conf/anna-config.yml
#  echo -e "    mgmt_ip: $MGMT_IP" >> conf/anna-config.yml
#  echo -e "    ip: $PRIVATE_IP" >> conf/anna-config.yml

  ./build/target/kvs/anna-monitor
elif [ "$1" = "r" ]; then
#  echo -e "routing:" >> conf/anna-config.yml
#  echo -e "    ip: $PRIVATE_IP" >> conf/anna-config.yml

#  LST=$(gen_yml_list "$MON_IPS")
#  echo -e "    monitoring:" >> conf/anna-config.yml
#  echo -e "$LST" >> conf/anna-config.yml

  ./build/target/kvs/anna-route
elif [ "$1" = "b" ]; then
#  echo -e "user:" >> conf/anna-config.yml
#  echo -e "    ip: $PRIVATE_IP" >> conf/anna-config.yml

#  LST=$(gen_yml_list "$MON_IPS")
#  echo -e "    monitoring:" >> conf/anna-config.yml
#  echo -e "$LST" >> conf/anna-config.yml

#  LST=$(gen_yml_list "$ROUTING_IPS")
#  echo -e "    routing:" >> conf/anna-config.yml
#  echo -e "$LST" >> conf/anna-config.yml

  ./build/target/benchmark/anna-bench
elif [ "$1" = "mem" ]; then
#  echo -e "server:" >> conf/anna-config.yml
#  echo -e "    seed_ip: $SEED_IP" >> conf/anna-config.yml
#  echo -e "    public_ip: $PUBLIC_IP" >> conf/anna-config.yml
#  echo -e "    private_ip: $PRIVATE_IP" >> conf/anna-config.yml
#  echo -e "    mgmt_ip: $MGMT_IP" >> conf/anna-config.yml

#  LST=$(gen_yml_list "$MON_IPS")
#  echo -e "    monitoring:" >> conf/anna-config.yml
#  echo -e "$LST" >> conf/anna-config.yml

#  LST=$(gen_yml_list "$ROUTING_IPS")
#  echo -e "    routing:" >> conf/anna-config.yml
#  echo -e "$LST" >> conf/anna-config.yml
  echo "Start monitor"
  ./build/target/kvs/anna-monitor & MPID=$!
  echo "Start route"
  ./build/target/kvs/anna-route & RPID=$!
  echo "Start kvs"
  export SERVER_TYPE="memory"
  ./build/target/kvs/anna-kvs
elif [ "$1" = "ebs" ]; then
  export SERVER_TYPE="ebs"
  ./build/target/kvs/anna-kvs
fi
