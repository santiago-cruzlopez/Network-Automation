# Network Automation and Security
A comprehensive Infrastructure as Code (IaC) repository designed to automate the lifecycle of network security operations. This project integrates automated configuration management, continuous security auditing, and rapid incident response playbooks.

### Core Objectives:
- **Standardization:** Maintain a "Single Source of Truth" for network configurations.
- **Compliance:** Automated validation of CIS benchmarks and internal security policies.
- **Agility:** Reduce MTTR (Mean Time to Repair) through automated patching and threat isolation.
- **Visibility:** Centralized logging and real-time topology mapping.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Installation](#core-installation)

## Prerequisites

## Core Installation

1. System Dependencies and Packages
    - Update package information and install the required packages:
    ```bash
    sudo apt update && sudo apt upgrade -y
    sudo apt install -y python3 python3-venv git wget curl build-essential pkg-config libssl-dev
    ```
    - CMake and C++ Dev:
    ```bash
    sudo apt install -y build-essential meson ninja-build pkg-config
    sudo apt install -y libnuma-dev libpcap-dev python3-pyelftools
    sudo apt install -y linux-headers-$(uname -r)
    sudo apt install -y cmake git wget curl

    # Install additional tools  
    sudo apt install -y htop iotop tcpdump wireshark
    ```
2. Install FFmpeg with SRT Support
   - Install the SRT Library:
   ```bash
   git clone --depth 1 https://github.com/Haivision/srt.git
   mkdir srt/build && cd srt/build
   cmake -DENABLE_SHARED=OFF -DENABLE_STATIC=ON ..
   make && sudo make install
   ```
   - Compile FFmpeg from source [link][https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu]
   ```bash
   git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git
   cd FFmpeg
   ./configure --enable-gpl --enable-nonfree --enable-libsrt --enable-openssl
   make -j$(nproc)
   sudo make install

   # Verification
   ffmpeg -protocols | grep -E "srt|udp"
   ```
