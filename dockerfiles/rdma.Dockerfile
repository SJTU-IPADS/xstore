FROM ubuntu:16.04

# Set MOFED version, OS version and platform
ENV MOFED_VERSION 5.1-2.3.7.1
ENV OS_VERSION ubuntu16.04
ENV PLATFORM x86_64
ENV OFED https://www.mellanox.com/downloads/ofed/MLNX_OFED-5.1-2.3.7.1/MLNX_OFED_LINUX-5.1-2.3.7.1-ubuntu16.04-x86_64.tgz
ENV OFED_FILE MLNX_OFED_LINUX-5.1-2.3.7.1-ubuntu16.04-x86_64.tgz
ENV OFED_PATH MLNX_OFED_LINUX-5.1-2.3.7.1-ubuntu16.04-x86_64

RUN apt-get update
RUN apt-get -y install apt-utils
RUN apt-get install -y --allow-downgrades --allow-change-held-packages --no-install-recommends \
        build-essential cmake tcsh tcl tk \
	        make git curl vim wget ca-certificates \
		        iputils-ping net-tools ethtool \
			        perl lsb-release python-libxml2 \
				        iproute2 pciutils libnl-route-3-200 \
					        kmod libnuma1 lsof openssh-server \
						        swig libelf1 automake libglib2.0-0 \
							        autoconf graphviz chrpath flex libnl-3-200 m4 \
								        debhelper autotools-dev gfortran libltdl-dev && \
									        rm -rf /rm -rf /var/lib/apt/lists/*

# Download and install Mellanox OFED 4.5-1.0.1.0 for Ubuntu 16.04

#RUN wget --quiet https://www.mellanox.com/downloads/ofed/MLNX_OFED-${MOFED_VER}/MLNX_OFED_LINUX-${MOFED_VER}-${OS_VER}-${PLATFORM}.tgz && \
RUN wget --quiet ${OFED} && \
    tar -xvf ${OFED_FILE} && \
        ${OFED_PATH}/mlnxofedinstall --user-space-only --without-fw-update -q && \
	    cd .. && \
	        rm -rf ${MOFED_DIR} && \
		    rm -rf *.tgz

# Allow OpenSSH to talk to containers without asking for confirmation
RUN cat /etc/ssh/ssh_config | grep -v StrictHostKeyChecking > /etc/ssh/ssh_config.new && \
    echo "    StrictHostKeyChecking no" >> /etc/ssh/ssh_config.new && \
        mv /etc/ssh/ssh_config.new /etc/ssh/ssh_config