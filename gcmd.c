#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_BUFFER_SIZE 8192
#define MAX_PROMPT_SIZE 2048
#define MAX_COMMAND_SIZE 1024

// Default prompt for shell command generation
static const char* DEFAULT_PROMPT =
    "You are an expert system administrator and shell command generator. "
    "Convert the following natural language request into a precise shell command with description. "
    "Rules: "
    "1. Return EXACTLY in this format: COMMAND|||DESCRIPTION "
    "2. COMMAND: The shell command only, no explanation or formatting "
    "3. DESCRIPTION: A clear explanation of what the command does "
    "4. Use standard POSIX commands when possible "
    "5. Prefer safe, commonly available commands "
    "6. For complex tasks, provide a single command or pipeline "
    "7. Do not include dangerous commands like 'rm -rf /' or 'dd' without explicit safety "
    "8. If the request is unclear, provide the most reasonable interpretation "
    "9. Do not use markdown formatting, backticks, or code blocks "
    "Examples: "
    "'list all files' -> 'ls -la|||Lists all files and directories with detailed information including hidden files' "
    "'find large files' -> 'find . -type f -size +100M -ls|||Searches for all files larger than 100MB in the current directory and subdirectories' "
    "'check disk usage' -> 'df -h|||Displays disk space usage in human-readable format for all mounted filesystems' "
    "'show running processes' -> 'ps aux|||Shows all running processes with detailed information including user, CPU, and memory usage' "
    "Convert this request: ";

void print_usage(const char* program_name) {
    printf("Usage: %s [options] <natural language command>\n\n", program_name);
    printf("Generate shell commands from natural language using AI.\n\n");
    printf("OPTIONS:\n");
    printf("    -m, --model MODEL       Specify the AI model (default: gemini-1.5-pro-latest)\n");
    printf("    -t, --temp TEMP         Set temperature (default: 0.3)\n");
    printf("    -p, --prompt FILE       Use custom prompt file\n");
    printf("    -g, --gcli PATH         Path to gcli binary (default: gcli)\n");
    printf("    -s, --shell SHELL       Target shell (bash, zsh, fish, etc.)\n");
    printf("    -e, --execute           Execute the command immediately (use with caution)\n");
    printf("    -c, --copy              Copy command to clipboard (macOS/Linux) [DEFAULT]\n");
    printf("    -q, --quiet             Only output the command, no prompts\n");
    printf("    -v, --verbose           Show the prompt being sent to AI\n");
    printf("    --dry-run               Show what would be executed without running\n");
    printf("    -h, --help              Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("    %s \"list all files here\"                    # Generate and copy: ls -la\n", program_name);
    printf("    %s \"find files larger than 100MB\"          # Generate and copy: find . -size +100M\n", program_name);
    printf("    %s \"show disk usage\"                        # Generate and copy: df -h\n", program_name);
    printf("    %s -e \"check running processes\"             # Generate and execute immediately\n", program_name);
    printf("    %s -q \"compress this directory\"             # Generate and output only (no copy)\n", program_name);
    printf("    %s -s fish \"list files by size\"            # Generate fish shell command and copy\n\n", program_name);
    printf("SAFETY:\n");
    printf("    - Commands are shown before execution\n");
    printf("    - Dangerous commands require confirmation\n");
    printf("    - Use --dry-run to see what would be executed\n");
    printf("    - Review generated commands before using -e flag\n\n");
}

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length > MAX_PROMPT_SIZE) {
        fprintf(stderr, "Error: Prompt file too large (max %d bytes)\n", MAX_PROMPT_SIZE);
        fclose(file);
        return NULL;
    }

    char* content = malloc(length + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

    return content;
}

