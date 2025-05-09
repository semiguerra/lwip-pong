
/*
  -------------------------------------------------------------------------------
  Ball Prediction: Client-Side Prediction for Smooth Rendering
  -------------------------------------------------------------------------------

  When the server sends the ball's position and velocity (via STATE:...),
  the client stores that information and starts *predicting* the ball's position
  in every frame using the last known velocity.

  This technique allows the client to render fluid motion independently of
  network delay, and then corrects any small deviation when the next STATE arrives.

  -------------------------------------------------------------------------------
  Formula Used (executed once per frame):
  
      x += dx * Δt * 60
      y += dy * Δt * 60

  Where:
      - x, y       → predicted ball position (logical server units)
      - dx, dy     → velocity components from the last server message
      - Δt         → time passed since last frame (from GetFrameTime())
      - 60         → correction factor to scale from seconds to frames (assuming 60 FPS)

  -------------------------------------------------------------------------------
  Variables involved:

      predicted.x       → last known x position of the ball
      predicted.y       → last known y position of the ball
      predicted.dx/dy   → last known velocity of the ball
      predicted.valid   → whether a prediction is currently valid
      predicted.last_update → timestamp of last server update (GetTime())

  -------------------------------------------------------------------------------
  Client Prediction Flow Diagram

    [Server]                            [Client]

       |                                  |
       |---- STATE:x,y,dx,dy,score,timer →|  ← Authoritative update
       |                                  |
       |                            +----------------------------+
       |                            | predicted.x ← x            |
       |                            | predicted.y ← y            |
       |                            | predicted.dx ← dx          |
       |                            | predicted.dy ← dy          |
       |                            | predicted.last_update ← now|
       |                            +----------------------------+
       |                                  |
       |                  For each frame (60 fps):
       |                            Δt ← GetFrameTime()
       |                            x ← x + dx · Δt · 60
       |                            y ← y + dy · Δt · 60
       |                                  |
       |              ← until next authoritative STATE message
       |<--- next STATE arrives -----------|

  -------------------------------------------------------------------------------

  This mechanism ensures smooth gameplay even with slight packet delay or jitter,
  improving the perceived responsiveness of the game.

*/


#include <stdio.h>          // Standard input/output functions
#include <stdlib.h>         // General utilities: memory allocation, conversion
#include <string.h>         // String manipulation (e.g., memcpy, strcat)
#include <unistd.h>         // POSIX close(), read(), write(), etc.
#include <netdb.h>          // Definitions for network database operations
#include <arpa/inet.h>      // Functions for manipulating IP addresses
#include <errno.h>          // For interpreting error codes returned by syscalls
#include <fcntl.h>          // File control options (not directly used here)
#include <sys/time.h>       // System time functions (e.g., for timestamps)
#include <sys/socket.h>     // Core socket API (socket(), connect(), etc.)
#include <netinet/tcp.h>    // TCP-specific socket options (e.g., TCP_NODELAY)
#include "raylib.h"         // Simple and portable graphics library for rendering

#define PORT 12345              // Must match the server's listening port
#define BUFFER_SIZE 256         // Buffer size for receiving data over TCP
#define CONNECT_TIMEOUT 5       // Timeout (in seconds) for initial connection
#define WELCOME_TIMEOUT 5       // Timeout (in seconds) to wait for "WELCOME" message

// Rendering settings for the window and elements (in pixels)
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PADDLE_WIDTH 20
#define PADDLE_HEIGHT 100
#define BALL_SIZE 15

// Virtual field dimensions and layout (match server logic)
#define SERVER_WIDTH 80
#define SERVER_HEIGHT 24
#define SERVER_PADDLE_HEIGHT 4
#define SERVER_PADDLE_OFFSET_X 2
#define SERVER_PADDLE_WIDTH 2
#define SERVER_EXPECTED_MESSAGES 9


// Represents the current status of the client's connection to the server
typedef enum {
    CONNECTION_STATE_CONNECTING,        // Initial state while attempting to connect
    CONNECTION_STATE_WAITING_WELCOME,   // Connected, waiting for server to send "WELCOME"
    CONNECTION_STATE_PLAYING,           // Game is active and running
    CONNECTION_STATE_DISCONNECTED       // Server connection was lost or closed
} ConnectionState;


// Represents the current game state as received from the server
typedef struct {
    int is_player1;     // 1 if this client is player 1, 0 otherwise
    int p1_y;           // Y-position of player 1's paddle (in logical units)
    int p2_y;           // Y-position of player 2's paddle
    int score1;         // Score for player 1
    int score2;         // Score for player 2
    int serve_timer;    // Frames remaining before ball is served (used for countdown)
} GameState;


