#
# Docker build environment for LLPM (Low Level Physical Machine) project
#

FROM ubuntu:14.10

MAINTAINER "John Demme" <me@teqdruid.com>

RUN apt-get update
RUN apt-get -y dist-upgrade
RUN apt-get -y install build-essential
RUN apt-get install -y sudo lsb-release git scons
RUN apt-get install -y clang-3.5 
RUN apt-get install -y libboost-all-dev libtinyxml2-dev
RUN apt-get install -y doxygen autoconf automake
RUN apt-get install -y flex bison groff
RUN apt-get install -y graphviz
RUN mkdir /home/user
RUN useradd -d /home/user -m -s /bin/bash user
RUN chown -R user:user /home/user
RUN echo 'user ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers.d/user
RUN chmod 0440 /etc/sudoers.d/user

RUN mkdir /src
RUN chown user:user /src

WORKDIR /src
USER user
CMD /bin/bash

