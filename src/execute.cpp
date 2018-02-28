#define _GNU_SOURCE
#include<iostream>
#include<sys/types.h>
#include<errno.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<cstring>
#include<signal.h>
#include<sys/stat.h>
#include<fcntl.h>

#include"execute.h"
#include"job.h"
#include"sig.h"
#include"err.h"
using namespace std;

extern vector<gao::Job> jobs;
extern char **environ;

namespace gao
{
	void just_do_it(char* cmdline)
	{
		Job new_job(cmdline);
		new_job.parse_cmdline();

		signal_mask(SIG_BLOCK, SIGCHLD);

		pid_t pid;
		if(!(pid = excute(job)))
		{
			signal_mask(SIG_UNBLOCK, SIGCHLD);
			return;
		}

		new_job.pid = pid;
		jobs.push_back(new_job);

		if(new_job.BACKGROUND)
		{
			fprintf(stderr, "[%d] (%ld) %s\n", pid2jid(pid), (long)pid, cmdline);
			signal_mask(SIG_UNBLOCK, SIGCHLD);
		}
		else
			wait_fg(pid);
	}

	pid_t excute(Job& job)
	{
		if(job.ncmd == 1)
		{
			if(!build_in(&(job.cmd[0])))
				return spawn_proc(STDIN_FILENO, STDOUT_FILENO, cmd, 0);
			else
				return 0;
		}
		else
			return fork_pipes(job);
	}

	bool build_in(Command* cmd)
	{
		if(!strcmp(cmd->argv[0], "quit"))
		{
			raise(SIGQUIT);
			return true;
		}

		if(!strncmp(cmd->argv[0], "jobs", 4))
		{
			int in = dup(STD_IN_FILENO);
			int out = dup(STDOUT_FILENO);

			redirect(cmd);
			list_jobs();

			dup2(in, STDIN_FILENO);
			close(in);
			dup2(out, STDOUT_FILENO);
			close(out);
			return true;
		}

		if(!strcmp(cmd->argv[0], "&"))
			return true;

		if(!strcmp(cmd->argv[0], "fg") || !strcmp(cmd->argv[0], "bg"))
		{
			do_bgfg(cmd);
			return true;
		}

		return false;
	}

	void shift_n(char** buf, int step)
	{
		while(*(buf + step))
		{
			*buf = *(buf + step);
			buf++;
		}
		*buf = (char*)NULL;
	}

	int redirect(Command* cmd)
	{
		char** buf = cmd->argv;
		char* file = NULL;
		int i, flags = 0, write = 0, fd;
		mode_t perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

		char* ptr;
		for(; *buf; buf++)
		{
			/* >> */
			if((ptr = strstr(*buf, ">>")))
			{
				/* cmd >> file*/
				if(!strcmp(*buf, ">>"))
				{
					file = strdupa(*(buf + 1));
					shift_n(buf, 2);
				}
				/* cmd>> file*/
				else if(!*(ptr + 2))
				{
					*ptr = '\0';
					file = strdupa(*(buf + 1));
					shift_n(buf + 1, 1);
				}
				else
				{
					file = strdupa(*(ptr + 2));
					*(ptr + 2) = '\0';
					/* cmd >>file */
					if(!strncmp(*buf, ">>"))
						shift_n(buf, 1);
					/* cmd>>file */
					else
						*ptr = '\0';
				}
				write = 1;
				flags = O_WRONLY | O_APPEND | O_CREAT;
				break;
			}
			else if((ptr = strchr(*buf, '>')))
			{
				/* cmd > file */
				if(!strcmp(*buf, ">"))
				{
					file = strdupa(*(buf + 1));
					shift_n(buf, 2);
				}
				/* cmd> file */
				else if(!*(ptr + 1))
				{
					*ptr = '\0';
					file = strdupa(*(buf + 1));
					shift_n(buf + 1, 1);
				}
				else
				{
					file = strdupa(ptr + 1);
					*(ptr + 1) = '\0';
					/* cmd >file */
					if(!strcmp(*buf, ">"))
						shift_n(buf, 1);
					/* cmd>file */
					else
						*ptr = '\0';
				}
				write = 1;
				flags = O_WRONLY | O_TRUNC | O_CREAT;
				break;
			}
			else if((ptr = strchr(*buf, '<')))
			{
				if(!strcmp(*buf, "<"))
				{
					file = strdupa(*(buf + 1));
					shift_n(buf, 2);
				}
				else if(!*(ptr + 1))
				{
					*ptr = '\0';
					file = strdupa(*(buf + 1));
					shift_n(buf + 1, 1);
				}
				else
				{
					file = strdupa(ptr + 1);
					*(ptr + 1) = '\0';
					if(!strcmp(*buf, "<"))
						shift_n(buf, 1);
					else
						*ptr = '\0';
				}
				flags = O_RDONLY;
			}
		}

		if(file)
		{
			if((fd = open(file, flags, perm)) == -1)
				err_msg("open");

			if(write)
			{
				if(fd != STDOUT_FILENO)
				{
					dup2(fd, STDOUT_FILENO);
					close(fd);
				}
			}
			else if(fd != STDIN_FILENO)
			{
				dup2(fd, STDIN_FILENO);
				close(fd);
			}
			return 1;
		}
		return 0;
	}

	pid_t fork_pipes(Job& job)
	{
		int i;
		pid_t pgid;
		int in, fd[2];

		in = STDIN_FILENO;
		for(i = 0; i < job.ncmd; i++)
		{
			pipe(fd);
			if(i == 0)
				pgid = spawn_proc(in, fd[1], &(job.cmd[i]), 0);
			else
				spawn_proc(in, fd[1], &(job.cmd[i]), pgid);

			close(fd[1]);

			in = fd[0];
		}

		spawn_proc(in, STDOUT_FILENO, job.cmd[i], pgid);
		return pgid;
	}

	pid_t spawn_proc(int in, int out, Command* cmd, pid_t pgid)
	{
		pid_t pid;
		switch((pid = fork()))
		{
			case -1:
			{
				err_msg("fork");
				return 0;
			}
			
			case 0:
			{
				if(setpgid(0, pgid) == -1)
					err_sys("setpgid");
				if(in != STDIN_FILENO)
				{
					dup2(in, STDIN_FILENO);
					close(in);
				}
				if(out != STDOUT_FILENO)
				{
					dup2(out, STDOUT_FILENO);
					close(out);
				}
				redirect(cmd);

				signal_mask(SIG_UNBLOCK, SIGCHLD);
				if(execvpe(cmd->argv[0], cmd->argv, environ) < 0)
				{
					fprintf(stderr, "%s: Command not found.\n", cmd->argv[0]);
					exit(1);
				}
			}

			default:
			{
				if(setpgid(pid, pgid) == -1 && errno != EACCES)
					err_msg("setpgid");
				if(in != STDIN_FILENO)
					close(in);
				return pid;
			}
		}
	}
	void do_bgfg(Job&);
	void wait_fg(pid_t pgid);
}