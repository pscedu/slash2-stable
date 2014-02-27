#!/bin/bash
#
# slash2-ubuntu.sh - install packages needed to build slash2 on ubuntu.
#

sudo apt-get install libssl-dev -y
sudo apt-get install libgcrypt11-dev -y
sudo apt-get install libfuse-dev -y
sudo apt-get install libsqlite3-dev -y
sudo apt-get install libaio-dev -y
sudo apt-get install libncurses5-dev -y
sudo apt-get install libreadline-dev -y

sudo apt-get install bison -y
sudo apt-get install flex -y
sudo apt-get install scons -y
sudo apt-get install screen -y
sudo apt-get install patch -y
sudo apt-get install cscope -y
