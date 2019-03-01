#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#define APPND O_WRONLY|O_APPEND
#define OUT O_CREAT|O_WRONLY|O_TRUNC
#define IN O_RDONLY
enum condition{
	text,
	input, 
	output, 
	append,  
	error,
	file,
	outfile,
	infile,
	appendfile,  
	pipeline,
	background
};
struct list
{
	struct list *next;
	char *cmd;
	int type;
};
/* universal flags  */
struct flag
{
	int err;
	int eof, eol;
	int quotes, bg, file, pipes;
	int fout, fin, fappend;
	int pipescount;
	char *append, *output, *input;
};
/* find error func */
int finderr(struct flag *status)
{
	if(status->err){
		if(status->bg == error)
			fprintf(stderr, "-sh: expected new line after '&'\n");
		if(status->quotes == error)
			fprintf(stderr, "-sh: expected \" \n");
		if(status->file == error)
			fprintf(stderr, "-sh: unexpected file or command\n");
		if(status->pipes == error)
			fprintf(stderr, "-sh: unexpected file or  command\n");
		return 1;
	}
	return 0;
}
/* compare str1 and str2*/
int cmpstr(char   *str1,  char *str2)
{
	int tmp = 1;
	while(*str1++ && *str2++){
		if(*str1 != *str2){
			tmp = 0;
			break;
		}
	}
	return tmp;
}
/* print enums */
void printtype(int x)
{
	switch(x){
		case text:
			printf("text\n");
			break;
		case input:
			printf("input\n");
			break;
		case output:
			printf("output\n");
			break;
		case append:
			printf("append\n");
			break;
		case error:
			printf("error\n");
			break;
		case appendfile:
			printf("append file\n");
			break;
		case outfile:
			printf("output file\n");
			break;
		case infile:
			printf("input file\n");
			break;
		case pipeline:
			printf("pipe\n");
			break;
		case background:
			printf("background\n");
			break;
		default:
			;
	}
}
/* print list elements */
void printlist(struct list *p)
{	
	struct list *q = p;
	while(q != NULL){
		printf("[%s] --", q->cmd);
		printtype(q->type);
		q = q->next;
	}
}
/* wait for bg processes, print shell message*/
void shellbar(struct flag *status)
{
	int p, inhome;
	char *logname = getenv("LOGNAME");
	char *home = getenv("HOME");
	char cwd[1024], *directory, *ch;
	if(!home){
		fprintf(stderr, "-sh: home dir not found\n");
		home = "~";
	}
	if(!logname){
		fprintf(stderr, "-sh: logname not found\n");
		logname = "shelluser";
	}
	if(getcwd(cwd, sizeof(cwd)) != NULL){
		if((inhome = cmpstr(home, cwd))){
			directory = "~";
		}
		else{
			ch = cwd;
			directory = cwd;
			while(*ch != '\0'){
				ch++;
				if(*ch == '/')
					directory = ch + 1;
			}
		}
	}
	else{
		perror("getcwd() error");
		cwd[0] = '~';
		cwd[1] = '\0';
	}
	while((p = wait4(-1, NULL, WNOHANG, NULL)) > 0)
		printf("[%d] Done\n", p);
	if(inhome)
		printf("%s:%s$ ", logname, directory);
	else
		printf("%s:~/%s$ ", logname, directory);
}
/* string length function */
int cmdlen(char * const line)
{
	int i = 0;
	while(line[i] != '\0')
		i++;
	return i;
}
/* expand memory for cmd char massive */
char *expandcmd(char *cmd, const int size_old, const int size_new)
{
	char *newcmd;
	int i;
	newcmd = malloc(size_new * sizeof(char));
	for(i = 0; i < size_old; i++)
		newcmd[i] = cmd[i];
	free(cmd);
	cmd = newcmd;
	return cmd;
}
struct list *makesym(int *cmdtype)
{
		struct list *p;
		p = malloc(sizeof(struct list)); 
		p->next = NULL;
		p->type = *cmdtype;
		if(*cmdtype == input)
			p->cmd = "<";
		if(*cmdtype == pipeline)
			p->cmd = "|";
		if(*cmdtype == background)
			p->cmd = "&";
		if(*cmdtype == output){
			p->cmd = ">";
		}
		else
			*cmdtype = text;	
		return p;
}
/* add special symbols as new elems to the list */
struct list *syscmd(struct list *p, int *cmdtype)
{
	struct list *prev, *current;
	prev = p;
	if(*cmdtype == text)
		return p;
	if(p == NULL){
		p = makesym(cmdtype);
	}
	else{
		while (prev->next != NULL)
			prev = prev->next;
		if(*cmdtype == append){
			prev->type = *cmdtype;
			prev->cmd = ">>";
			return p;
		}	
		current = makesym(cmdtype);
	}
	return p;
}
/* recognize and sort commands and files */
char *parser(struct flag *status, int *cmdtype)
{
	int i = 0, sizebuf = 16, c;
	int q = 0;
	char *p = malloc(sizebuf * sizeof(char));
	while((c = getchar()) != EOF){
		if((c == '\n') || (!q && ((c == ' ') || (c == '\t')))) {
			*cmdtype = text;
			if(c == '\n')
				status->eol = 1;
			break;
		}
		if(!status->err){
			if (c == '&' && !q){
				*cmdtype = background;
				break;
			}
			if (c == '|' && !q){
				*cmdtype = pipeline;
				break;
			}
			if(c == '<' && !q){
				*cmdtype = input;
				break;
			}
			if(c == '>' && !q){
				if(*cmdtype == text){
					*cmdtype = output;
					break;
				}
				if(*cmdtype == output){
					*cmdtype = append;
					break;
				}
			}
			*cmdtype = text;
			if(c == '"'){
				status->quotes = 1;
				q = !q;
				continue;
			}
			if(i == sizebuf){
				sizebuf += 16;
				p = expandcmd(p, i, sizebuf);
			}
			p[i] = c;
			i++;
		}
	}
	if(c == EOF)
		status->eof = 1;
	if (q){
		status->err = 1;
		status->quotes = error;
	}
	p[i] = '\0';
	return p;
}
/* add cmd to the command list */
struct list *makelist(struct flag *status)
{
	struct list *prev, *current;
	struct list *p = NULL;
	char *cmd;
	int cmdtype = text;	
	while(!(status->eol) && !(status->eof)){
		cmd = parser(status, &cmdtype);
			if (cmdlen(cmd) > 0){
				if (p != NULL){
					prev = p;
					while (prev->next != NULL)
						prev = prev->next;
					current = malloc(sizeof(struct list));
					current->next = NULL;
					current->cmd = cmd;
					current->type = text;
					prev->next = current;
					p = syscmd(p, &cmdtype);
				}
				else{
					p = malloc(sizeof(struct list));
					p->cmd = cmd;
					p->next = NULL;
					p->type = text;
					p = syscmd(p, &cmdtype);
				}
			}
			else{
				p = syscmd(p, &cmdtype);
				free(cmd);
			}
	}
	return p;
}
/* delete list */
void freelist(struct list *p)
{
	if(p != NULL){
		freelist(p->next);
		if(p->type == text && p->cmd)
			free(p->cmd);
		free(p);
	}
}
/* check list elements count */
int countlist(struct list * const p)
{
	int l = 0;
	struct list *q = p;
	while(q){
		l++;
		q = q->next;
	}
	return l;
}
/* print massive of strings */
void printcmd(char ** const cmdmas)
{
	int i = 0;
	while(cmdmas[i])
		printf("[%s]\n", cmdmas[i++]);
}
/* create massive of command from list */
char **makemas(struct list  * const cmdlist)
{
	int i = 0,  lsize;
	struct list *tmp = cmdlist;
	char **cmdmas;
	lsize = countlist(cmdlist);
	cmdmas = malloc((lsize + 1) * sizeof(char *));
	while(tmp != NULL){
		if(tmp->type == text)
			cmdmas[i++] = tmp->cmd;
		tmp = tmp->next;
	}
	cmdmas[i] = NULL;
	return cmdmas;
}
/*make massive of commands */
char ***makesupermas(struct list *cmdlist, struct flag *status)
{
	struct list *tmp = cmdlist;
	struct list *p = cmdlist;
	char ***supermas;
	char **cmdmas;
	int size;
	int j, i = 0;
	int k = status->pipescount + 1;
	supermas = malloc((k)*sizeof(char **));
	for(i = 0; i < k; i++){
		tmp = p;
		size = j = 0;
		while(tmp != NULL && tmp->type != pipeline){
			size++;
			tmp = tmp->next;
		}	
		cmdmas = malloc((size + 1)*sizeof(char *));
		while(p != NULL){
			if(p->type == pipeline || p->type == background){
				p = p->next;
				break;
			}
			if(p->type == text)
				cmdmas[j++] = p->cmd;
			p = p->next;
		}
		cmdmas[j] = NULL;
		supermas[i] = cmdmas;
		if(p == NULL)
			break;
	}
	return supermas;
}
/* check commands count */
int countmas(char ** const cmdmas){
	int i = 0;
	while(cmdmas[i] != NULL)
		i++;
	return i;
}
/* run new process */
void  runcmd(char **cmdmas, struct flag *status)
{
	int pid, k, fd;
	pid = fork();
	if(pid == -1){
		perror("fork");
		exit(1);
	}
	if((status->bg == 1) && (pid > 0))
		printf("[%d] Background\n", pid);
	if (pid == 0){
		if(status->file == 1){
			if(status->fout){
				fd = open(status->output, OUT , 0666);
				if(fd == -1){
					perror(status->output);
					exit(1);
				}
				dup2(fd, 1);
			}
			if(status->fappend){
				fd = open(status->append, APPND);
				if(fd == -1){
					perror(status->append);
					exit(1);
				}
				dup2(fd, 1);
			}
			if(status->fin){
				fd = open(status->input, IN);
				if(fd == -1){
					perror(status->input);
					exit(1);
				}
				dup2(fd, 0);
			}
			close(fd);
		}
		execvp(cmdmas[0], cmdmas);
		perror(cmdmas[0]);
		exit(1);
	}
	if(!status->bg){
		while((k = wait(NULL)) != pid)
			printf("[%d] Done\n", k);
	}

}
void runcd(char **cmdmas)
{
	int cmdnum = countmas(cmdmas);
	if (cmdnum != 2){
		if (cmdnum == 1){
			char *home = getenv("HOME");
			if(home)
				chdir(home);
			else
				fprintf(stderr, "-sh: home dir not found\n");
		}
		else
			fprintf(stderr, "-sh: %s: cmd not found\n", cmdmas[0]);
	}
	else{
		if(chdir(cmdmas[1]) == -1)
			perror(cmdmas[1]);
	}
}
void opnfiles(struct flag *status, int *s0, int *s1)
{
	char **cmdmas;
	int n = status->pipescount + 1, k = n, i = 0;
	int fd[2], pidmas[k+1], p, fdin;
	int save0, save1, dr;
		if(status->file == 1){	
		if(status->fappend){
			dr = open(status->append, APPND);
			if(dr == -1)
				perror(status->append);						
		}
		if(status->fout){
			dr = open(status->output, OUT, 0666);
			if(dr == -1)
				perror(status->append);	
		}
		if(dr != -1){
			*s1 = dup(1);
			dup2(dr, 1);
			close(dr);
		}
		if(status->fin){
			*s0 = dup(0);
			dr = open(status->input, IN);
			if(dr != -1){
				dup2(dr, 0);
				close(dr);
			}
		}
		if(dr == -1)
			return;
	}
}
void provchan(char *** const supermas, struct flag *status)
{
	char **cmdmas;
	int n = status->pipescount + 1, k = n, i = 0;
	int fd[2], pidmas[k+1], p, fdin;
	int save0, save1, dr;
	for(i = 0; i < n; i++){
		cmdmas = supermas[i];
		if(i != n-1)
			pipe(fd);
		p = fork();
		if((status->bg == 1) && (p > 0))
			printf("[%d] Background\n", p);
		if(p == 0){
			if(i == 0){
				dup2(fd[1], 1);
				close(fd[1]);
				close(fd[0]);
			}
			if(i == n-1){
				dup2(fdin, 0);
				close(fdin);
			}
			if(i > 0 && i < n-1){
				dup2(fdin, 0);
				close(fdin);
				dup2(fd[1], 1);
				close(fd[1]);
				close(fd[0]);
			}
			execvp(cmdmas[0], cmdmas);
			perror(cmdmas[0]);
			exit(1);
		}
		pidmas[i] = p;
		if(i > 0 && i < n-1){
			close(fdin);
			fdin = fd[0];
			close(fd[1]);
		}
		if(i == n-1)
			close(fdin);
		if(i == 0){
			fdin = fd[0];
			close(fd[1]);
		}
	}	
	if(!status->bg){
		while(k >= 1){
			p = wait(NULL);
			for(i = 0; i < n; i++)
				if(p == pidmas[i]){
					k--;
					break;
				}
		}
	}
}
void runpipe(char *** const supermas, struct flag *status)
{
	char **cmdmas;
	int n = status->pipescount + 1, k = n, i = 0;
	int fd[2], pidmas[k+1], p, fdin;
	int save0, save1, dr;
	opnfiles(status, &save0, &save1);
	provchan(supermas, status);
	if(status->fin){
		dup2(save0, 0);
		close(save0);
	}
	if(status->fout || status->fappend){
		dup2(save1, 1);
		close(save1);
	}
}
void  run(struct list *p, struct flag *status)
{	
	if(!status->pipes){
		char **cmdmas;	
		cmdmas = makemas(p);
		if(cmdmas[0] == NULL)
			return;
		if (!cmpstr(cmdmas[0], "cd"))
			runcmd(cmdmas, status);
		else	
			runcd(cmdmas);
		free(cmdmas);
	}
	else{
		char ***supermas;
		supermas = makesupermas(p, status);
		runpipe(supermas, status);
	}
}
void findbg(struct list *p, struct flag *status)
{
	status->bg = 0;
	if(status->err || p == NULL)
		return;
	if(p->type == background){
		status->bg = error;
		status->err = 1;
		return;
	}	
	while(p != NULL){
		if(status->bg == 1){
			status->bg = error;
			status->err = 1;
			return;
		}
		if(p->type == background)
			status->bg = 1;
		p = p->next;
	}
}
/* check command list for files */
void findredir(struct list *p, struct flag *st)
{
	int errstatus = 0;
	st->file = 0;
	if(st->err)
		return;
	while(p != NULL){
		if(p->type == append || p->type == output || p->type == input){
			if(p->next == NULL){
				errstatus = 1;
				break;
			}
			if(st->fappend || st->fout || p->next->type != text){
				errstatus = 1;
				break;
			}
			if(st->fin || p->next->type != text){
				errstatus = 1;
				break;
			}	
		}
		if(p->type == append){
			st->append = p->next->cmd;
			p->next->type = appendfile;
			st->fappend = 1;
			st->file = 1;
		}
		if(p->type == output){
			st->output = p->next->cmd;
			p->next->type = outfile;
			st->fout = 1;
			st->file = 1;
		}
		if(p->type == input){
			st->input = p->next->cmd;
			p->next->type = infile;
			st->fin = 1;
			st->file = 1;
		}
		p = p->next;
	}
	if(errstatus){
		st->err = 1;
		st->file = error;
	}
}
void findpipes(struct list *p, struct flag *status)
{
	int cd = 0;
	status->pipes = 0;
	status->pipescount = 0;
	if(status->err || p == NULL)
		return;
	if(p->type == pipeline){
		status->pipes = error;
		status->err = 1;
		return;
	}	
	while(p != NULL){
		if(!cd && cmpstr(p->cmd, "cd"))
			cd = 1;
		if(p->type == pipeline){
			status->pipes = 1;
			if(p->next != NULL && p->next->type != pipeline && !cd)
				status->pipescount++;
			else{
				status->pipes = error;
				status->err = 1;
				return;			
			}
		}
		p = p->next;
	}	
}
void cleanflag(struct flag *status){
	status->bg = 0;
	status->err = 0;
	status->eol = 0;
	status->eof = 0;
	status->quotes = 0;
	status->fin = 0;
	status->fout = 0;
	status->fappend = 0;
	status->file = 0;
	status->pipes = 0;
	status->pipescount = 0;
	status->output = "";
	status->input = "";
	status->append = "";
}
int main()
{
	struct list *cmdlist = NULL;
	struct flag *status =  malloc(sizeof(struct flag));
	status->eof = 0;
	printf("welcome to shell 5.0!\n");
	for(;;){
		shellbar(status);
		cleanflag(status);
		cmdlist = makelist(status);
		if(status->eof)
			break;
		findbg(cmdlist, status);
		findredir(cmdlist, status);	
		findpipes(cmdlist, status);
		printlist(cmdlist);
		if(!finderr(status))
			run(cmdlist, status);
		freelist(cmdlist);
	}
	return 0;
}
