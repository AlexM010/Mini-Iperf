# Mini-Iperf

Mini-Iperf is a lightweight, custom-built network performance measurement tool written in C using POSIX sockets. It was developed as part of Assignment 2 for the HY435 Networks Technology and Programming Lab.

The tool operates in a client-server architecture and allows for testing:
- **Throughput**
- **Goodput**
- **Jitter**
- **Packet Loss**
- **One-way delay**

It uses:
- TCP for control signaling
- UDP for data transmission
- Multithreading for handling parallel data streams

---

## 📁 Project Structure

Mini-Iperf/  ├── src/ # Source code │ ├── mini_iperf.c # Main program (argument parsing, flow control) │ ├── mini_iperf_client.c # Client-side implementation │ ├── mini_iperf_server.c # Server-side implementation │ └── mini_iperf.h # Shared data structures and function declarations ├── .gitignore # Git ignore rules ├── CMakeLists.txt # CMake configuration (for GCC builds) └── README.md # Project documentation

---
## 🛠️ Build Instructions

### 🔧 Option 1: Using CMake (Recommended)

```bash
mkdir build
cd build
cmake ..
make