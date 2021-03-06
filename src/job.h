#ifndef _JOB_H
#define _JOB_H

#include<sys/types.h>
#include<stdlib.h>
#include<cstring>

#define MAXCMDLINELENGTH 1024
#define MAXCMDNUM 20
#define MAXARGS 128
#define MAXJOB 16
#define MAXJID (1<<16)

namespace gao
{
	enum class JobState {UNDEFINED = 0, FRONTGROUND, BACKGROUND, STOPPED};

	class Command
	{
	public:
		char* argv[MAXARGS];
		Command()
		{
			for(int i = 0; i < MAXARGS; i++)
				argv[i] = NULL;
		}
	};

	class Job
	{
	public:
		bool BACKGROUND;

		int jid;
		pid_t pid;
		JobState state;
		int ncmd;

		char cmdline[MAXCMDLINELENGTH];
		Command cmd[MAXCMDNUM];

		static int next_jid;
		Job() : BACKGROUND(false), pid(0), state(JobState::UNDEFINED), ncmd(0)
		{
			jid = next_jid++;

			memset(cmdline, '\0', MAXCMDLINELENGTH);
			for(int i = 0; i < MAXCMDNUM; i++)
				cmd[i] = Command();
		}

		Job(char* cl) : BACKGROUND(false), pid(0), state(JobState::UNDEFINED), ncmd(0)
		{
			jid = next_jid++;

			strcpy(this->cmdline, cl);
			for(int i = 0; i < MAXCMDNUM; i++)
				cmd[i] = Command();
		}

		void parse_cmdline();
	};

	inline void pass_whitespace(char*& cmd)
	{
		while(*cmd != '\0' && *cmd == ' ')
			cmd++;
	}

	int get_front_job_index();

	int pid2jid(pid_t);

	int pid2index(pid_t);

	int jid2index(int);

	void list_jobs();
}
#endif