int is_dangerous_command(const char* command) {
    // List of potentially dangerous command patterns
    const char* dangerous_patterns[] = {
        "rm -rf /",
        "rm -rf /*",
        "dd if=",
        "mkfs",
        "fdisk",
        "parted",
        ":(){ :|:& };:",  // fork bomb
        "chmod 777 /",
        "chown root /",
        "> /dev/sda",
        "format c:",
        "del /s /q c:\\",
        NULL
    };

    for (int i = 0; dangerous_patterns[i] != NULL; i++) {
        if (strstr(command, dangerous_patterns[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

int confirm_dangerous_command(const char* command) {
    printf("WARNING: This command may be dangerous:\n");
    printf("Command: %s\n", command);
    printf("Do you want to continue? (y/N): ");
    
    char response[10];
    if (fgets(response, sizeof(response), stdin) != NULL) {
        if (response[0] == 'y' || response[0] == 'Y') {
            return 1;  // Continue
        }
    }
    return 0;  // Don't continue
}

int prompt_user_action(const char* command) {
    printf("Generated command: %s\n", command);
    printf("What would you like to do?\n");
    printf("1. Execute the command\n");
    printf("2. Copy to clipboard\n");
    printf("3. Show command only\n");
    printf("4. Quit\n");
    printf("Enter your choice (1-4): ");
    
    char input[10];
    if (fgets(input, sizeof(input), stdin) != NULL) {
        int choice = atoi(input);
        switch (choice) {
            case 1: return 1;  // Execute
            case 2: return 2;  // Copy
            case 3: return 3;  // Show only
            default: return 0;  // Quit
        }
    }
    return 0;  // Quit
}

int copy_to_clipboard(const char* command) {
    char copy_cmd[MAX_COMMAND_SIZE + 100];

#ifdef __APPLE__
    snprintf(copy_cmd, sizeof(copy_cmd), "echo '%s' | pbcopy", command);
#elif __linux__
    // Try xclip first, then xsel
    if (system("which xclip > /dev/null 2>&1") == 0) {
        snprintf(copy_cmd, sizeof(copy_cmd), "echo '%s' | xclip -selection clipboard", command);
    } else if (system("which xsel > /dev/null 2>&1") == 0) {
        snprintf(copy_cmd, sizeof(copy_cmd), "echo '%s' | xsel --clipboard --input", command);
    } else {
        printf("Error: No clipboard utility found (install xclip or xsel)\n");
        return 1;
    }
#else
    printf("Error: Clipboard copy not supported on this platform\n");
    return 1;
#endif

    int result = system(copy_cmd);
    if (WEXITSTATUS(result) == 0) {
        return 0;
    } else {
        printf("Failed to copy to clipboard\n");
        return 1;
    }
}

void display_command_result(const char* generated_output, int copy_success) {
    // Parse the command|||description format
    char* output_copy = strdup(generated_output);
    char* command = output_copy;
    char* description = NULL;
    
    // Look for the separator
    char* separator = strstr(output_copy, "|||");
    if (separator) {
        *separator = '\0';
        description = separator + 3;
        
        // Trim whitespace from command
        while (*command == ' ' || *command == '\t' || *command == '\n') command++;
        char* end = command + strlen(command) - 1;
        while (end > command && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        // Trim whitespace from description
        while (*description == ' ' || *description == '\t' || *description == '\n') description++;
        end = description + strlen(description) - 1;
        while (end > description && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
    } else {
        // Fallback if no separator found
        description = "Generated shell command";
        
        // Trim whitespace from command
        while (*command == ' ' || *command == '\t' || *command == '\n') command++;
        char* end = command + strlen(command) - 1;
        while (end > command && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
    }
    
    // Display in tree format
    printf("\033[1;36m◇  Command for:\033[0m\n");
    printf("│\n");
    printf("│  %s\n", description ? description : "Generated shell command");
    printf("│\n");
    printf("\033[1;36m◆  Command copied to clipboard:\033[0m\n");
    printf("│\n");
    printf("└  %s\n", command);
    
    if (!copy_success) {
        printf("\n\033[1;36m◆  Failed to copy to clipboard:\033[0m\n");
        printf("│\n");
        printf("└  Command is shown above\n");
    }
    
    free(output_copy);
}

char* generate_command(const char* natural_language, const char* gcli_path, const char* model,
                      const char* temp, const char* prompt, const char* shell, int verbose) {
    // Create temporary files for input and prompt
    char temp_input_file[] = "/tmp/gcmd_input_XXXXXX";
    char temp_prompt_file[] = "/tmp/gcmd_prompt_XXXXXX";

    int input_fd = mkstemp(temp_input_file);
    if (input_fd == -1) {
        fprintf(stderr, "Error: Failed to create temporary input file\n");
        return NULL;
    }

    int prompt_fd = mkstemp(temp_prompt_file);
    if (prompt_fd == -1) {
        fprintf(stderr, "Error: Failed to create temporary prompt file\n");
        close(input_fd);
        unlink(temp_input_file);
        return NULL;
    }

    // Build the full prompt
    char full_prompt[MAX_PROMPT_SIZE];
    if (prompt) {
        snprintf(full_prompt, sizeof(full_prompt), "%s", prompt);
    } else {
        snprintf(full_prompt, sizeof(full_prompt), "%s", DEFAULT_PROMPT);
    }

    // Add shell-specific instructions if specified
    if (shell) {
        char shell_addition[256];
        snprintf(shell_addition, sizeof(shell_addition),
                " Generate commands for %s shell syntax.", shell);
        strncat(full_prompt, shell_addition, sizeof(full_prompt) - strlen(full_prompt) - 1);
    }

    // Write the natural language request and prompt to temporary files
    write(input_fd, natural_language, strlen(natural_language));
    write(prompt_fd, full_prompt, strlen(full_prompt));
    close(input_fd);
    close(prompt_fd);

    if (verbose) {
        printf("=== Prompt being sent to AI ===\n");
        printf("%s\n", full_prompt);
        printf("=== Natural language input ===\n");
        printf("%s\n", natural_language);
        printf("===============================\n\n");
    }

    // Build the gcli command
    char command[MAX_BUFFER_SIZE];
    int cmd_len = snprintf(command, sizeof(command),
        "cat '%s' | %s -q -e -m '%s' -t %s \"$(cat '%s')\"",
        temp_input_file, gcli_path, model, temp, temp_prompt_file);

    if (cmd_len >= (int)sizeof(command)) {
        fprintf(stderr, "Error: Command too long\n");
        unlink(temp_input_file);
        unlink(temp_prompt_file);
        return NULL;
    }

    // Execute the command and capture output
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute gcli command\n");
        unlink(temp_input_file);
        unlink(temp_prompt_file);
        return NULL;
    }

    char* result = malloc(MAX_COMMAND_SIZE);
    if (!result) {
        pclose(pipe);
        unlink(temp_input_file);
        unlink(temp_prompt_file);
        return NULL;
    }

    size_t total_read = 0;
    size_t bytes_read;

    while ((bytes_read = fread(result + total_read, 1, MAX_COMMAND_SIZE - total_read - 1, pipe)) > 0) {
        total_read += bytes_read;
        if (total_read >= MAX_COMMAND_SIZE - 1) {
            break;
        }
    }

    result[total_read] = '\0';

    int exit_status = pclose(pipe);

    // Cleanup temporary files
    unlink(temp_input_file);
    unlink(temp_prompt_file);

    if (WEXITSTATUS(exit_status) != 0) {
        fprintf(stderr, "Error: Failed to generate command\n");
        free(result);
        return NULL;
    }

    // Clean up the result (remove trailing whitespace and newlines)
    char* end = result + strlen(result) - 1;
    while (end > result && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }

    if (strlen(result) == 0) {
        fprintf(stderr, "Error: No command generated\n");
        free(result);
        return NULL;
    }

    return result;
}

int main(int argc, char* argv[]) {
    char* model = "gemini-1.5-pro-latest";
    char* temp = "0.3";  // Lower temperature for more consistent commands
    char* prompt_file = NULL;
    char* gcli_path = "gcli";
    char* shell = NULL;
    int execute = 0;
    int copy = 0;
    int quiet = 0;
    int verbose = 0;
    int dry_run = 0;

    // Parse command line arguments
    int arg_start = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) {
                model = argv[++i];
                arg_start = i + 1;
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--temp") == 0) {
            if (i + 1 < argc) {
                temp = argv[++i];
                arg_start = i + 1;
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 < argc) {
                prompt_file = argv[++i];
                arg_start = i + 1;
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gcli") == 0) {
            if (i + 1 < argc) {
                gcli_path = argv[++i];
                arg_start = i + 1;
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--shell") == 0) {
            if (i + 1 < argc) {
                shell = argv[++i];
                arg_start = i + 1;
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--execute") == 0) {
            execute = 1;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--copy") == 0) {
            copy = 1;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
            arg_start = i + 1;
        } else {
            // This is the start of the natural language command
            arg_start = i;
            break;
        }
    }

    // Collect the natural language command from remaining arguments
    if (arg_start >= argc) {
        fprintf(stderr, "Error: No natural language command provided\n");
        fprintf(stderr, "Use -h or --help for usage information.\n");
        return 1;
    }

    // Join all remaining arguments into a single natural language string
    char natural_language[MAX_BUFFER_SIZE] = "";
    for (int i = arg_start; i < argc; i++) {
        if (i > arg_start) {
            strcat(natural_language, " ");
        }
        strcat(natural_language, argv[i]);
    }

    if (strlen(natural_language) == 0) {
        fprintf(stderr, "Error: Empty natural language command\n");
        return 1;
    }

    // Read custom prompt if specified
    char* prompt = NULL;
    if (prompt_file) {
        prompt = read_file(prompt_file);
        if (!prompt) {
            fprintf(stderr, "Error: Failed to read prompt file '%s'\n", prompt_file);
            return 1;
        }
    }

    if (!quiet && shell) {
        printf("Target shell: %s\n", shell);
        printf("\n");
    }

    // Generate the command
    char* generated_command = generate_command(natural_language, gcli_path, model, temp, prompt, shell, verbose);

    if (prompt) {
        free(prompt);
    }

    if (!generated_command) {
        return 1;
    }

    // Handle different output modes
    if (quiet) {
        // Parse the command from the generated output for quiet mode
        char* output_copy = strdup(generated_command);
        char* command_only = output_copy;
        char* separator = strstr(output_copy, "|||");
        if (separator) {
            *separator = '\0';
            // Trim whitespace from command
            while (*command_only == ' ' || *command_only == '\t' || *command_only == '\n') command_only++;
            char* end = command_only + strlen(command_only) - 1;
            while (end > command_only && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end = '\0';
                end--;
            }
        }
        
        if (execute) {
            // Execute silently and only show command output
            int result = system(command_only);
            free(output_copy);
            free(generated_command);
            return WEXITSTATUS(result);
        } else {
            // Just show the command
            printf("%s\n", command_only);
            free(output_copy);
            free(generated_command);
            return 0;
        }
    }

    if (dry_run) {
        // Parse the command from the generated output for dry run display
        char* output_copy = strdup(generated_command);
        char* command_only = output_copy;
        char* description = NULL;
        char* separator = strstr(output_copy, "|||");
        if (separator) {
            *separator = '\0';
            description = separator + 3;
            
            // Trim whitespace from command
            while (*command_only == ' ' || *command_only == '\t' || *command_only == '\n') command_only++;
            char* end = command_only + strlen(command_only) - 1;
            while (end > command_only && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
            // Trim whitespace from description
            while (*description == ' ' || *description == '\t' || *description == '\n') description++;
            end = description + strlen(description) - 1;
            while (end > description && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end = '\0';
                end--;
            }
        } else {
            description = "Generated shell command";
            // Trim whitespace from command
            while (*command_only == ' ' || *command_only == '\t' || *command_only == '\n') command_only++;
            char* end = command_only + strlen(command_only) - 1;
            while (end > command_only && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end = '\0';
                end--;
            }
        }
        
        // Display in tree format for dry run
        printf("\033[1;36m◇  Command for:\033[0m\n");
        printf("│\n");
        printf("│  %s\n", description ? description : "Generated shell command");
        printf("│\n");
        printf("\033[1;36m◆  Dry run - command not executed:\033[0m\n");
        printf("│\n");
        printf("└  %s\n", command_only);
        
        free(output_copy);
        free(generated_command);
        return 0;
    }

    // Determine action based on flags
    int action = 2;  // Default to copy (2=copy)

    if (execute && copy) {
        fprintf(stderr, "Error: Cannot use both --execute and --copy flags\n");
        free(generated_command);
        return 1;
    }

    if (execute) {
        action = 1;  // Execute
    } else {
        action = 2;  // Copy (default)
    }

    int result = 0;

    // Parse the command from the generated output for execution/copying
    char* output_copy = strdup(generated_command);
    char* command_only = output_copy;
    char* separator = strstr(output_copy, "|||");
    if (separator) {
        *separator = '\0';
        // Trim whitespace from command
        while (*command_only == ' ' || *command_only == '\t' || *command_only == '\n') command_only++;
        char* end = command_only + strlen(command_only) - 1;
        while (end > command_only && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
    }

    switch (action) {
        case 1: { // Execute
            // Parse and display command description, then execute
            char* exec_copy = strdup(generated_command);
            char* exec_description = NULL;
            char* exec_separator = strstr(exec_copy, "|||");
            if (exec_separator) {
                *exec_separator = '\0';
                exec_description = exec_separator + 3;
                
                // Trim whitespace from description
                while (*exec_description == ' ' || *exec_description == '\t' || *exec_description == '\n') exec_description++;
                char* end = exec_description + strlen(exec_description) - 1;
                while (end > exec_description && (*end == ' ' || *end == '\t' || *end == '\n')) {
                    *end = '\0';
                    end--;
                }
            } else {
                exec_description = "Generated shell command";
            }
            
            printf("\033[1;36m◇  Command for:\033[0m\n");
            printf("│\n");
            printf("│  %s\n", exec_description ? exec_description : "Generated shell command");
            printf("│\n");
            printf("\033[1;36m◆  Executing command:\033[0m\n");
            printf("│\n");
            printf("└  %s\n\n", command_only);
            
            result = system(command_only);
            if (WEXITSTATUS(result) != 0) {
                printf("Command failed with exit code %d\n", WEXITSTATUS(result));
            }
            
            free(exec_copy);
            break;
        }

        case 2:  // Copy
            result = copy_to_clipboard(command_only);
            display_command_result(generated_command, result == 0);
            break;

        case 3: { // Show only
            // Parse and display in tree format
            char* desc_copy = strdup(generated_command);
            char* desc_description = NULL;
            char* desc_separator = strstr(desc_copy, "|||");
            if (desc_separator) {
                *desc_separator = '\0';
                desc_description = desc_separator + 3;
                
                // Trim whitespace from description
                while (*desc_description == ' ' || *desc_description == '\t' || *desc_description == '\n') desc_description++;
                char* end = desc_description + strlen(desc_description) - 1;
                while (end > desc_description && (*end == ' ' || *end == '\t' || *end == '\n')) {
                    *end = '\0';
                    end--;
                }
            } else {
                desc_description = "Generated shell command";
            }
            
            printf("\033[1;36m◇  Command for:\033[0m\n");
            printf("│\n");
            printf("│  %s\n", desc_description ? desc_description : "Generated shell command");
            printf("│\n");
            printf("\033[1;36m◆  Generated command:\033[0m\n");
            printf("│\n");
            printf("└  %s\n", command_only);
            printf("\n(Use -e to execute or -c to copy)\n");
            
            free(desc_copy);
            break;
        }

        default:  // Quit
            printf("Operation cancelled.\n");
            break;
    }

    free(output_copy);
    free(generated_command);
    return result;
}