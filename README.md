# LWIP Pong

This is a multiplayer Pong game implemented using the `netconn` API from the [lwIP](https://savannah.nongnu.org/projects/lwip/) TCP/IP stack in a virtual TAP environment.

- The **server** runs over LWIP-TAP and manages paddle positions, ball physics, scoring, and game state broadcasting.
- The **client** is written using [Raylib](https://www.raylib.com/), a lightweight C graphics library, and features client-side prediction to ensure smooth motion even under variable network conditions.

## Features

- Real-time synchronization over TCP
- Graphical interface with paddle and ball movement
- Client-side prediction for smooth rendering
- Keyboard input support (W/S for player 1, ↑/↓ for player 2)
- Minimal latency using TCP_NODELAY

## How to Build

> This project assumes you have a working LWIP-TAP environment.

Server (LWIP-TAP):

1. Make sure you have the original LWIP-TAP environment set up.
2. Clone the repo and place the `pong.c` and `pong.h` inside your `lwip-contrib` folder.
3. Run the original ./configure script (unmodified).
4. Replace the generated Makefile with the provided one (modified for Pong).
5. Then build and run:

make
sudo ./lwip-tap -P -i  addr=162.13.0.2,netmask=255.255.255.0,name=tap0,gw=162.13.0.1 (example)

Client (Raylib):

1. Navigate to the pong-client/ folder.
2. Build using the standalone Makefile provided
3. Run the client by passing the server IP and player number.

./pong-client 162.13.0.2 1

## Planned Improvements

The current version of the client requires users to specify the server IP address and player number as command-line arguments. In future versions, the following enhancements are planned:

- **Interactive menu**: Add a graphical menu for IP input and player selection, eliminating the need to launch via command-line.
- **Automatic player assignment**: Instead of requiring "HELLO:1" or "HELLO:2", the server will dynamically assign the player number based on availability.
- **Better input handling**: More robust detection of disconnections and user feedback.
- **Audio effects and visual polish**: For a more immersive and arcade-like experience.

These changes aim to improve usability and make the game more accessible without requiring technical setup.


## Credits

This project is based on the LWIP-TAP environment by Takayuki Usui.  
All original code is licensed under the BSD 2-Clause License.

Modifications and integration for the Pong server and client were developed by  
**Jose Miguel Guerra**, 2025.

