#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_LINE_LENGTH 256

// Structure to represent a command
typedef struct {
    int timestamp;
    char process_label[8];
    char action;
} Command;

// Structure to represent statistics for a child process
typedef struct {
    int start_time;
    int end_time;
    int message_count;
} ChildStats;

// Global variables
ChildStats *child_stats;

int sem_id_sync;
int user_max_children;
int *sem_id_children;
pid_t *pids;

int shm_id;
char *shared_memory;

// Counts the number of lines in the specified file.
int count_lines_in_file(const char *filename) {
    // Open the file in read mode
    FILE *file = fopen(filename, "r");
    if (!file) {
        // Print an error message and terminate the program if the file cannot be opened
        perror("Unable to read the file");
        exit(1);
    }

    int count = 0;
    char buffer[MAX_LINE_LENGTH];

    // Read the file line by line and increment the count for each line
    while (fgets(buffer, MAX_LINE_LENGTH, file)) {
        count++;
    }

    // Close the file and return the total line count
    fclose(file);
    return count;
}

// Selects a random line from a file and stores it in the given output array.
void get_random_line(const char *filename, int total_lines, char *output) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to read the file");
        exit(1);
    }

    // Get a random line number from the file
    int target_line = rand() % total_lines;

    // Read the file line by line until we reach the target line
    int current_line = 0;
    while (fgets(output, MAX_LINE_LENGTH, file)) {
        if (current_line == target_line) {
            // We have reached the target line, break out of the loop
            break;
        }
        current_line++;
    }

    fclose(file);
}

// Creates all the necessary semaphores for the program.
void create_semaphores() {
    // Create the sync semaphore
    sem_id_sync = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (sem_id_sync == -1) {
        perror("semget failed for sync semaphore");
        exit(1);
    }
    if (semctl(sem_id_sync, 0, SETVAL, 0) == -1) {
        perror("semctl failed for sync semaphore");
        exit(1);
    }

    // Create the child semaphores
    for (int i = 0; i < user_max_children; i++) {
        if (i >= user_max_children) {
            fprintf(stderr, "Index %d out of bounds for semaphores.\n", i);
            exit(1);
        }
        sem_id_children[i] = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
        if (sem_id_children[i] == -1) {
            perror("semget failed for child semaphore");
            exit(1);
        }
        if (semctl(sem_id_children[i], 0, SETVAL, 0) == -1) {
            perror("semctl failed for child semaphore");
            exit(1);
        }
    }
}

// Waits for a signal on the given semaphore.
void semaphore_wait(int sem_id) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop wait failed");
        exit(1);
    }
}

// Sends a signal on the given semaphore.
void semaphore_signal(int sem_id) {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop signal failed");
        exit(1);
    }
}

// Creates and attaches a shared memory segment.
void create_shared_memory() {
    // Allocate a shared memory segment
    shm_id = shmget(IPC_PRIVATE, MAX_LINE_LENGTH, IPC_CREAT | 0600);
    if (shm_id == -1) {
        // Handle error if memory allocation fails
        perror("shmget failed");
        exit(1);
    }

    // Attach the shared memory segment to the process's address space
    shared_memory = (char *)shmat(shm_id, NULL, 0);
    if (shared_memory == (char *)-1) {
        // Handle error if attachment fails
        perror("shmat failed");
        exit(1);
    }
}

// Disconnects and destroys the shared memory segment.
void destroy_shared_memory() {
    if (shm_id > 0) {
        // Disconnect the shared memory segment
        if (shmdt(shared_memory) == -1) {
            perror("shmdt failed");
        } else {
            printf("Shared memory disconnected.\n");
        }

        // Destroy the shared memory segment
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("shmctl failed");
        } else {
            printf("Shared memory destroyed.\n");
        }

        // Reset the shared memory ID
        shm_id = -1;
    }
}

// Terminates all active child processes.
void terminate_all_children(int max_children, int current_time) {
    printf("Parent: Terminate all active processes.\n");

    for (int i = 0; i < max_children; i++) {
        if (pids[i] > 0) {
            // Send termination command to the child
            strcpy(shared_memory, "T");
            semaphore_signal(sem_id_children[i]);

            // Wait for the message count from the child
            semaphore_wait(sem_id_sync);
            int message_count = atoi(shared_memory);
            child_stats[i].message_count = message_count;

            // Calculate the lifetime of the child
            int duration = current_time - child_stats[i].start_time;

            // Print the lifetime and message count statistics
            printf("Child C%d: Lifetime: %d steps. Messages received: %d.\n", i + 1, duration, child_stats[i].message_count);

            // Wait for the child to terminate
            waitpid(pids[i], NULL, 0);

            // Release the PID
            pids[i] = 0;
        }
    }

    printf("Parent: All processes terminated.\n");
}

