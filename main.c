#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define MAX_ARGS 50 /* Maximum arguments that a command supports */
#define MAX_ALIASES 50 /* Maximum number of aliases */
#define MAX_VARIABLES 50 /* Maximum number of variables */
#define MAX_VARIABLE_NAME 20 /* Maximum length of variable name */
#define MAX_VARIABLE_VALUE 100 /* Maximum length of variable value */

/* Struct to store alias data */
typedef struct {
	char *alias_name;
	char *command;
} Alias;

/* Struct to store variable data */
typedef struct {
	char name[MAX_VARIABLE_NAME];
	char value[MAX_VARIABLE_VALUE];
} Variable;

/* Function prototypes */
char *read_input(FILE *stream);
int execute_command(char *command, char **args);
int string_token(char *input_string, const char *delimiters, char **args);
int is_builtin_command(char *command);
void handle_alias_command(char **args, Alias *aliases, int *alias_count);
int is_interactive_mode(void);
int is_variable(char *input_string, const char *variable_name);
void replace_variables(char **args, Variable *variables, int variable_count);
void handle_dollar_dollar(char **args);
void execute_shell(FILE *stream);
void free_args(char **args);

/* Function to read input from a file or pipe */
char *read_input(FILE *stream) {
	char *buf = NULL;
	size_t bufsize = 0;
	ssize_t read;

	/* Check if the input stream is not a terminal (e.g., pipe) */
	if (!is_interactive_mode()) {
		/* Read the entire input from the file or pipe */
		read = getdelim(&buf, &bufsize, EOF, stream);
	} else {
		/* Read a line of input from the user */
		read = getline(&buf, &bufsize, stream);
	}

	if (read == -1) {
		free(buf);
		return NULL;
	}
	return buf;
}

/* Function to execute a command */
int execute_command(char *command, char **args) {
    int status;
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork Error");
        return 1;
    } else if (pid == 0) {
        /* Child process: execute the command */
        if (execvp(command, args) == -1) {
            perror("Command not found");
            exit(127); 
        }
    } else {
        /* Parent process: wait for the child to finish executing */
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1; /* Command execution failed */
        }
    }
    return (1);/*default exit status */
}

/* Function to tokenize the input string */
int string_token(char *input_string, const char *delimiters, char **args) {
    size_t token_len;
    int numtoken = 0;

    if (input_string == NULL) {
        perror("String not found");
        return 0;
    }

    /* Skip leading delimiters */
    input_string += strspn(input_string, delimiters);

    while (*input_string) {
        /* Find the end of the token */
        token_len = strcspn(input_string, delimiters);

        if (token_len > 0) {
            char *token = malloc(token_len + 1);
            if (token == NULL) {
                perror("Memory allocation error");
                return 0;
            }

            strncpy(token, input_string, token_len);
            token[token_len] = '\0';

            args[numtoken] = token;
            numtoken++;

            if (numtoken >= MAX_ARGS) {
                fprintf(stderr, "Too many arguments. Maximum allowed: %d\n", MAX_ARGS);
                free_args(args); /* Free memory for allocated arguments */
                break;
            }
        }
        /* Move to the next token */
        input_string += token_len + strspn(input_string + token_len, delimiters);
    }

    args[numtoken] = NULL;

    return numtoken;
}

/* Function to check if the command is a built-in command */
int is_builtin_command(char *command) {
	if (strcmp(command, "exit") == 0) {
		/* Handle "exit" command */
		exit(EXIT_SUCCESS); /* Terminate the shell */
	} else if (strcmp(command, "alias") == 0) {
		/* Handle "alias" command */
		return 1;
	}

	return 0; /* Command is not a built-in command, continue with external execution */
}

/* Function to check if the shell is running in interactive mode */
int is_interactive_mode(void) {
	return isatty(STDIN_FILENO);
}

/* Function to handle the "alias" built-in command */
void handle_alias_command(char **args, Alias *aliases, int *alias_count) 
{
	int i;
	if (args[1] == NULL) {
		/* No arguments provided, list all aliases */
		for (i = 0; i < *alias_count; i++) {
			printf("alias %s='%s'\n", aliases[i].alias_name, aliases[i].command);
		}
	} else if (args[2] == NULL) {
		/* Single argument provided, show the alias if it exists */
		for (i = 0; i < *alias_count; i++) {
			if (strcmp(args[1], aliases[i].alias_name) == 0) {
				printf("alias %s='%s'\n", aliases[i].alias_name, aliases[i].command);
				return;
			}
		}
		printf("Alias '%s' not found.\n", args[1]);
	} else {
		/* Two arguments provided, define or redefine an alias */
		if (*alias_count >= MAX_ALIASES) {
			fprintf(stderr, "Too many aliases. Maximum allowed: %d\n", MAX_ALIASES);
			return;
		}

		aliases[*alias_count].alias_name = strdup(args[1]);
		aliases[*alias_count].command = strdup(args[2]);

		if (aliases[*alias_count].alias_name == NULL || aliases[*alias_count].command == NULL) {
			perror("Memory allocation error");
			return;
		}

		(*alias_count)++;
	}
}

/* Function to handle comments */
void handle_comment(char *input_string) {
	char *comment_start = strchr(input_string, '#');
	if (comment_start != NULL) {
		*comment_start = '\0'; 
		/* Replace '#' with '\0' to terminate the string at the comment start */
	}
}

