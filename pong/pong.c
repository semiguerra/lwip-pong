#include "pong.h"
#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/sys.h"
#include "lwip/api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

// === Constants for game settings ===
#define PORT 12345                         // TCP port used for the Pong server
#define FPS 60                             // Frames per second
#define FRAME_TIME_MS (1000 / FPS)         // Time per frame in milliseconds
#define FIELD_WIDTH 80                     // Width of the playing field (text-based)
#define FIELD_HEIGHT 24                    // Height of the playing field
#define PADDLE_HEIGHT 4                    // Height of each paddle
#define PADDLE_WIDTH 2                     // Width of each paddle
#define PADDLE_OFFSET_X 2                  // Horizontal offset of paddles from edge
#define SERVE_TIME (FPS * 3)               // Time to wait before serving the ball
#define MAX_BUFFER_SIZE 256                // Max size of TCP receive buffer
#define MAX_INPUT_LEN 64                   // Max length of input command

// Ball movement configuration
#define INITIAL_BALL_SPEED 0.5f
#define MAX_BALL_SPEED 1.2f
#define SPEED_INCREASE_FACTOR 1.03f
#define MAX_BOUNCE_ANGLE (M_PI / 4.0f)
#define MIN_BOUNCE_ANGLE 0.3f

// === Input enumeration ===
typedef enum { NONE, UP, DOWN } Input;

// === Player state ===
typedef struct {
    int y;         // Paddle vertical position
    Input input;   // Last received input
} Player;

// === Ball state ===
typedef struct {
    float x, y;        // Ball position
    float dx, dy;      // Ball velocity
    int serve_timer;   // Delay before serve
    float speed;       // Current ball speed
} Ball;

// === Client connection state ===
typedef struct {
    struct netconn *conn;             // TCP connection object
    char buffer[MAX_BUFFER_SIZE];     // Input buffer
    int buffer_len;                   // Length of buffered data
    int id;                           // Player ID (1 or 2)
} Client;

// Ensures that the paddle's vertical position stays within the boundaries of the game field.
static void clamp_paddle(Player *p) {
    if (p->y < 0) p->y = 0;
    // If the paddle is above the top edge (y < 0), clamp it to the top.

    if (p->y > FIELD_HEIGHT - PADDLE_HEIGHT)
        p->y = FIELD_HEIGHT - PADDLE_HEIGHT;
    // If the bottom of the paddle exceeds the bottom of the field,
    // clamp it so its bottom aligns with the bottom edge.
    // FIELD_HEIGHT is the total number of vertical units.
    // PADDLE_HEIGHT is the number of units the paddle occupies.
}

// Parses a text command received from the client into a movement action.
// Returns UP, DOWN, or NONE depending on the command string.
static Input parse_input_line(const char *line) {
    if (strncmp(line, "INPUT:UP", 8) == 0) return UP;
    // If the line starts with "INPUT:UP", we interpret it as the UP command.

    if (strncmp(line, "INPUT:DOWN", 10) == 0) return DOWN;
    // If the line starts with "INPUT:DOWN", it's interpreted as the DOWN command.

    return NONE;
    // If it doesn't match either, the input is ignored and treated as no movement.
}

// Resets the ball to the center of the field and assigns an initial velocity.
// The direction of the horizontal movement depends on which player is serving.
static void reset_ball(Ball *ball, int serving_player) {
    ball->x = FIELD_WIDTH / 2;
    ball->y = FIELD_HEIGHT / 2;
    // Places the ball at the center of the field, using the defined dimensions.

    ball->speed = INITIAL_BALL_SPEED;
    // Sets the ball speed to its default value.

    float angle;
    do {
        // Randomly generate an angle within +-30 degrees (pi/6) from the horizontal.
        angle = ((rand() % 1000) / 1000.0f) * (M_PI / 3) - (M_PI / 6);
    } while (fabsf(sinf(angle)) < 0.3f);
    // If the vertical component is too small (almost horizontal),
    // regenerate the angle to avoid boring, flat serves.

    float dir = (serving_player == 1) ? 1.0f : -1.0f;
    // If player 1 is serving, the ball goes right (positive x);
    // otherwise, it goes left (negative x).

    ball->dx = dir * ball->speed * cosf(angle);
    ball->dy = ball->speed * sinf(angle);
    // Sets the horizontal and vertical components of velocity using trigonometry.

    ball->serve_timer = SERVE_TIME;
    // Introduces a delay before the ball starts moving, allowing players to prepare.
}

