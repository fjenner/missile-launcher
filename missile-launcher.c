#include <hidapi.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <argp.h>

//
// USB vendor ID and product ID for missile launcher
//
#define LAUNCHER_VID        0x0a81
#define LAUNCHER_PID        0x0701

//
// Output report values
//
#define CMD_MOVE_DOWN       0x01
#define CMD_MOVE_UP         0x02
#define CMD_MOVE_LEFT       0x04
#define CMD_MOVE_RIGHT      0x08
#define CMD_FIRE            0x10
#define CMD_STOP            0x20
#define CMD_GET_STATUS      0x40

//
// Input report values
//
#define STATUS_DOWN_LIMIT   0x01
#define STATUS_UP_LIMIT     0x02
#define STATUS_LEFT_LIMIT   0x04
#define STATUS_RIGHT_LIMIT  0x08
#define STATUS_DEVICE_FIRED 0x10

//
// Default delay times
//
#define MOVE_HOLD_TIME_US   100000
#define FIRE_HOLD_TIME_US   500000

/**
 * Version information string
 */
const char *argp_program_version = PROGRAM_NAME " " PROGRAM_VERSION;

/**
 * Bug report email address string
 */
const char *argp_program_bug_address = "<" BUG_EMAIL_ADDRESS ">";

/**
 * Documentation string displayed in help output
 */
static char doc[] =
    "USB missile launcher application for Dream Cheeky's Rocket Baby device. "
    "If no options are provided, fires one missle and exits.";

/**
 * Command-line options supported by this application
 */
static struct argp_option options[] =
{
    { "move",       'm', "DIR",     0,  "Move the turret in the requested direction. Must be one of up, down, left, or right" },
    { "time",       't', "TIME",    0,  "The duration for moving the requested direction, in milliseconds" },
    { "fire",       'f', 0,         0,  "Fire the turret" },
    { "status",     'p', 0,         0,  "Print out status information" },
    { 0 }
};

/**
 * The movements that the missile launcher can perform
 */
enum movement
{
    MOVEMENT_NONE,
    MOVEMENT_TILT_UP,
    MOVEMENT_TILT_DOWN,
    MOVEMENT_PAN_LEFT,
    MOVEMENT_PAN_RIGHT,
};

/**
 * Custom structure to share program option information between command-line
 * parser and application logic
 */
struct arguments
{
    enum movement movement;
    useconds_t movement_duration;
    bool fire;
    bool display_status;
};

/**
 * Command line parser function. Interprets command line options and populates
 * custom state information as appropriate.
 *
 * @param[in] key An identifier corresponding to the option being parsed
 * @param[in] arg The argument (if any) provided with the option
 * @param[in,out] state Custom state passed between parser and application
 *
 * @return Returns 0 on success or an appropriate error code on failure
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    error_t ret = 0;

    struct arguments *arguments = state->input;

    switch (key)
    {
        case 'm':
            if (strcmp(arg, "up") == 0)
                arguments->movement = MOVEMENT_TILT_UP;
            else if (strcmp(arg, "down") == 0)
                arguments->movement = MOVEMENT_TILT_DOWN;
            else if (strcmp(arg, "left") == 0)
                arguments->movement = MOVEMENT_PAN_LEFT;
            else if (strcmp(arg, "right") == 0)
                arguments->movement = MOVEMENT_PAN_RIGHT;
            else
            {
                fprintf(stderr, "Invalid movement: %s\n", arg);
                argp_usage(state);
            }
            break;
        case 'f':
            arguments->fire = true;
            break;
        case 't':
        {
            unsigned long duration_ms;
            char *endptr;

            duration_ms = strtoul(arg, &endptr, 0);
            
            // Check for a valid numeric argument
            if (arg != '\0' && *endptr == '\0' && duration_ms < 10000)
            {
                arguments->movement_duration = duration_ms * 1000;
            }
            else
            {
                fprintf(stderr, "Invalid duration specified\n");
                argp_usage(state);
            }
        }
        break;
            
        case 'p':
            arguments->display_status = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= 0)
            {
                argp_usage(state);
            }
            break;
        default:
            ret = ARGP_ERR_UNKNOWN;
            break;
    }

    return ret;
}

/**
 * Defines parser settings for command-line argument processing
 */
static struct argp argp = { options, parse_opt, NULL, doc };

/**
 * Sends a command to the launcher
 *
 * @param[in] device The HID device corresponding to the launcher
 * @param[in] cmd The command to send to the launcher
 *
 * @return Returns zero on success or non-zero otherwise
 */
static int send_command(hid_device *device, uint8_t cmd)
{
    int ret = 0;
    uint8_t buf[2];

    buf[0] = 0;     // First byte is report number (always 0 for this device)
    buf[1] = cmd;   // Second byte is report value (command)

    // Write an output report to the device
    if (hid_write(device, buf, sizeof(buf)) < 0)
    {
        fprintf(stderr, "Output report write failed\n");
        ret = -1;
    }

    return ret;
}

/**
 * Reads the current status byte from the launcher
 *
 * @param[in] device The HID device corresponding to the launcher
 * @param[out] status Gets populated with the status byte from the launcher
 *
 * @return Returns zero on success or non-zero otherwise
 */
static int get_status(hid_device *device, uint8_t *status)
{
    int ret = 0;

    // Send a request for a status report
    if (send_command(device, CMD_GET_STATUS)!= 0)
    {
        fprintf(stderr, "Failed to send command to fetch status\n");
        ret = -1;
    }
    else
    {
        // Read the input report
        if (hid_read(device, status, sizeof(*status)) < 0)
        {
            fprintf(stderr, "Failed to read input report\n");
            ret = -1;
        }
    }

    return ret;
}

