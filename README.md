# Networking-DPDK: High-Speed Packet Processing with DKDP
A comprehensive project for learning and implementing high-speed packet processing using the Data Plane Development Kit (DPDK), from basic concepts to advanced real-world applications.


### Project Overview
The Data Plane Development Kit (DPDK) is a set of libraries and drivers that enable fast packet processing by bypassing the kernel's network stack and directly accessing network hardware in user space. This repository provides a structured learning path from basic DPDK concepts to advanced packet processing applications.

## Install Dependencies
```bash
# Update system packages
sudo apt update && sudo apt upgrade -y

# Install build dependencies
sudo apt install -y build-essential meson ninja-build pkg-config
sudo apt install -y libnuma-dev libpcap-dev python3-pyelftools
sudo apt install -y linux-headers-$(uname -r)
sudo apt install -y cmake git wget curl

# Install additional tools
sudo apt install -y htop iotop tcpdump wireshark
```