// Main server loop executed in a separate thread.
// Handles client connections, game state updates, and state broadcasting.
static void pong_thread(void *arg) {
    srand(time(NULL)); 
    // Seed the random number generator to ensure varying serve angles.

    struct netconn *listener = netconn_new(NETCONN_TCP);
    if (!listener) return;
    // Create a new TCP connection object for listening. If allocation fails, exit.

    // Bind the listener to any local IP and the predefined port.
    // Then set it to listen mode to accept incoming connections.
    if (netconn_bind(listener, NULL, PORT) != ERR_OK || netconn_listen(listener) != ERR_OK) {
        netconn_delete(listener);
        return;
    }

    Client clients[2] = {0}; 
    // Array of two client structures to track both player connections.

    int ready = 0;
    // Counter to keep track of how many clients have successfully connected.

    // === Wait for both players to connect ===
    while (ready < 2) {
        struct netconn *conn;
        if (netconn_accept(listener, &conn) == ERR_OK) {
            // Accept a new incoming TCP connection from a client.

            struct netbuf *nbuf;
            char buf[32] = {0}; 
            // Temporary buffer to store the handshake message.

            // Try to receive a message from the client to identify it.
            if (netconn_recv(conn, &nbuf) == ERR_OK && nbuf) {
                void *data; u16_t len;
                netbuf_data(nbuf, &data, &len);
                len = len > 31 ? 31 : len;
                memcpy(buf, data, len); 
                buf[len] = '\0'; 
                netbuf_delete(nbuf);
            }

            // Match incoming message to identify the client as player 1 or 2.
            if (strncmp(buf, "HELLO:1", 7) == 0 && !clients[0].conn) {
                clients[0] = (Client){ .conn = conn, .id = 1 };
                netconn_write(conn, "WELCOME 1\n", 10, NETCONN_COPY);
                ready++;
            } else if (strncmp(buf, "HELLO:2", 7) == 0 && !clients[1].conn) {
                clients[1] = (Client){ .conn = conn, .id = 2 };
                netconn_write(conn, "WELCOME 2\n", 10, NETCONN_COPY);
                ready++;
            } else {
                // If message is invalid or the slot is already taken, reject connection.
                netconn_close(conn);
                netconn_delete(conn);
            }
        }
        sys_msleep(100); 
        // Avoid CPU overuse while polling for clients.
    }

        // === Initialize game state ===
    Player p1 = {FIELD_HEIGHT / 2 - PADDLE_HEIGHT / 2, NONE};
    Player p2 = {FIELD_HEIGHT / 2 - PADDLE_HEIGHT / 2, NONE};
    // Both paddles start centered vertically, with no input.

    Ball ball;
    int score1 = 0, score2 = 0;
    // Initialize ball structure and player scores to 0.

    reset_ball(&ball, 1);
    // Start the game with player 1 serving.

    // === Main game loop ===
    while (1) {
        // === Handle player input ===
        for (int i = 0; i < 2; i++) {
            struct netbuf *nbuf;

            // Receive data from each client (non-blocking if no data).
            if (clients[i].conn && netconn_recv(clients[i].conn, &nbuf) == ERR_OK && nbuf) {
                void *data;
                u16_t len;
                netbuf_data(nbuf, &data, &len);

                if (len >= 5) {
                    Input in = parse_input_line(data);
                    // Convert the received string into an input enum (UP/DOWN/NONE).

                    // Update the corresponding player’s input.
                    if (clients[i].id == 1) p1.input = in;
                    else p2.input = in;
                }
                netbuf_delete(nbuf);
            }
        }

        // === Update paddle positions based on input ===
        if (p1.input == UP)   p1.y--;
        if (p1.input == DOWN) p1.y++;
        if (p2.input == UP)   p2.y--;
        if (p2.input == DOWN) p2.y++;

        // Ensure paddles stay within screen bounds.
        clamp_paddle(&p1);
        clamp_paddle(&p2);

    // === Move ball if serve timer is 0 ===
        if (ball.serve_timer > 0) {
            ball.serve_timer--;
            // If a point was just scored, we wait SERVE_TIME frames before moving the ball.
            // This gives players time to react after a reset.
        } else {
            ball.x += ball.dx;
            ball.y += ball.dy;
            // Move the ball according to its current velocity.
        }

        // === Bounce on top and bottom screen edges ===
        if (ball.y < 0 || ball.y > FIELD_HEIGHT - 1)
            ball.dy *= -1;
        // If the ball goes above the top or below the bottom of the screen,
        // invert its vertical direction to simulate a bounce.

        // === Collision detection with paddle 1 (left side) ===
        if (ball.dx < 0 && ball.x <= PADDLE_OFFSET_X + PADDLE_WIDTH) {
            // Only check collision if the ball is moving left (dx < 0)
            // and reaches the horizontal area where paddle 1 is located.

            if (ball.y >= p1.y && ball.y <= p1.y + PADDLE_HEIGHT) {
                // If the ball's vertical position is within paddle 1's height,
                // it is considered a valid hit.
                ball.dx *= -1;
                // Invert the horizontal direction to simulate a bounce off paddle 1.
            }
        }

        // === Collision detection with paddle 2 (right side) ===
        if (ball.dx > 0 && ball.x >= FIELD_WIDTH - PADDLE_OFFSET_X - PADDLE_WIDTH) {
            // Ball is moving to the right and reaches paddle 2's area.

            if (ball.y >= p2.y && ball.y <= p2.y + PADDLE_HEIGHT) {
                // If it’s within the paddle’s vertical range, bounce it back.
                ball.dx *= -1;
            }
        }

        // === Scoring ===
        if (ball.x < 0) {
            // If the ball exits the field on the left side, player 2 scores.
            score2++;
            reset_ball(&ball, 1); // Restart the ball with player 1 serving.
        } else if (ball.x > FIELD_WIDTH) {
            // If the ball exits the field on the right side, player 1 scores.
            score1++;
            reset_ball(&ball, 2); // Restart the ball with player 2 serving.
        }

        // === Format the current game state into a string ===
        char state[128];
        snprintf(state, sizeof(state), "STATE:%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d\n",
                 p1.y, p2.y,         // Paddle positions (vertical only)
                 ball.x, ball.y,     // Ball position (float precision)
                 ball.dx, ball.dy,   // Ball velocity (dx = horizontal, dy = vertical)
                 score1, score2,     // Current scores of both players
                 ball.serve_timer);  // Remaining delay before next ball movement

        // === Send the state to both connected clients ===
        for (int i = 0; i < 2; i++) {
            if (clients[i].conn) {
                // Send the formatted string using LWIP's netconn API.
                netconn_write(clients[i].conn, state, strlen(state), NETCONN_COPY);
                // NETCONN_COPY tells LWIP to copy the data into its own buffer,
                // allowing us to reuse or free our buffer safely after.
            }
        }

        // === Control frame rate ===
        sys_msleep(FRAME_TIME_MS);
        // Pause execution for the duration of one frame.
        // This ensures that updates occur at a fixed rate (e.g., 60 FPS).
    }
}

// Entry point to start the game logic thread from outside.
// This function is called once at setup time to launch the server.
void pong_init(void) {
    sys_thread_new("pong_thread", pong_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
    // Creates a new system thread named "pong_thread" to run the game logic.
    // The stack size and priority are defined by LWIP's configuration.
}

#endif /* LWIP_NETCONN */