/**
 * Reads and prints the current status fields from the device
 *
 * @param[in] device The HID device corresponding to the launcher
 *
 * @return Returns zero on success or non-zero otherwise
 */
static int print_status(hid_device *device)
{
    int ret = 0;
    uint8_t status;

    if (get_status(device, &status) != 0)
    {
        fprintf(stderr, "Failed to retrieve status information\n");
        ret = -1;
    }
    else
    {
        printf("Tilt up limit:      %s\n"
                "Tilt down limit:    %s\n"
                "Pan left limit:     %s\n"
                "Pan right limit:    %s\n"
                "Fire complete:      %s\n",
                (status & STATUS_UP_LIMIT)       ? "true" : "false",
                (status & STATUS_DOWN_LIMIT)     ? "true" : "false",
                (status & STATUS_LEFT_LIMIT)     ? "true" : "false",
                (status & STATUS_RIGHT_LIMIT)    ? "true" : "false",
                (status & STATUS_DEVICE_FIRED)   ? "true" : "false");
    }

    return ret;
}

/**
 * Fires a single missile from the launcher
 *
 * @param[in] device The HID device corresponding to the launcher
 * 
 * @return Returns zero on success or non-zero otherwise
 */
static int fire_missile(hid_device *device)
{
    int ret = 0;

    if (send_command(device, CMD_FIRE) != 0)
    {
        fprintf(stderr, "Failed to perform requested movement\n");
        ret = -1;
    }
    else
    {
        uint8_t status;

        // Keep reading status until failure or we've completed firing
        do
        {
            if (get_status(device, &status) != 0)
            {
                fprintf(stderr, "Failed to get status\n");
                ret = -1;
            }
        }
        while (!(status & STATUS_DEVICE_FIRED) && ret == 0);

        if (ret == 0)
        {
            // Intentionally overshoot with the firing time to (hopefully)
            // ensure that the missile actually gets fired
            usleep(FIRE_HOLD_TIME_US);

            // Stop firing
            if (send_command(device, CMD_STOP) != 0)
            {
                fprintf(stderr, "Failed to stop firing\n");
                ret = -1;
            }
        }
    }

    return ret;
}

/**
 * Moves the turret in the requested direction for the specified amount of time
 *
 * @param[in] device The HID device corresponding to the launcher
 * @param[in] movement The direction to move the turret
 * @param[in] duration The time to move (in microseconds)
 *
 * @return Returns zero on success or non-zero otherwise
 */
static int move_turret(hid_device *device, enum movement movement,
        useconds_t duration)
{
    int ret = 0;
    uint8_t cmd;

    // Determine which command to send based on the action
    switch (movement)
    {
        case MOVEMENT_TILT_UP:    cmd = CMD_MOVE_UP;      break;
        case MOVEMENT_TILT_DOWN:  cmd = CMD_MOVE_DOWN;    break;
        case MOVEMENT_PAN_LEFT:   cmd = CMD_MOVE_LEFT;    break;
        case MOVEMENT_PAN_RIGHT:  cmd = CMD_MOVE_RIGHT;   break;
        default:
              fprintf(stderr, "Unrecognized movement\n");
              ret = -1;
              break;
    }

    if (ret == 0)
    { 
        // Send the movement command
        if (send_command(device, cmd) != 0)
        {
            fprintf(stderr, "Failed to perform requested movement\n");
            ret = -1;
        }
        else
        {
            // Move for the specified amount of time
            usleep(duration);

            // Stop moving
            if (send_command(device, CMD_STOP) != 0)
            {
                fprintf(stderr, "Failed to stop movement\n");
                ret = -1;
            }
        }
    }

    return ret;
}

/**
 * Application entry point
 *
 * @param[in] argc The number of command-line arguments
 * @param[in] argv Array of command-line argument strings
 *
 * @return Returns zero on success or non-zero otherwise
 */
int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;
    struct arguments arguments;
    hid_device *device;

    // Set default options
    arguments.movement = MOVEMENT_NONE;
    arguments.movement_duration = MOVE_HOLD_TIME_US;
    arguments.display_status = false;
    arguments.fire = false;

    // Parse user-specified options
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    
    // Attempt to open the missile launcher device
    device = hid_open(LAUNCHER_VID, LAUNCHER_PID, NULL);

    if (device == NULL)
    {
        fprintf(stderr, "Failed to open requested device\n");
        ret = EXIT_FAILURE;
    }
    else
    {
        // Perform the requested movement, if any
        if (arguments.movement != MOVEMENT_NONE)
        {
            if (move_turret(device, arguments.movement,
                        arguments.movement_duration) != 0)
            {
                fprintf(stderr, "Failed to move turret\n");
                ret = EXIT_FAILURE;
            }
        }

        // To fire a single shot, we must initiate firing, read status
        // information until the fire cycling indicator toggles, delay briefly
        // to allow firing to complete, then stop firing
        if (arguments.fire && ret == EXIT_SUCCESS)
        {
            if (fire_missile(device) != 0)
            {
                fprintf(stderr, "Failed to fire missile\n");
                ret = EXIT_FAILURE;
            }
        }

        // Display the status information, if requested
        if (arguments.display_status && ret == EXIT_SUCCESS)
        {
            if (print_status(device) != 0)
            {
                fprintf(stderr, "Failed to print status information\n");
                ret = EXIT_FAILURE;
            }
        }
        
        // Clean up the device
        hid_close(device);
    }

    return ret;
}
