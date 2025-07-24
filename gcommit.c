#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_BUFFER_SIZE 65536
#define MAX_PROMPT_SIZE 4096

// Default prompt for conventional commits
static const char* DEFAULT_PROMPT =
    "You are an expert at following the Conventional Commit specification. "
    "Given the git diff listed below, please generate a commit message for me: "
    "1. First line: conventional commit format (type: concise description) "
    "(remember to use semantic types like feat, fix, docs, style, refactor, perf, test, chore, etc.) "
    "2. Optional bullet points if more context helps: "
    "- Keep the second line blank "
    "- Keep them short and direct "
    "- Focus on what changed "
    "- Always be terse "
    "- Don't overly explain "
    "- Drop any fluffy or formal language "
    "Return ONLY the commit message - no introduction, no explanation, no quotes around it. "
    "Examples: "
    "feat: add user auth system\n\n"
    "- Add JWT tokens for API auth\n"
    "- Handle token refresh for long sessions\n\n"
    "fix: resolve memory leak in worker pool\n\n"
    "- Clean up idle connections\n"
    "- Add timeout for stale work\n\n"
    "Simple change example: "
    "fix: typo in README.md "
    "Very important: Do not respond with any of the examples. "
    "Your message must be based off the diff that is about to be provided, "
    "with a little bit of styling informed by the recent commits you're about to see. "
    "Based on this format, generate appropriate commit messages. "
    "Respond with message only. "
    "DO NOT format the message in Markdown code blocks, DO NOT use backticks";

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Generate conventional commit messages using AI based on staged git changes.\n\n");
    printf("OPTIONS:\n");
    printf("    -m, --model MODEL       Specify the AI model (default: gemini-1.5-pro-latest)\n");
    printf("    -t, --temp TEMP         Set temperature (default: 0.7)\n");
    printf("    -p, --prompt FILE       Use custom prompt file\n");
    printf("    -g, --gcli PATH         Path to gcli binary (default: gcli)\n");
    printf("    -v, --verbose           Show the diff being sent to AI\n");
    printf("    -h, --help              Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("    %s                                 # Basic usage\n", program_name);
    printf("    %s -m gemini-1.5-flash           # Use different model\n", program_name);
    printf("    %s -t 0.3                        # Lower temperature for more focused output\n", program_name);
    printf("    %s -p custom-prompt.txt          # Use custom prompt\n", program_name);
    printf("    %s -v                             # Show what's being sent to AI\n", program_name);
    printf("    %s -v                             # Show verbose output\n\n", program_name);
    printf("REQUIREMENTS:\n");
    printf("    - git repository with staged changes\n");
    printf("    - gcli installed and configured\n");
    printf("    - Internet connection\n\n");
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

int check_git_repo() {
    int status = system("git rev-parse --git-dir > /dev/null 2>&1");
    return WEXITSTATUS(status) == 0;
}

int has_staged_changes() {
    int status = system("git diff --staged --quiet");
    return WEXITSTATUS(status) != 0; // Returns 1 if there are changes
}

char* get_staged_diff() {
    FILE* pipe = popen("git diff --staged", "r");
    if (!pipe) {
        return NULL;
    }

    char* buffer = malloc(MAX_BUFFER_SIZE);
    if (!buffer) {
        pclose(pipe);
        return NULL;
    }

    size_t total_read = 0;
    size_t bytes_read;

    while ((bytes_read = fread(buffer + total_read, 1, MAX_BUFFER_SIZE - total_read - 1, pipe)) > 0) {
        total_read += bytes_read;
    }

    buffer[total_read] = '\0';
    pclose(pipe);

    if (total_read == 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

int main(int argc, char* argv[]) {
    char* model = "gemini-1.5-pro-latest";
    char* temp = "0.7";
    char* prompt_file = NULL;
    char* gcli_path = "gcli";
    int verbose = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) {
                model = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--temp") == 0) {
            if (i + 1 < argc) {
                temp = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 < argc) {
                prompt_file = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gcli") == 0) {
            if (i + 1 < argc) {
                gcli_path = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            fprintf(stderr, "Use -h or --help for usage information.\n");
            return 1;
        }
    }

    // Check if we're in a git repository
    if (!check_git_repo()) {
        fprintf(stderr, "Error: Not in a git repository\n");
        return 1;
    }

    // Check if there are staged changes
    if (!has_staged_changes()) {
        fprintf(stderr, "No staged changes found. Stage some changes first with 'git add'.\n");
        return 1;
    }

    // Get the staged diff
    char* diff = get_staged_diff();
    if (!diff) {
        fprintf(stderr, "Error: Failed to get staged changes\n");
        return 1;
    }

    if (verbose) {
        printf("=== Staged Changes ===\n");
        printf("%s", diff);
        printf("======================\n\n");
    }

    // Determine prompt to use
    char* prompt = NULL;
    if (prompt_file) {
        prompt = read_file(prompt_file);
        if (!prompt) {
            fprintf(stderr, "Error: Failed to read prompt file '%s'\n", prompt_file);
            free(diff);
            return 1;
        }
    } else {
        prompt = strdup(DEFAULT_PROMPT);
    }

    // Build the gcli command - use temporary files to avoid shell escaping issues
    char temp_diff_file[] = "/tmp/gcommit_diff_XXXXXX";
    char temp_prompt_file[] = "/tmp/gcommit_prompt_XXXXXX";

    int diff_fd = mkstemp(temp_diff_file);
    if (diff_fd == -1) {
        fprintf(stderr, "Error: Failed to create temporary diff file\n");
        free(diff);
        free(prompt);
        return 1;
    }

    int prompt_fd = mkstemp(temp_prompt_file);
    if (prompt_fd == -1) {
        fprintf(stderr, "Error: Failed to create temporary prompt file\n");
        close(diff_fd);
        unlink(temp_diff_file);
        free(diff);
        free(prompt);
        return 1;
    }

    // Write diff and prompt to temporary files
    write(diff_fd, diff, strlen(diff));
    write(prompt_fd, prompt, strlen(prompt));
    close(diff_fd);
    close(prompt_fd);

    char command[8192];
    int cmd_len = snprintf(command, sizeof(command),
        "cat '%s' | %s -q -e -m '%s' -t %s \"$(cat '%s')\"",
        temp_diff_file, gcli_path, model, temp, temp_prompt_file);

    if (cmd_len >= (int)sizeof(command)) {
        fprintf(stderr, "Error: Command too long\n");
        free(diff);
        free(prompt);
        return 1;
    }

    if (verbose) {
        printf("Executing: %s\n\n", command);
    }

    // Execute the command
    int result = system(command);

    // Cleanup
    unlink(temp_diff_file); // Remove temporary files
    unlink(temp_prompt_file);
    free(diff);
    free(prompt);

    if (WEXITSTATUS(result) != 0) {
        fprintf(stderr, "Error: Failed to generate commit message\n");
        return 1;
    }

    return 0;
}