// main loop for a child process.
void child_process(int child_index, const char *process_label) {
    int message_count = 0;

    while (1) {
        // Wait for a signal from the parent to read the shared memory
        semaphore_wait(sem_id_children[child_index]);

        // Check if the received message is a termination command
        if (strcmp(shared_memory, "T") == 0) {
            // Send back the message count to the parent process
            snprintf(shared_memory, MAX_LINE_LENGTH, "%d", message_count);
            semaphore_signal(sem_id_sync);

            // Exit the child process
            exit(0);
        }

        // Print the received message and increment the message count
        printf("Child %s: Received message: %s", process_label, shared_memory);
        message_count++;

        // Clear the shared memory and signal the parent
        shared_memory[0] = '\0';
        semaphore_signal(sem_id_sync);
    }
}

// Spawns a new child process with the given child_index and process_label.
void spawn_child(int child_index, const char *process_label) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        child_process(child_index, process_label);
        exit(0);
    } else if (pid > 0) {
        // Parent process
        printf("Parent: Created child process %s with PID %d\n",
               process_label, pid);
        pids[child_index] = pid;
    } else {
        // Error
        perror("fork failed");
        exit(1);
    }
}

// Reads the configuration file and populates the commands array with parsed data.
int read_config(const char *filename, Command *commands) {
    // Open the configuration file for reading
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to read config file.");
        exit(1);
    }

    int count = 0;
    // Read the file line by line
    while (!feof(file)) {
        char line[MAX_LINE_LENGTH];
        // Get a line from the file
        if (!fgets(line, MAX_LINE_LENGTH, file)) {
            break;
        }

        // Parse the line into timestamp, process_label, and action
        int items_read = sscanf(line, "%d %7s %c", &commands[count].timestamp, commands[count].process_label, &commands[count].action);

        // Check if the line indicates the end of the command sequence
        if (items_read == 2 && strcmp(commands[count].process_label, "EXIT") == 0) {
            commands[count].action = 'E'; // Set action for exit command
            count++;
        } else if (items_read == 3) {
            count++;
        }
    }

    // Close the configuration file
    fclose(file);
    return count;
}


// Returns the index of a child process based on its label.
int get_child_index(const char *label) {
    // Check if the label starts with 'C' and the second character is a digit between '1' and '9'
    if (label[0] == 'C' && label[1] >= '1' && label[1] <= '9') {
        // Convert the digit to an integer and subtract 1 to get the zero-based index
        return atoi(&label[1]) - 1;
    }

    return -1;
}

// Sends a random line from the given text file to a random child process.
void send_random_line_to_random_child(const char *text_filename, int total_lines) {
    int active_indices[user_max_children];
    int active_count = 0;

    // Find all active child indices
    for (int i = 0; i < user_max_children; i++) {
        if (pids[i] > 0) {
            active_indices[active_count++] = i;
        }
    }

    // If there are no active children, return
    if (active_count == 0) {
        return;
    }

    // Select a random child
    int random_child_index = active_indices[rand() % active_count];

    // Get a random line from the text file
    char random_line[MAX_LINE_LENGTH];
    get_random_line(text_filename, total_lines, random_line);

    // Send the message to the child process
    printf("Parent: Sending message to child C%d: %s", random_child_index + 1, random_line);
    strcpy(shared_memory, random_line);
    semaphore_signal(sem_id_children[random_child_index]);
    semaphore_wait(sem_id_sync);
}

// Runs the parent process.
void parent_process(const char *text_filename, const char *config_filename) {
    srand(time(NULL));

    int total_lines = count_lines_in_file(text_filename);
    printf("The file contains %d lines.\n", total_lines);

    Command commands[1000];
    int command_count = read_config(config_filename, commands);

    int active_children = 0;
    int current_time = 0;

    int max_timestamp = 0;
    // Find the maximum timestamp of all commands
    for (int i = 0; i < command_count; i++) {
        if (commands[i].timestamp > max_timestamp) {
            max_timestamp = commands[i].timestamp;
        }
    }

    // Iterate through the commands in order of increasing timestamp
    for (current_time = 0; current_time <= max_timestamp; current_time++) {
        for (int i = 0; i < command_count; i++) {
            if (commands[i].timestamp == current_time) {
                Command cmd = commands[i];

                if (cmd.action == 'S') {
                    // Create a new child process
                    printf("Timestamp %d: Received command S to create child %s.\n", cmd.timestamp, cmd.process_label);
                    int child_index = get_child_index(cmd.process_label);
                    if (child_index != -1 && active_children < user_max_children) {
                        child_stats[child_index].start_time = current_time;
                        child_stats[child_index].end_time = -1;
                        child_stats[child_index].message_count = 0;

                        spawn_child(child_index, cmd.process_label);
                        active_children++;
                    }
                } else if (cmd.action == 'T') {
                    // Terminate a child process
                    printf("Timestamp %d: Received command T for child %s.\n", cmd.timestamp, cmd.process_label);
                    int child_index = get_child_index(cmd.process_label);

                    if (child_index != -1 && pids[child_index] > 0) {
                        child_stats[child_index].end_time = current_time;

                        // Send a termination signal to the child
                        strcpy(shared_memory, "T");
                        semaphore_signal(sem_id_children[child_index]);

                        // Wait for the child to finish
                        semaphore_wait(sem_id_sync);
                        child_stats[child_index].message_count = atoi(shared_memory);

                        // Print the lifetime and the number of messages the child received
                        int duration = child_stats[child_index].end_time - child_stats[child_index].start_time;

                        printf("Child %s: Lifetime: %d steps. Messages received: %d.\n",
                            cmd.process_label, duration, child_stats[child_index].message_count);

                        // Wait for the child to terminate
                        waitpid(pids[child_index], NULL, 0);

                        pids[child_index] = 0;
                        active_children--;
                    } else {
                        printf("Child %s not created, skipping T command.\n", cmd.process_label);
                    }
                } else if (cmd.action == 'E') {
                    // Terminate all active children and exit
                    printf("Timestamp %d: Received EXIT command. The termination begins.\n", cmd.timestamp);
                    terminate_all_children(user_max_children, current_time);
                }
            }
        }

        // Send a random line to a random child
        send_random_line_to_random_child(text_filename, total_lines);
    }

    //return active_children;
}

