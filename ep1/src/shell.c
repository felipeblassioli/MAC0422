#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

int g_done;
char *g_current_working_directory = (char *)NULL;
char g_prompt[1024];

void set_working_directory(char *name)
{
	free(g_current_working_directory);
	g_current_working_directory = strcpy((char *)(malloc(1 + strlen(name))), name);
}

void update_working_directory()
{
	char cwd[1024];
	getcwd(cwd,sizeof(cwd));
	set_working_directory(cwd);
}

char *get_working_directory()
{
	if(!g_current_working_directory)
		update_working_directory();
	return g_current_working_directory;
}

typedef struct {
	char *name;
	rl_icpfunc_t *func;
	char *doc;
} COMMAND;

int com_cd (char *);
int com_pwd (char *);
int com_ls (char *);

COMMAND g_commands[] = {
	{ "cd", com_cd, "Change to directory DIR" },
	{ "pwd", com_pwd, "Print the current working directory" },
	{ "ls", com_ls, "List files in DIR" }
};

int is_whitespace(char c)
{
	int i;
	char WHITESPACE_CHARS[] = { ' ', '\t', '\n', '\v', '\f', '\r', '\0' };
	
	i = 0;
	while(WHITESPACE_CHARS[i])
		if( c == WHITESPACE_CHARS[i++] )
			return 1;
	return 0;
}

char * stripwhite(char *string)
{
	char *s, *t;
	for(s = string; is_whitespace(*s); s++);

	if(*s == 0)
		return s;
	t = s + strlen(s) - 1;
	while(t > s && is_whitespace(*t))
		t--;
	*++t = '\0';

	return string;
}

int com_cd (char *arg)
{
	if(!arg)
		arg = "";
	char *new_dir = arg;
	if(chdir(new_dir) == 0){
		/*set_working_directory(new_dir);*/
		update_working_directory();
	}
	return 1;
}

int com_ls (char *arg)
{
	if(!arg)
		arg = "";
	pid_t parent = getpid();
	pid_t pid = fork();

	if(pid == -1){
		fprintf(stderr, "FATAL: failed to fork.\n");
		exit(2);
	}
	else if(pid > 0){
			int status;
			waitpid(pid, &status, 0);
	} else {
		char *argv[] = { "/bin/ls", "-l", 0 };
		char *envp[] = { 0 };
		execve(argv[0], &argv[0], envp);
	}
	return 1;
}

int com_pwd (char *arg){
	if(!arg)
		arg = "";

	printf("%s\n", get_working_directory());

	return 1;
}

COMMAND *find_command(char *name)
{
	int i;
	for(i = 0; g_commands[i].name; i++)
		if(strcmp(name, g_commands[i].name) == 0)
			return (&g_commands[i]);
	return ((COMMAND *)NULL);
}

int execute_line(char *line)
{
	int i;
	COMMAND *command;
	char *word;

	i = 0;
	while(line[i] && is_whitespace(line[i]))
		i++;
	word = line + i;

	while(line[i] && !is_whitespace(line[i]))
		i++;

	if(line[i])
		line[i++] = '\0';

	command = find_command(word);
	if(!command)
	{
		fprintf(stderr, "%s: No such command.\n", word);
		return -1;
	}

	while(is_whitespace(line[i]))
		i++;
	word = line + i;

	return ((*(command->func))(word));
}

int main()
{
	char *line, *s;

	g_done = 0;
	while(!g_done)
	{
		sprintf(g_prompt, "[%s] ", get_working_directory());
		line = readline(g_prompt);
		if(!line)
			break;

		s = stripwhite(line);
		if(*s)
		{
			add_history(s);
			execute_line(s);
		}
		free(line);
	}
	return 0;
}