// Structure to hold locally predicted ball state between updates
typedef struct {
    float x, y;              // Predicted position of the ball
    float dx, dy;            // Predicted velocity (from last known server state)
    double last_update;      // Timestamp of the last authoritative update
    int valid;               // 1 if prediction is active; 0 if not yet initialized
} PredictedBall;

PredictedBall predicted = {0}; // Global variable initialized to all zeros

// Renders the entire current frame of the game, including paddles, ball, score, and UI.
void draw_game(GameState *state, const char *last_input) {
    BeginDrawing();                     // Start drawing a new frame
    ClearBackground(BLACK);            // Clear screen with black background

    // Convert paddle Y positions from logical (server) units to screen pixels
    float p1_screen_y = ((float)state->p1_y / SERVER_HEIGHT) * SCREEN_HEIGHT;
    float p2_screen_y = ((float)state->p2_y / SERVER_HEIGHT) * SCREEN_HEIGHT;

    // Calculate X positions of paddles using fixed server constants
    float paddle1_x = ((float)SERVER_PADDLE_OFFSET_X / SERVER_WIDTH) * SCREEN_WIDTH;
    float paddle2_x = ((float)(SERVER_WIDTH - SERVER_PADDLE_OFFSET_X - SERVER_PADDLE_WIDTH) / SERVER_WIDTH) * SCREEN_WIDTH;

    // Draw both paddles using converted positions
    DrawRectangle(paddle1_x, p1_screen_y, PADDLE_WIDTH, PADDLE_HEIGHT, WHITE);
    DrawRectangle(paddle2_x, p2_screen_y, PADDLE_WIDTH, PADDLE_HEIGHT, WHITE);


    // Convert predicted ball position to screen coordinates
    float ball_screen_x = (predicted.x / SERVER_WIDTH) * SCREEN_WIDTH;
    float ball_screen_y = (predicted.y / SERVER_HEIGHT) * SCREEN_HEIGHT;

    // Only draw the ball if serve_timer is zero (i.e., game is active)
    if (state->serve_timer <= 0) {
        DrawCircle(ball_screen_x, ball_screen_y, BALL_SIZE, WHITE);
    }


    // Draw the current score (left and right)
    DrawText(TextFormat("%d", state->score1), SCREEN_WIDTH / 4, 30, 40, WHITE);
    DrawText(TextFormat("%d", state->score2), 3 * SCREEN_WIDTH / 4, 30, 40, WHITE);


    // Draw vertical dashed line in the middle of the screen
    for (int i = 0; i < SCREEN_HEIGHT; i += 30) {
        DrawRectangle(SCREEN_WIDTH / 2 - 2, i, 4, 20, WHITE);
    }

    // Show countdown number if a serve delay is active
    if (state->serve_timer > 0) {
        int countdown = (state->serve_timer + 29) / 30;
        // Divide remaining frames by 30 to approximate a countdown in seconds (rounded up)
        DrawText(TextFormat("%d", countdown), SCREEN_WIDTH / 2 - 10, SCREEN_HEIGHT / 2 - 20, 40, WHITE);
    }

    // Show last sent input (debug/feedback)
    if (last_input) {
        DrawText(TextFormat("Last input: %s", last_input), 10, SCREEN_HEIGHT - 30, 20, GREEN);
    }

    EndDrawing(); // Submit the frame to be displayed
}

// Sends player input to the server based on keypresses.
// Returns a string representing the last input sent (for optional display/debug).
const char *handle_input(int sockfd, GameState *state) {
    const char *msg = "INPUT:IDLE\n";
    // Default message to send when no input is detected (idle state).

    if (state->is_player1) {
        // Player 1 uses W and S keys for movement
        if (IsKeyDown(KEY_W)) msg = "INPUT:UP\n";
        else if (IsKeyDown(KEY_S)) msg = "INPUT:DOWN\n";
    } else {
        // Player 2 uses UP and DOWN arrow keys
        if (IsKeyDown(KEY_UP)) msg = "INPUT:UP\n";
        else if (IsKeyDown(KEY_DOWN)) msg = "INPUT:DOWN\n";
    }

    send(sockfd, msg, strlen(msg), MSG_NOSIGNAL);
    // Send the input message to the server over TCP.
    // MSG_NOSIGNAL prevents the process from receiving SIGPIPE if the connection is closed.

    return msg;
    // Return the sent message so it can be optionally shown on screen.
}