/* Function to check if a string is a variable */
int is_variable(char *input_string, const char *variable_name) {
	size_t variable_name_len = strlen(variable_name);
	return strncmp(input_string, variable_name, variable_name_len) == 0 &&
		input_string[variable_name_len] == '\0';
}

/* Function to replace variables in command arguments */
void replace_variables(char **args, Variable *variables, int variable_count) 
{
	int i;
	int j;
	for (i = 0; args[i] != NULL; i++) {
		for (j = 0; j < variable_count; j++) {
			if (is_variable(args[i], variables[j].name)) {
				/* Replace the variable with its value */
				free(args[i]);
				args[i] = strdup(variables[j].value);
				if (args[i] == NULL) {
					perror("Memory allocation error");
				}
			}
		}
	}
}

/* Function to handle the $$ variable */
void handle_dollar_dollar(char **args) 
{
        int i;
	for (i = 0; args[i] != NULL; i++) {
		if (is_variable(args[i], "$$")) {
			/* Replace the $$ with the current process ID */
			char pid_str[20];
			snprintf(pid_str, sizeof(pid_str), "%d", getpid());
			free(args[i]);
			args[i] = strdup(pid_str);
			if (args[i] == NULL) {
				perror("Memory allocation error");
			}
		}
	}
}

/* Function to execute a shell with a given input stream */
void execute_shell(FILE *stream) {
	int i;
	char *buf;
	int num_tokens;
	int alias_count;
	int variable_count;
	int status;
	Alias aliases[MAX_ALIASES];
	Variable variables[MAX_VARIABLES];
	char *command = NULL;
	char *op = NULL;
	char prompt[] = "$ ";

	/* Default delimiters if not provided through command-line arguments */
	const char default_delimiters[] = " \t\n";

	const char *delimiters = default_delimiters; /* Initialize delimiters */

	if (is_interactive_mode()) {
		write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
		/* Print the prompt on a new line in interactive mode */
	}

	alias_count = 0;

	variable_count = 0;

	while (1) {
		/* Declare the args array to hold command arguments */
		char *args[MAX_ARGS + 1] = { NULL }; /* Initialize all elements to NULL */
		buf = read_input(stream);

		if (buf == NULL) {
			break;
		}

		/* Remove the newline character from the input_string */
		buf[strcspn(buf, "\n")] = '\0';

		/* Handle comments in the input */
		handle_comment(buf);

		/* Skip processing if the command is a comment or an empty line */
		if (strlen(buf) == 0) {
			free(buf);
			continue;
		}

		/* Split commands based on ';' as a delimiter */
		command = strtok(buf, ";");
		while (command != NULL) {
			/* Prevent processing empty commands or excessively long commands */
			if (strlen(command) > 0 && strlen(command) < 1024) {
				num_tokens = string_token(command, delimiters, args);

				if (num_tokens > 0) {
					/* Check if the command is a comment */
					if (is_variable(args[0], "#")) {
						break; /* Skip the comment line */
					}

					/* Check if the command is a built-in command */
					if (is_builtin_command(args[0])) {
						if (strcmp(args[0], "alias") == 0) {
							handle_alias_command(args, aliases, &alias_count);
						}
						command = strtok(NULL, ";"); /* Move to the next command */
						continue;
					}

					/* Replace variables in the command */
					replace_variables(args, variables, variable_count);

					/* Handle the $$ variable */
					handle_dollar_dollar(args);

					/* Execute the command and consider logical operators */
					status = execute_command(args[0], args);

					/* Check if there is an AND (&&) or OR (||) operator in the command */
					op = strtok(NULL, "&|");
					while (op != NULL) {
						int is_and = strcmp(op, "&&") == 0;
						int is_or = strcmp(op, "||") == 0;

						if (is_and && status != 0) {
							break; /* Skip the next command after '&&' if the previous command failed */
						} else if (is_or && status == 0) {
							break; /* Skip the next command after '||' if the previous command succeeded */
						}

						/* Execute the next command after '&&' or '||' */
						command = strtok(NULL, ";");
						if (command != NULL) {
							/* Replace variables in the command */
							replace_variables(args, variables, variable_count);

							/* Handle the $$ variable */
							handle_dollar_dollar(args);

							status = execute_command(args[0], args);
							op = strtok(NULL, "&|");
						} else {
							op = NULL;
						}
					}
				} else {
					fprintf(stderr, "Invalid input length.\n");
				}
			}

			command = strtok(NULL, ";"); /* Move to the next command */
		}

		free_args(args); /* Free allocated memory for arguments */
		free(buf); /* Don't forget to free the allocated memory! */

		if (is_interactive_mode() && (buf[0] != '\n')) {

			write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
			/* Print the prompt again after executing commands */
		}
	}

	/* Free memory allocated for aliases */
	for (i = 0; i < alias_count; i++) {
		free(aliases[i].alias_name);
		free(aliases[i].command);
	}
}

/* Function to free memory allocated for arguments */
void free_args(char **args) 
{
	int i;
	for (i = 0; args[i] != NULL; i++) {
		free(args[i]);
	}
}

/* Main function */
int main(int argc, char *argv[]) {
	if (argc > 1) {
		/* File provided as input, attempt to open it */
		FILE *file = fopen(argv[1], "r");
		if (file == NULL) {
			perror("Error opening file");
			exit(EXIT_FAILURE);
		}

		execute_shell(file); /* Execute the shell with the file as input */
		fclose(file);
	} else {
		/* No file provided, execute the shell with standard input */
		execute_shell(stdin);
	}

	return 0;
}