// Destroys the semaphores created for the parent, sync and all children.
void destroy_semaphores() {
    // Destroy the semaphores for all children
    for (int i = 0; i < user_max_children; i++) {
        // Destroy the semaphore for the current child
        if (semctl(sem_id_children[i], 0, IPC_RMID) == -1) {
            perror("semctl failed for child semaphore");
        }
    }
    // Destroy the sync semaphore
    if (semctl(sem_id_sync, 0, IPC_RMID) == -1) {
        perror("semctl failed for sync semaphore");
    }

    printf("Semaphores destroyed successfully.\n");
}

// Reads the configuration file and returns the maximum child index present in the configuration file.
int get_max_child_from_config(const char *config_filename) {
    FILE *file = fopen(config_filename, "r");
    if (!file) {
        perror("Unable to read the config file");
        exit(1);
    }

    int max_child_index = -1;
    char line[MAX_LINE_LENGTH];

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        char process_label[8];
        // Extract the process label if the line format is correct
        if (sscanf(line, "%*d %7s", process_label) == 1) {
            // Check if the label starts with 'C' followed by a digit between '1' and '9'
            if (process_label[0] == 'C' && process_label[1] >= '1' && process_label[1] <= '9') {
                int child_index = atoi(&process_label[1]);
                // Update max_child_index if a larger value is found
                if (child_index > max_child_index) {
                    max_child_index = child_index;
                }
            }
        }
    }

    fclose(file);
    return max_child_index;
}

int main(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <max_children> <text_filename> <config_filename>\n", argv[0]);
        return 1;
    }

    user_max_children = atoi(argv[1]);
    // Validate that the number of children is greater than 0
    if (user_max_children <= 0) {
        fprintf(stderr, "The number of children must be > 0\n");
        return 1;
    }

    int config_max_children = get_max_child_from_config(argv[3]);
    // Check if the configuration file contains valid children data
    if (config_max_children == -1) {
        fprintf(stderr, "No children found in the config file\n");
        return 1;
    }

    // Ensure the user-defined max_children is not less than the config max_children
    if (user_max_children < config_max_children) {
        fprintf(stderr, "Error: The max_children (%d) is less than the max children in config (%d)\n", user_max_children, config_max_children);
        return 1;
    }

    if (count_lines_in_file(argv[2]) == 0) {
        fprintf(stderr, "The text file is empty\n");
        return 1;
    }

    sem_id_children = malloc(user_max_children * sizeof(int));
    // Check if memory allocation for semaphores is successful
    if (!sem_id_children) {
        perror("malloc failed");
        return 1;
    }

    pids = calloc(user_max_children, sizeof(pid_t));
    // Check if memory allocation for pids is successful
    if (!pids) {
        perror("calloc failed for pids");
        free(sem_id_children);
        return 1;
    }

    child_stats = calloc(user_max_children, sizeof(ChildStats));
    // Check if memory allocation for child stats is successful
    if (!child_stats) {
        perror("calloc failed for child_stats");
        free(sem_id_children);
        free(pids);
        return 1;
    }

    create_semaphores();
    create_shared_memory();

    // Start the parent process with the given configuration
    parent_process(argv[2], argv[3]);

    destroy_shared_memory();
    destroy_semaphores();

    if (sem_id_children != NULL) {
        free(sem_id_children);
        sem_id_children = NULL;
    }
    if (pids != NULL) {
        free(pids);
        pids = NULL;
    }
    if (child_stats != NULL) {
        free(child_stats);
        child_stats = NULL;
    }
    
    return 0;
}