// Parses a line received from the server and updates the local game state and prediction.
// Returns 1 if the line was successfully parsed and applied, 0 otherwise.
int process_game_state(char *line, GameState *state) {
    int new_p1_y, new_p2_y, score1, score2, timer;
    float ball_x, ball_y, ball_dx, ball_dy;

    // Try to parse a full game state update from the server.
    // Expected format:
    // STATE:<p1_y>,<p2_y>,<ball_x>,<ball_y>,<ball_dx>,<ball_dy>,<score1>,<score2>,<timer>
    int parsed = sscanf(line, "STATE:%d,%d,%f,%f,%f,%f,%d,%d,%d",
                        &new_p1_y, &new_p2_y, &ball_x, &ball_y,
                        &ball_dx, &ball_dy, &score1, &score2, &timer);

    if (parsed == SERVER_EXPECTED_MESSAGES) {
        // If all the values were successfully parsed, update the local game state:
        state->p1_y = new_p1_y;
        state->p2_y = new_p2_y;
        state->score1 = score1;
        state->score2 = score2;
        state->serve_timer = timer;

        // Update the prediction structure using the latest authoritative ball state.
        predicted.x = ball_x;
        predicted.y = ball_y;
        predicted.dx = ball_dx;
        predicted.dy = ball_dy;
        predicted.last_update = GetTime(); // Timestamp of the update
        predicted.valid = 1;               // Enable prediction on the next frame

        return 1; // Parsing and update successful
    }

    return 0; // Message format was invalid or incomplete
}


int main(int argc, char *argv[]) {
    // Check argument count: expects server IP and player number
    if (argc != 3) {
        printf("Usage: %s <server_ip> <player_number>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int player_number = atoi(argv[2]);

    // Validate player number: must be 1 or 2
    if (player_number != 1 && player_number != 2) {
        printf("Player must be 1 or 2.\n");
        return 1;
    }

    // Initialize graphical window with target FPS
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Pong Client (Predicted)");
    SetTargetFPS(60);

    // Create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Set up server address struct
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    // Connect to server
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    // Disable Nagle's algorithm for lower latency
    int opt = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // Send initial HELLO message to identify as player 1 or 2
    char hello_msg[32];
    snprintf(hello_msg, sizeof(hello_msg), "HELLO:%d\n", player_number);
    send(sockfd, hello_msg, strlen(hello_msg), MSG_NOSIGNAL);

    // Initialize local game state
    GameState state = {.is_player1 = (player_number == 1)};

    char buffer[BUFFER_SIZE * 2] = {0}; // Accumulated incoming data buffer
    const char *last_input = NULL;      // Pointer to last input sent (for UI)

    // === Main game loop ===
    while (!WindowShouldClose()) {
        double now = GetTime(); // Get current timestamp (in seconds since program start)

        // --- Ball prediction logic ---
        // If we have received at least one authoritative update from the server,
        // and it was recent enough (within 1 second), we continue predicting.
        if (predicted.valid && (now - predicted.last_update) < 1.0) {
            // GetFrameTime() returns the time elapsed since the last frame (in seconds).
            // For example, at 60 FPS, this will return approximately 0.01667.
             double dt = GetFrameTime();

            // The server expresses ball velocity in "units per frame", assuming 60 FPS.
            // To convert it into "units per second", we multiply by 60.0f.
            // Then we multiply by dt to scale the movement by real elapsed time.
            //
            // This results in: delta_position = velocity_per_frame × (seconds/frame) × frames/sec
            //                ≈ velocity_per_frame × seconds
            //
            // So overall:
            //      predicted.x ← predicted.x + dx × dt × 60
            //      predicted.y ← predicted.y + dy × dt × 60

            predicted.x += predicted.dx * dt * 60.0f;
            predicted.y += predicted.dy * dt * 60.0f;

            // Update the prediction timestamp to the current time.
            // This ensures prediction continues smoothly on the next frame.
            predicted.last_update = now;
        }

        // --- Handle input ---
        last_input = handle_input(sockfd, &state);

        // --- Receive and process data from server ---
        char netbuf[BUFFER_SIZE] = {0};
        ssize_t n = recv(sockfd, netbuf, sizeof(netbuf) - 1, 0);

        if (n > 0) {
            netbuf[n] = '\0';
            strcat(buffer, netbuf);

            char *line;
            while ((line = strchr(buffer, '\n'))) {
                *line = '\0'; // Null-terminate line
                process_game_state(buffer, &state); // Try to parse
                memmove(buffer, line + 1, strlen(line + 1) + 1); // Shift buffer
            }
        }

        // --- Render frame ---
        draw_game(&state, last_input);
    }

    // === Cleanup ===
    shutdown(sockfd, SHUT_RDWR); // Gracefully close TCP socket
    close(sockfd);               // Release descriptor
    CloseWindow();               // Close graphical window
    return 0;
}

