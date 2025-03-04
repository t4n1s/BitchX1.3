/*
 * ircII: a new irc client.  I like it.  I hope you will too!
 *
 * Written By Michael Sandrof
 * Copyright(c) 1990 
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#define __irc_c

#include "irc.h"
#include "struct.h"

static char cvsrevision[] = "$Id: irc.c 206 2012-06-13 12:34:32Z keaston $";
CVS_REVISION(irc_c)

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#ifdef USING_CURSES
#include <curses.h>
#endif
#include <stdarg.h>

#include "status.h"
#include "dcc.h"
#include "names.h"
#include "vars.h"
#include "input.h"
#include "alias.h"
#include "output.h"
#include "ircterm.h"
#include "exec.h"
#include "flood.h"
#include "screen.h"
#include "log.h"
#include "server.h"
#include "hook.h"
#include "keys.h"
#include "ircaux.h"
#include "commands.h"
#include "window.h"
#include "history.h"
#include "exec.h"
#include "notify.h"
#include "numbers.h"
#include "mail.h"
#include "debug.h"
#include "newio.h"
#include "timer.h"
#include "whowas.h"
#include "misc.h"
#include "gui.h"
#include "cdns.h"
#include "tcl_bx.h"
#include "ssl.h"
#include <pwd.h>
#define MAIN_SOURCE
#include "modval.h"

#ifdef __EMX__
#include <signame.h>
#endif


#ifdef HAVE_SSL
#define OPENSSL_API_COMPAT 0x10100000L
#endif

#ifndef VERSION
	const char irc_version[] = "BitchX-1.3-git";
#else
	const char irc_version[] = VERSION;
#endif

/* Format of bitchx_numver: MMmmpp
 * MM = major version (eg 10 = 1.0)
 * mm = minor version
 * pp = patchlevel (00 = development, 01 = release)
 */
const unsigned long bitchx_numver = 120200;

/*
 * INTERNAL_VERSION is the number that the special alias $V returns.
 * Make sure you are prepared for floods, pestilence, hordes of locusts( 
 * and all sorts of HELL to break loose if you change this number.
 * Its format is actually YYYYMMDD, for the _release_ date of the
 * client..
 */
const char internal_version[] = "20210425";

int	irc_port = IRC_PORT,			/* port of ircd */
	strip_ansi_in_echo,
	current_on_hook = -1,			/* used in the send_text()
						 * routine */
	use_flow_control = USE_FLOW_CONTROL,	/* true: ^Q/^S used for flow
						 * cntl */
	current_numeric,			/* this is negative of the
						 * current numeric! */
	dumb_mode = 0,				/* if true, IRCII is put in
						 * "dumb" mode */
	bflag = 1,
	
	use_input = 1,				/* if 0, stdin is never
						 * checked */
	waiting_out = 0,				/* used by /WAIT command */
	waiting_in = 0,				/* used by /WAIT command */
	who_mask = 0,				/* keeps track of which /who
						 * switchs are set */
	dead	   = 0, 
	inhibit_logging = 0,
#ifndef ONLY_STD_CHARS
	startup_ansi = 1,			/* display startup ansi */
#else
	startup_ansi = 0,			/* DO NOT display startup ansi */
#endif
	auto_connect = 1,			/* auto-connect to first server*/

	background = 0,
	do_check_pid = 0,
	do_ignore_ajoin = 0,
#ifdef HAVE_SSL
	do_use_ssl = 0,
#endif
	run_level = 0,

	foreground = 1,
	reconnect = 0,				/* reconnecting to old process */
	use_nat_address = 0,			/* use NAT address */
	term_initialized = 0;
	
char	zero[]	= "0",
	one[]	= "1",
	space[]	= " ",
	space_plus[] = " +",
	space_minus[] = " -",
	dot[]   = ".",
	star[]	= "*",
	comma[] = ",",
	empty_string[] = "",
	on[]	= "ON",
	off[]	= "OFF";
		 
const	char	*unknown_userhost = "<UNKNOWN>@<UNKNOWN>";

char	oper_command = 0;	/* true just after an oper() command is
				 * given.  Used to tell the difference
				 * between an incorrect password generated by
				 * an oper() command and one generated when
				 * connecting to a new server */
	struct	sockaddr_foobar	MyHostAddr;		/* The local machine address */
	struct	sockaddr_foobar LocalHostAddr;
char	*LocalHostName = NULL;

char	*channel = NULL;


int		inbound_line_mangler = 0,
		logfile_line_mangler = 0,
		operlog_line_mangler = 0,
		outbound_line_mangler = 0;



char	*invite_channel = NULL,		/* last channel of an INVITE */
	*ircrc_file = NULL,		/* full path .ircrc file */
	*bircrc_file = NULL,		/* full path .ircrc file */
	*my_path = NULL,		/* path to users home dir */
	*irc_path = NULL,		/* paths used by /load */
	*irc_lib = NULL,		/* path to the ircII library */
	*ircservers_file = NULL,	/* name  of server file */
	nickname[NICKNAME_LEN + 1],	/* users nickname */
	hostname[NAME_LEN + 1],		/* name of current host */
	userhost[(NAME_LEN + 1) * 2],
	realname[REALNAME_LEN + 1],	/* real name of user */
	username[NAME_LEN + 1],		/* usernameof user */
	attach_ttyname[500],		/* ttyname for this term */
	socket_path[500],		/* ttyname for this term */
	*forwardnick = NULL,		/* used for /forward */
	*send_umode = NULL,		/* sent umode */
	*args_str = NULL,		/* list of command line args */
	*last_notify_nick = NULL,	/* last detected nickname */
	*auto_str = NULL,		/* auto response str */
	*new_script = NULL,		/* rephacement for .bitchxrc and .ircrc */
	*cut_buffer = NULL;		/* global cut_buffer */

	int quick_startup = 0;		/* set if we ignore .ircrc */
	int cpu_saver = 0;

	int use_socks = 0;		/* do we use socks info to connect */
	char	*old_tty = NULL,	/* re-attach tty and password */
		*old_pass = NULL;
	
extern char *FromUserHost;

		
	int cx_line = 0;
	char cx_file[BIG_BUFFER_SIZE/4];		/* debug file info */
	char cx_function[BIG_BUFFER_SIZE/4];
extern	int	doing_privmsg, doing_notice;




time_t	idle_time = 0,
	now = 0,
	start_time;
fd_set	readables, writables;
struct in_addr nat_address;


static	void	quit_response (char *, char *);
static	void	versionreply (void);
static	char	*parse_args (char **, int, char **);
static	void	remove_pid(void);
	void	do_ansi_logo(int);
	
extern void set_process_bits(fd_set *rd);

static	volatile int	cntl_c_hit = 0;

	char	version[] = _VERSION_;
	
static		char	*switch_help[] = {
"Usage: BitchX [switches] [nickname] [server list] \n",
"  The [nickname] can be at most 15 characters long\n",
"  The [server list] is a whitespace separate list of server name\n",
"  The [switches] may be any or all of the following\n",
#ifndef WINNT
"   -H <hostname>\tuses the virtual hostname if possible\n",
#endif
"   -N do not auto-connect to the first server\n",
"   -A do not display the startup ansi\n",
"   -c <channel>\tjoins <channel> on startup. don\'t forget to escape the # using \\\n",
#if defined(WINNT) || defined(__EMX__)
"   -b\t\tload bx-rc or irc-rc after connecting to a server\n",
#else
"   -b\t\tload .bitchxrc  or .ircrc after connecting to a server\n",
#endif
"   -p <port>\tdefault server connection port (usually 6667)\n",
#ifndef WINNT
"   -f\t\tyour terminal uses flow controls (^S/^Q), so BitchX shouldn't\n",
"   -F\t\tyour terminal doesn't use flow control (default)\n",
#endif
	"   -d\t\truns BitchX in \"dumb\" terminal mode\n",
#if defined(WINNT) || defined(__EMX__)
"   -q\t\tdoes not load ~/irc-rc\n",
#else
"   -q\t\tdoes not load ~/.ircrc\n",
#endif
"   -r file\tload file as list of servers\n",
"   -n nickname\tnickname to use\n",
"   -a\t\tadds default servers and command line servers to server list\n",
"   -x\t\truns BitchX in \"debug\" mode\n",
"   -Z\t\tuse NAT address when doing dcc.\n",
"   -P\t\ttoggle check pid.nickname for running program.\n",
"   -v\t\ttells you about the client's version\n",
#ifdef HAVE_SSL
"   -s\t\tservers specified are SSL.\n",
#endif
"   -i\t\tignores the autojoin list entries.\n",
#if defined(WINNT) || defined(__EMX__)
"   -l <file>\tloads <file> in place of your irc-rc\n\
   -L <file>\tloads <file> in place of your irc-rc and expands $ expandos\n",
#else
"   -l <file>\tloads <file> in place of your .ircrc\n\
   -L <file>\tloads <file> in place of your .ircrc and expands $ expandos\n",
#endif
#if !defined(WINNT) && !defined(__EMX__)
"   -B\t\tforce BitchX to fork and return you to shell. pid check on.\n",
#endif
NULL };

char	*time_format = NULL;	/* XXX Bogus XXX */
#ifdef __EMX__
char	*strftime_24hour = "%H:%M";
#else
char	*strftime_24hour = "%R";
#endif
char	*strftime_12hour = "%I:%M%p";
char	time_str[61];

char    saved_termvar[BUFSIZ];

void detachcmd(char *, char *, char *, char *);


int
munge_term_env_var(void)
{
  char *termvar = getenv("TERM");
  int i;

  /*
   * Initialize saved_termvar
   */

  for (i = 0; i < BUFSIZ; i++)
      saved_termvar[i] = '\0';

  /*
   * TERM var not there?  It'll get detected later.  Exit silently.
   */

  if (!termvar)
      return 0;

  if (!strcmp(termvar, "xterm-256color"))
    {
      snprintf(saved_termvar, BUFSIZ, "TERM=%s", termvar);
      return 1;
    }
  else if (!strcmp(termvar, "xterm"))
    {
      snprintf(saved_termvar, BUFSIZ, "TERM=%s", termvar);
      return 1;
    }

  /*
   * Add more if's here, if needed.
   */
}

void
restore_term_env_var(void)
{
  if (strlen(saved_termvar) > 0)
    {
      putenv(saved_termvar);
    }
}


/* update_clock: figUres out the current time and returns it in a nice format */
char	*BX_update_clock(int flag)
{
	static	int		min = -1,
				hour = -1;
	static	time_t		last_minute = -1;
	time_t			idlet;
	static struct tm	time_val;
		time_t		hideous;
		int		new_minute = 0;
		int		new_hour = 0;

	hideous = now;

#if !defined(NO_CHEATING)
	if (hideous / 60 > last_minute)
	{
		last_minute = hideous / 60;
		time_val = *localtime(&hideous);
	}
#else
	time_val = *localtime(&hideous);
#endif


	if (flag == RESET_TIME || time_val.tm_min != min || time_val.tm_hour != hour)
	{
		int ofs = from_server;
		from_server = primary_server;
		
		if (time_format)	/* XXXX Bogus XXXX */
			strftime(time_str, 60, time_format, &time_val);
		else if (get_int_var(CLOCK_24HOUR_VAR))
			strftime(time_str, 60, strftime_24hour, &time_val);
		else
			strftime(time_str, 60, strftime_12hour, &time_val);

		lower(time_str);
		if ((time_val.tm_min != min) || (time_val.tm_hour != hour))
		{
			int is_away = 0;
			if (time_val.tm_hour != hour)
				new_hour = 1;
			new_minute = 1;
			hour = time_val.tm_hour;
			min = time_val.tm_min;
			do_hook(TIMER_LIST, "%02d:%02d", hour, min);
			if (min == 0 || new_hour)
				do_hook(TIMER_HOUR_LIST, "%02d:%02d", hour, min);
			idlet = hideous - idle_time;
			if (from_server != -1)
				is_away = get_server_away(from_server) ? 1 : 0;
			if (do_hook(IDLE_LIST, "%lu", (unsigned long)idlet / 60))
			{
				if (is_away && new_hour && get_int_var(TIMESTAMP_AWAYLOG_HOURLY_VAR))
					logmsg(LOG_CRAP, NULL, 4, NULL);
				check_auto_away(idlet);
			}
			check_channel_limits();
		}
		if (!((hideous - start_time) % 20))
			check_serverlag();
		from_server = ofs;
		if (flag != RESET_TIME || new_minute)
			return time_str;
		else
			return NULL;
	}
	if (flag == GET_TIME)
		return time_str;
	else
		return NULL;
}

void reset_clock(Window *win, char *unused, int unused1)
{
	update_clock(RESET_TIME);
	update_all_status(win, NULL, 0);
}



/* sig_refresh_screen: the signal-callable version of refresh_screen */
SIGNAL_HANDLER(sig_refresh_screen)
{
	refresh_screen(0, NULL);
}

/* irc_exit: cleans up and leaves */
SIGNAL_HANDLER(irc_exit_old)
{
	irc_exit(1,NULL, NULL);
}

volatile int dead_children_processes;

/* This is needed so that the fork()s we do to read compressed files dont
 * sit out there as zombies and chew up our fd's while we read more.
 */
SIGNAL_HANDLER(child_reap)
{
	dead_children_processes++;
#ifdef __EMX__
	my_signal(SIGCHLD, SIG_IGN, 0);
#endif
}

SIGNAL_HANDLER(nothing)
{
      /* nothing to do! */
}
                           
SIGNAL_HANDLER(sigpipe)
{
static int sigpipe_hit = 0;
	sigpipe_hit++;
	
}
          
#if 0
SIGNAL_HANDLER(sigusr2)
{
	mtrace();
}
SIGNAL_HANDLER(sigusr3)
{
	muntrace();
}
#endif

/* irc_exit: cleans up and leaves */
void BX_irc_exit (int really_quit, char *reason, char *format, ...)
{
	char buffer[BIG_BUFFER_SIZE];
		
	logger(current_window, NULL, 0);
	if (get_int_var(MSGLOG_VAR))
		log_toggle(0, NULL);
	if (format)
	{
		va_list arglist;
		va_start(arglist, format);
		vsprintf(buffer, format, arglist);
		va_end(arglist);
	}
	else
		sprintf(buffer, "%s -- just do it.",irc_version);

	if (really_quit)
	{
		put_it("%s", convert_output_format("$G Signon time  :    $0-", "%s", my_ctime(start_time)));
		put_it("%s", convert_output_format("$G Signoff time :    $0-", "%s", my_ctime(now)));
		put_it("%s", convert_output_format("$G Total uptime :   $0-", "%s", convert_time(now - start_time)));
	}
	do_hook(EXIT_LIST, "%s", reason ? reason : buffer);

	close_all_servers(reason ? reason : buffer);

	put_it("%s", buffer ? buffer : reason ? reason : empty_string);


	clean_up_processes();
	if (!dumb_mode && term_initialized)
	{
		cursor_to_input();	/* Needed so that ircII doesn't gobble
					 * the last line of the kill. */
		term_cr();
		term_clear_to_eol();
		term_reset();
	}

	destroy_call_stack();

#if defined(THREAD) && defined(WANT_NSLOOKUP)
	kill_dns();
#endif

	remove_pid();
	if (really_quit)
	{
		
#ifdef GUI
		gui_exit();
#else
#if defined(WANT_DETACH) && !defined(GUI)
		kill_attached_if_needed(0);
#endif
		fprintf(stdout, "\r");
		fflush(stdout);
		exit(0);
#endif
	}
}

#ifndef WANT_CORE
volatile int segv_recurse = 0;
/* sigsegv: something to handle segfaults in a nice way */
/* this needs to be changed to *NOT* use printf(). */
SIGNAL_HANDLER(coredump)
{
#if defined(WINNT)
extern char *sys_siglist[];
#endif
	if (segv_recurse)
		_exit(1);
	segv_recurse = 1;
	panic_dump_call_stack();
	
#ifdef TDEBUG
	putlog(LOG_ALL, "*", "Error logged. %s at (%d)  %s", cx_file, cx_line, cx_function?cx_function:empty_string);
#endif
	printf("\n\r\n\rIRCII has been terminated by a [%s]\n\r", sys_siglist[unused]);
	printf("Please email " BUG_EMAIL "\n\r");
	printf("with as much detail as possible about what you were doing when it happened.\n\r");
	printf("Please include the version of IRCII (%s) and type of system in the report.\n\r", irc_version);
	fflush(stdout);
	irc_exit(1, "Hmmmm... BitchX error!!!! unusual :)", NULL);
}
#endif

/*
 * quit_response: Used by irc_io when called from irc_quit to see if we got
 * the right response to our question.  If the response was affirmative, the
 * user gets booted from irc.  Otherwise, life goes on. 
 */
static	void quit_response(char *dummy, char *ptr)
{
	int	len;

	if ((len = strlen(ptr)) != 0)
		if (!my_strnicmp(ptr, "yes", len))
			irc_exit(1, NULL, "IRC][ %s:  Rest in peace", irc_version);
}

/* irc_quit: prompts the user if they wish to exit, then does the right thing */
void irc_quit(char key, char * ptr)
{
	static	int in_it = 0;

	if (in_it)
		return;
	in_it = 1;
	add_wait_prompt("Do you really want to quit? ", quit_response, empty_string, WAIT_PROMPT_LINE, 1);
	in_it = 0;
}

/*
 * cntl_c: emergency exit.... if somehow everything else freezes up, hitting
 * ^C five times should kill the program. 
 */
SIGNAL_HANDLER(cntl_c)
{

	if (cntl_c_hit++ >= 4)
		irc_exit(1, "User abort with 5 Ctrl-C's", NULL);
	else if (cntl_c_hit > 1)
		kill(getpid(), SIGALRM);
}

SIGNAL_HANDLER(sig_user1)
{
	bitchsay("Got SIGUSR1, closing DCC connections and EXECed processes");
	close_all_dcc();
	clean_up_processes();
}


SIGNAL_HANDLER(sig_detach)
{
	detachcmd(NULL, NULL, NULL, NULL);
}

void set_detach_on_hup(Window *dummy, char *unused, int value)
{
	if(value)
		my_signal(SIGHUP, sig_detach, 0);
	else
		my_signal(SIGHUP, irc_exit_old, 0);
}

/* shows the version of irc */
static	void versionreply(void)
{
	printf("BitchX version %s (%s)\n\r", irc_version, internal_version);
	exit (0);
}

#ifndef RAND_MAX
#define RAND_MAX 2147483647
#endif

void display_bitchx(int j) 
{

int i = 0;
int old_strip_ansi = strip_ansi_in_echo;

	strip_ansi_in_echo = 0; 

	if (j == -1)
#ifdef ASCII_LOGO
	        i = (int) (5.0*rand()/RAND_MAX);
#else
	        i = (int) (17.0*rand()/RAND_MAX);
#endif   
	else
		i = j;

	if (!startup_ansi)
		return;

#if !defined(WINNT) && !defined(__EMX__)
	charset_ibmpc();
#endif

	do_ansi_logo(i);
#if !defined(WINNT) && !defined(__EMX__)
#if defined(LATIN1)
	charset_lat1();
#elif defined(CHARSET_CUSTOM)
	charset_cst();
#endif

#endif
	strip_ansi_in_echo = old_strip_ansi;
}



/*
 * parse_args: parse command line arguments for irc, and sets all initial
 * flags, etc. 
 *
 * major rewrite 12/22/94 -jfn
 *
 *
 * Im going to break backwards compatability here:  I think that im 
 * safer in doing this becuase there are a lot less shell script with
 * the command line flags then there are ircII scripts with old commands/
 * syntax that would be a nasty thing to break..
 *
 * Sanity check:
 *   Supported flags: -b, -l, -v, -c, -p, -f, -F, -L, -a, -S, -z
 *   New (changed) flags: -s, -I, -i, -n
 *
 * Rules:
 *   Each flag must be included by a hyphen:  -lb <filename> is not the
 * 		same as -l <filename> -b  any more...
 *   Each flag may or may not have a space between the flag and the argument.
 *   		-lfoo  is the same as -l foo
 *   Anything surrounded by quotation marks is honored as one word.
 *   The -c, -p, -L, -l, -s, -z flags all take arguments.  If no argumenTs
 *		are given between the flag and the next flag, an error
 * 		message is printed and the program is halted.
 *		Exception: the -s flag will be accepted without a argument.
 *		(ick: backwards compatability sucks. ;-)
 *   Arguments occuring after a flag that does not take an argument
 * 		will be parsed in the following way: the first instance
 *		will be an assumed nickname, and the second instance will
 *		will be an assumed server. (some semblance of back compat.)
 *   The -bl sequence will emit a depreciated feature warning.
 *   The -I flag forces you to become invisible <NOT YET SUPPORTED>
 *   The -i flag forces you to become visible <NOT YET SUPPORTED>
 *   The -X flag forces ircII to become an X application <NOT YET SUPPORTED>
 *   The -n flag means "nickname"
 *
 * Bugs:
 *   The -s flag is hard to use without an argument unless youre careful.
 */
#ifdef CLOAKED
extern char proctitlestr[140];
extern char **Argv;
extern char *LastArgv;
#endif


static	char	*parse_args (char *argv[], int argc, char **envp)
{
	int ac;
	int add_servers = 0;
#if !defined(WINNT)
	struct passwd *entry;
#endif
	char *channel = NULL;
	struct hostent * hp;
	char *ptr;
	
	*nickname = 0;

#ifdef CLOAKED
	Argv = argv;
	while (*envp)
		envp++;
	LastArgv = envp[-1] + strlen(envp[-1]);
#endif                    

	for ( ac = 1; ac < argc; ac++ )
	{
		if (argv[ac][0] == '-')
		{
		    switch (argv[ac][1]) {

			case 'A': /* turn off startup ansi */
				startup_ansi =  0;
				break;
			case 'v': /* Output ircII version */
			{
				versionreply();
				/* NOTREACHED */
			}

			case 'c': /* Default channel to join */
			{
				char *what = empty_string;

				if (argv[ac][2])
					what = &(argv[ac][2]);
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr, "Missing parameter after -c\n");
					exit(1);
				}
				malloc_strcpy(&channel, what);
				break;
			}

			case 'p': /* Default port to use */
			{
				char *what = empty_string;
				if (reconnect)
				{
					what = getpass("Enter password : ");
					if (what && *what)
						malloc_strcpy(&old_pass, what);
					break;
				}

				if (argv[ac][2])
					what = &argv[ac][2];
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr, "Missing parameter after -p\n");
					exit(1);
				}
				irc_port = my_atol(what);
				break;
			}
#ifndef WINNT
			case 'f': /* Use flow control */
			{
				use_flow_control = 1;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -f\n");
				break;
			}

			case 'F': /* dont use flow control */
			{
				use_flow_control = 0;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -F\n");
				break;
			}
#endif
			case 'd': /* use dumb mode */
			{
				dumb_mode = 1;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -d\n");
				break;
			}

			case 'l': /* Load some file instead of ~/.ircrc */
			{
				char *what = empty_string;

				if (argv[ac][2])
					what = &argv[ac][2];
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr, "Missing argument to -l\n");
					exit(1);
				}
				malloc_strcpy(&new_script, what);
				break;
			}

			case 'L': /* load and expand */
			{
				char *what = empty_string;

				if (argv[ac][2])
					what = &argv[ac][2];
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr, "Missing argument to -L\n");
					exit(1);
				}
				malloc_strcpy(&ircrc_file, what);
/*				malloc_strcat(&ircrc_file, " -");*/
				break;
			}

			case 'r': /* Load list of servers from this file */
			{
				char *what = empty_string;

				if (argv[ac][2])
					what = &argv[ac][2];
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
					fprintf(stderr, "Missing argumenT to -r\n");

				if (*what)
				{
					add_servers = 1;
					malloc_strcpy(&ircservers_file, what);
				}
				break;
			}
			case 'R':
				fprintf(stderr, "Use \"scr-bx\" to reconnect\r\n");
				exit(1);
				break;
			case 'a': /* add server, not replace */
			{
				add_servers = 1;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -a\n");
				break;
			}

			case 'q': /* quick startup -- no .ircrc */
			{
				quick_startup = 1;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -q\n");
				break;
			}

			case 'b':
			{
				bflag = 0;
				break;
			}

			case 'B':
			{
				if (argv[ac][2] && argv[ac][2] != 'l')
					fprintf(stderr, "Ignoring junk after -B\n");
				else if (argv[ac][2] == 'l')
				{
					fprintf(stderr, "Usage of -bl is decprecated: use -b -l instead.\n");
					exit(1);
				}
/*				dumb_mode = 1;*/
/*				use_input = 0;*/
				background = 1;
			}
			case 'P':
			{
				do_check_pid ^= 1;
				break;
			}
			case 'n':
			{
				char *what = empty_string;

				if (argv[ac][2])
					what = &(argv[ac][2]);
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr,"Missing argument for -n\n");
					exit(1);
				}
				strmcpy(nickname, what, NICKNAME_LEN);
				break;
			}
			case 'N':
			{
				auto_connect = 0;
				break;
			}
			case 'x': /* set server debug */
			{
				x_debug = (unsigned long)0xffffffff;
				if (argv[ac][2])
					fprintf(stderr, "Ignoring junk after -x\n");
				break;
			}
			case 'Z':
			{
				use_nat_address = 1;
				break;
			}

			case 'z':
			{
				char *what;
				if (argv[ac][2])
					what = &argv[ac][2];
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
					break;
				strmcpy(username, what, NAME_LEN);
				break;
			}
			case 'H':
			{
				char *what = empty_string;
				
				if (argv[ac][2])
					what = &(argv[ac][2]);
				else if (argv[ac+1] && argv[ac+1][0] != '-')
				{
					what = argv[ac+1];
					ac++;
				}
				else
				{
					fprintf(stderr, "You forgot to specify a hostname\n");
					
					exit(1);
				}
				malloc_strcpy(&LocalHostName, what);
				break;
			}
			case 'S':
				use_socks = 1;
				break;
			case 'i':
				do_ignore_ajoin = 1;
				break;
#ifdef HAVE_SSL
			case 's':
				do_use_ssl = 1;
				break;
#endif
			case '\0': 
				break;	/* ignore - alone */

			default:
				fprintf(stderr, "Unknown flag: %s\n",argv[ac]);
			case 'h':
				{
					int t = 0;

					while(switch_help[t])
					{
						fprintf(stderr, "%s", switch_help[t]);
						t++;
					}
					exit(1);
				}
		   } /* End of switch */
		}
		else
		{
			if (!strchr(argv[ac], '.') && !strchr(argv[ac], ',') && !*nickname)
				strmcpy(nickname, argv[ac], NICKNAME_LEN);
			else
				build_server_list(argv[ac]);
#ifdef HAVE_SSL
			do_use_ssl = 0;
#endif
		}
	}

	if (!*nickname && (ptr = getenv("IRCNICK")))
		strmcpy(nickname, ptr, NICKNAME_LEN);

	if (!ircservers_file)
#if defined(WINNT) || defined(__EMX__)
		malloc_strcpy(&ircservers_file, "irc-serv");
#else
		malloc_strcpy(&ircservers_file, ".ircservers");
#endif
	/* v-- right there was a '!' that should not have been there. */
	if ((ptr = getenv("IRCLIB")))
	{
		malloc_strcpy(&irc_lib, ptr);
		malloc_strcat(&irc_lib, "/");
	}
	else
		malloc_strcpy(&irc_lib, IRCLIB);

	if (!ircrc_file && (ptr = getenv("IRCRC")))
		malloc_strcpy(&ircrc_file, ptr);

	if ((ptr = getenv("IRCUMODE")))
		malloc_strcpy(&send_umode, ptr);

	if ((ptr = getenv("IRCNAME")))
		strmcpy(realname, ptr, REALNAME_LEN);
	else if ((ptr = getenv("NAME")))
		strmcpy(realname, ptr, REALNAME_LEN);

	if ((ptr = getenv("IRCPATH")))
		malloc_strcpy(&irc_path, ptr);
	else
	{
#ifdef IRCPATH
		malloc_strcpy(&irc_path, IRCPATH);
#else
		malloc_strcpy(&irc_path, ".:~/.irc:");
		malloc_strcat(&irc_path, irc_lib);
		malloc_strcat(&irc_path, "script");
#endif
	}

	set_string_var(LOAD_PATH_VAR, irc_path);
	new_free(&irc_path);
	
       /*
        * Yes... this is EXACTLY what you think it is.  And if you don't know..
        * then I'm not about to tell you!           -- Jake [WinterHawk] Khuon
        *
        * Here we determine our username. It may be set above, by the -z command line
        * option. If not check IRCUSER, then USER, then the IDENT_HACK file, then
        * fallback to gecos below.
        */
	if (!*username && (ptr = getenv("IRCUSER"))) strmcpy(username, ptr, NAME_LEN);
	else if (!*username && (ptr = getenv("USER"))) strmcpy(username, ptr, NAME_LEN);
	else if (!*username)
	{
#ifdef IDENT_FAKE
		char *p = NULL, *q = NULL;
		FILE *f;
		malloc_sprintf(&p, "~/%s", get_string_var(IDENT_HACK_VAR));
		q = expand_twiddle(p);
		if ((f = fopen(q, "r")))
		{
			fgets(username, NAME_LEN, f);
			if (*username && strchr(username, '\n'))
				username[strlen(username)-1] = 0;
		}
		fclose(f);
		new_free(&p); new_free(&q);
		if (!*username)
#endif
			strmcpy(username, "Unknown", NAME_LEN); 

	}

#ifndef WINNT
	if ((entry = getpwuid(getuid())))
	{
		if (!*realname && entry->pw_gecos && *(entry->pw_gecos))
		{
#ifdef GECOS_DELIMITER
			if ((ptr = index(entry->pw_gecos, GECOS_DELIMITER)))
				*ptr = (char) 0;
#endif
			if ((ptr = strchr(entry->pw_gecos, '&')) == NULL)
				strmcpy(realname, entry->pw_gecos, REALNAME_LEN);
			else {
				int len = ptr - entry->pw_gecos;

				if (len < REALNAME_LEN && *(entry->pw_name)) {
					char *q = realname + len;

					strmcpy(realname, entry->pw_gecos, len);
					strmcat(realname, entry->pw_name, REALNAME_LEN);
					strmcat(realname, ptr + 1, REALNAME_LEN);
					if (islower((unsigned char)*q) && (q == realname || isspace((unsigned char)*(q - 1))))
						*q = toupper(*q);
				} else
					strmcpy(realname, entry->pw_gecos, REALNAME_LEN);
			}
		}
		if (entry->pw_name && *(entry->pw_name) && !*username)
			strmcpy(username, entry->pw_name, NAME_LEN);
		if (entry->pw_dir && *(entry->pw_dir))
			malloc_strcpy(&my_path, entry->pw_dir);
	}
#else
	{
		u_long size=NAME_LEN+1;
		if (!(ptr = getenv("IRCUSER")))
			strcpy(username, "unknown");
		else
			strncpy(username,ptr, size);
	}
#endif

	if ((ptr = getenv("HOME")))
		malloc_strcpy(&my_path, ptr);
	else if (!my_path || !*my_path)
#ifdef WINNT
		malloc_strcpy(&my_path, empty_string);
#else
		malloc_strcpy(&my_path, "/");
#endif
#if defined(WINNT) || defined(__EMX__)
	convert_unix(my_path);
#endif
	if (!*realname)
		strmcpy(realname, "* I'm too lame to read BitchX.doc *", REALNAME_LEN);

	if (!LocalHostName && ((ptr = getenv("IRC_HOST")) || (ptr = getenv("IRCHOST"))))
		LocalHostName = m_strdup(ptr);

	if ((gethostname(hostname, sizeof(hostname))))
		if (!LocalHostName)
			exit(1);

	if (LocalHostName)
	{
		printf("Your hostname appears to be [%s]\n", LocalHostName);
#ifndef IPV6
		memset((void *)&LocalHostAddr, 0, sizeof(LocalHostAddr));
		if ((hp = gethostbyname(LocalHostName)))
			memcpy((void *)&LocalHostAddr.sf_addr, hp->h_addr, sizeof(struct in_addr));
	} 
	else
	{
		if ((hp = gethostbyname(hostname)))
			memcpy((char *) &MyHostAddr.sf_addr, hp->h_addr, sizeof(struct in_addr));
#endif
	}

	if (!nickname || !*nickname)
		strmcpy(nickname, username, NICKNAME_LEN);

	if (!check_nickname(nickname))
	{
		fprintf(stderr, "Illegal nickname %s\n", nickname);
		fprintf(stderr, "Please restart IRC II with a valid nickname\n");
		exit(1);
	}
	if (ircrc_file == NULL)
	{
		ircrc_file = (char *) new_malloc(strlen(my_path) + strlen(IRCRC_NAME) + 1);
		strcpy(ircrc_file, my_path);
		strcat(ircrc_file, IRCRC_NAME);
	}
	if (bircrc_file == NULL)
#if defined(WINNT) || defined(__EMX__)
		malloc_sprintf(&bircrc_file, "%s/bx-rc", my_path);
#else
		malloc_sprintf(&bircrc_file, "%s/.bitchxrc", my_path);
#endif

	if ((ptr = getenv("IRCPORT")))
		irc_port = my_atol(ptr);

	if ((ptr = getenv("IRCSERVER")))
		build_server_list(ptr);
	
	if (!server_list_size() || add_servers)
	{
		if (!read_server_file(ircservers_file) && !server_list_size())
		{
#ifdef DEFAULT_SERVER
			char *ptr = NULL;
			malloc_strcpy(&ptr, DEFAULT_SERVER);
			build_server_list(ptr);
			new_free(&ptr);

#endif
			from_server = -1;
		}
	}
	return (channel);
}



/* 
 * io() is a ONE TIME THROUGH loop!  It simply does ONE check on the
 * file descriptors, and if there is nothing waiting, it will time
 * out and drop out.  It does everything as far as checking for exec,
 * dcc, ttys, notify, the whole ball o wax, but it does NOT iterate!
 * 
 * You should usually NOT call io() unless you are specifically waiting
 * for something from a file descriptor.  It doesnt look like bad things
 * will happen if you call this elsewhere, but its long time behavior has
 * not been observed.  It *does* however, appear to be much more reliable
 * then the old irc_io, and i even know how this works. >;-)
 */
extern void set_screens (fd_set *, fd_set *);

void BX_io (const char *what)
{
	static	int	first_time = 1,	
			level = 0;
		long	clock_timeout = 0, 
			timer_timeout = 0,
			server_timeout = 0,
			real_timeout = 0; 
static	struct	timeval my_now,
			my_timer,
			*time_ptr = &my_timer;

	int		hold_over,
			rc;
	fd_set		rd, 
			wd;
	static int 	old_level = 0;
static	const	char	*caller[51] = { NULL }; /* XXXX */
	static	int	last_warn = 0;

	level++;

	get_time(&my_now);
	now = my_now.tv_sec;
	
	if (x_debug & DEBUG_WAITS)
	{
		if (level != old_level)
		{
			yell("Moving from io level [%d] to level [%d] from [%s]", old_level, level, what);
			old_level = level;
		}
	}


	if (level && (level - last_warn == 5))
	{
		last_warn = level;
		yell("io's nesting level is [%d],  [%s]<-[%s]<-[%s]<-[%s]<-[%s]<-[%s]", level, what, caller[level-1], caller[level-2], caller[level-3], caller[level-4], caller[level-5]);
		if ((level % 30) == 0)
			ircpanic("Ahoy there matey!  Abandon ship!");
		return;
	}
	else if (level  && (last_warn -level == 5))
		last_warn -= 5;

	caller[level] = what;

	/* CHECK FOR CPU SAVER MODE */
	if (!cpu_saver && get_int_var(CPU_SAVER_AFTER_VAR))
		if (now - idle_time > get_int_var(CPU_SAVER_AFTER_VAR) * 60)
			cpu_saver_on(0, NULL);

	rd = readables;
	wd = writables;
	FD_ZERO(&wd);
	FD_ZERO(&rd);

#ifndef GUI
	set_screens(&rd, &wd);
#endif
	server_timeout = set_server_bits(&rd, &wd);
	set_process_bits(&rd);
	set_socket_read(&rd, &wd);
	set_nslookupfd(&rd);
#ifdef GUI
	gui_setfd(&rd);
#endif

	clock_timeout = (60 - (my_now.tv_sec % 60)) * 1000;
	if (cpu_saver && get_int_var(CPU_SAVER_EVERY_VAR))
		clock_timeout += (get_int_var(CPU_SAVER_EVERY_VAR) - 1) * 60000;

	timer_timeout = TimerTimeout();

	if ((hold_over = unhold_windows()))
		real_timeout = 0;
	else if (timer_timeout <= clock_timeout)
		real_timeout = timer_timeout;
	else
		real_timeout = clock_timeout;

	if(server_timeout && (server_timeout == -1 || server_timeout < real_timeout))
		real_timeout = server_timeout;

	time_ptr = &my_timer;

	if (real_timeout == -1)
		time_ptr = NULL;
	else
	{
		time_ptr->tv_sec = real_timeout / 1000;
		time_ptr->tv_usec = ((real_timeout % 1000) * 1000);
	}
	
	/* GO AHEAD AND WAIT FOR SOME DATA TO COME IN */
	switch ((rc = new_select(&rd, &wd, time_ptr)))
	{
		case 0:
			if(server_timeout)
				do_idle_server();
			break;
		case -1:
		{
			/* if we just got a sigint */
			first_time = 0;
			if (cntl_c_hit)
			{
				edit_char('\003');
/*				cntl_c_hit = 0; */
			}
			else if (errno != EINTR)
				yell("Select failed with [%s]", strerror(errno));
			break;

		}

		/* we got something on one of the descriptors */
		default:
		{
			cntl_c_hit = 0;
			now = time(NULL);
			make_window_current(NULL);
			do_server(&rd, &wd);
			do_processes(&rd);
			do_screens(&rd);
#ifdef WANT_CDCC
			dcc_sendfrom_queue();
#endif
			dcc_check_idle();
			print_nslookup(&rd);
			scan_sockets(&rd, &wd);
#ifdef GUI
			scan_gui(&rd);
#endif
			break;
		} 
	}

	now = time(NULL);
	ExecuteTimers();
#ifdef WANT_TCL
	check_utimers();
#endif	
	send_from_server_queue();
	get_child_exit(-1);

	if (!hold_over)
		cursor_to_input();


#ifdef WANT_LLOOK
	if (get_int_var(LLOOK_VAR) && from_server > -1 && !get_server_linklook(from_server)) 
	{
		if (now - get_server_linklook_time(from_server) > get_int_var(LLOOK_DELAY_VAR)) 
		{
			set_server_linklook(from_server, get_server_linklook(from_server)+1);
			my_send_to_server(from_server, "LINKS");
			set_server_linklook_time(from_server, now);
		}
	}
#endif
	if (update_clock(RESET_TIME))
	{
		do_notify();
#ifdef WANT_TCL
		check_timers();
#endif
		clean_whowas_chan_list();
		clean_whowas_list();
		clean_flood_list();
		clean_split_server_list(CHAN_SPLIT, 1800);
#ifdef WANT_LLOOK
		clean_split_server_list(LLOOK_SPLIT, 3600);
#endif
		if (get_int_var(CLOCK_VAR) || check_mail_status())
		{
			update_all_status(current_window, NULL, 0);
			cursor_to_input();
		}
		check_server_connect(from_server);
	}

	/* (set in term.c) -- we should redraw the screen here */
	if (need_redraw)
		refresh_screen(0, NULL);
	
	FromUserHost = empty_string;
	
	
	alloca(0);
	caller[level] = NULL;
	level--;
	return;
}

char pidfile[80];

void check_pid(void)
{
char *p = NULL;
FILE *t;
	if (!do_check_pid)
		return;
	p = expand_twiddle(DEFAULT_CTOOLZ_DIR);
	snprintf(pidfile, 79, "%s/pid.%s", p, nickname);
	if ((t = fopen(pidfile, "r")))
	{
		char buffer[80];
		int i;
		fgets(buffer, 10, t);
		i = atol(buffer);
		kill(i, SIGCHLD);
		if (errno != ESRCH)
		{
			fprintf(stderr, "Bitchx already running as %s\n", nickname);
			exit(1);
		}
	}
}

extern int save_ipc;

static void remove_pid(void)
{
#ifdef WANT_DETACH
char *p;
FILE *t;
	if (!do_check_pid)
		return;
	p = expand_twiddle(DEFAULT_CTOOLZ_DIR);
	snprintf(pidfile, 79, "%s/pid.%s", p, nickname);
	if ((t = fopen(pidfile, "r")))
	{
#if !defined(__EMX__) && !defined(WINNT) && !defined(GUI)
		SocketList *s;
		fclose(t);
		unlink(pidfile);
		if (save_ipc != -1 && (s = get_socket(save_ipc)))
		{
			char buf[500];
			sprintf(buf, s->server, s->port);
			unlink(buf);
		}
#endif
	}	
#endif
}

void setup_pid(void)
{
#ifdef WANT_DETACH
pid_t pid;
	if (!do_check_pid)
		return;
	if ((pid = getpid()))
	{
		FILE *t;
		unlink(pidfile);
		if ((t = fopen(pidfile, "w")))
		{
			fprintf(t, "%u\n", pid);
			fclose(t);
		}
	}
#endif
}


int main(int argc, char *argv[], char *envp[])
{
	srand((unsigned)time(NULL));
	time(&start_time);
	time(&idle_time);
	time(&now);

	if (munge_term_env_var())
	    putenv("TERM=vt100");

	/* We need to zero these early */
	FD_ZERO(&readables);
	FD_ZERO(&writables);

	/* First thing we need to do is initialize
	 * the global function table with the default
	 * function pointers.
	 */
	init_global_functions();

	/* Setup OS/2 */
#if defined(__EMX__)
	setvbuf(stdout, NULL, _IOLBF, 80);
#endif
	/* Setup the GUIs */
#ifdef GUI
	gui_startup(argc, argv);
#endif
        
#ifdef SOCKS
	SOCKSinit(argv[0]);
#endif

#ifdef TDEBUG
	*cx_file = 0;
	cx_line = 0;
	*cx_function = 0;
#endif

#if !defined(GUI) 

	printf("BitchX - Based on EPIC Software Labs epic ircII (1998).\r\n");
	printf("Version (%s) -- Date (%s).\r\n", irc_version, internal_version);
	printf("Process [%d]", getpid());
	if ((isatty(0) && !background) || (!isatty(0) && background))
	{
		char s[90];
		if (ttyname(0))
		{
			strncpy(s, ttyname(0), 85);
/*			printf(" connected to tty [%s]", s);*/
			strncpy(attach_ttyname, s, 400);
		}		
		else if (background)
			strcpy(attach_ttyname, "tty1");
	}
	else
		dumb_mode = 1;
	printf("\n");
#endif	

	
	channel = parse_args(argv, argc, envp);
	check_pid();
	init_socketpath();

#ifdef WANT_TCL
	tcl_interp = Tcl_CreateInterp();
#endif
	
	if (!dumb_mode && term_init(NULL))
		_exit(1);

	if (background)
	{
#ifdef WANT_DETACH
#ifndef PUBLIC_ACCESS
		detachcmd(NULL, NULL, NULL, NULL);
#else
		exit(0);
#endif
#else
		fprintf(stderr, "Try compiling with WANT_DETACH\r\n");
		exit(0);
#endif
	}
#ifndef WANT_CORE
	my_signal(SIGSEGV, coredump, 0);
	my_signal(SIGBUS, coredump, 0);
#endif
	my_signal(SIGQUIT, SIG_IGN, 0);
#ifdef WANT_DETACH
	set_detach_on_hup(NULL, NULL, DEFAULT_DETACH_ON_HUP);
#else
	my_signal(SIGHUP, irc_exit_old, 0);
#endif
	my_signal(SIGTERM, irc_exit_old, 0);
	my_signal(SIGPIPE, SIG_IGN, 0);
	my_signal(SIGINT, cntl_c, 0);
	my_signal(SIGCHLD, child_reap, 0);
	my_signal(SIGALRM, nothing, 0);
	my_signal(SIGUSR1, sig_user1, 0);
#ifdef GTK
	my_signal(SIGTTOU, SIG_IGN, 0);
#endif
#if 0
	my_signal(SIGUSR1, sigusr2, 0);
	my_signal(SIGUSR2, sigusr3, 0);
#endif	
	init_output();

	if (!dumb_mode) 
	{
		init_screen();
		if (background)
		{
			use_input = 0;
			main_screen->fdin = main_screen->fdout = open("/dev/null", O_RDWR|O_NOCTTY);
			new_open(main_screen->fdin);
			main_screen->fpout = fdopen(1, "w");
			main_screen->fpin = fdopen(0, "r");
		}
#ifndef __EMX__
		my_signal(SIGCONT, term_cont, 0);
#if !defined(WINNT) && !defined(GTK)
		my_signal(SIGWINCH, sig_refresh_screen, 0);
#endif
#endif

	}
	else
	{
		if (background)
		{
			my_signal(SIGHUP, SIG_IGN, 0);
			background = 0;
#ifndef WANT_DETACH
			term_reset();
			fprintf(stderr, "try recompiling the client with WANT_DETACH defined\r\n");
			exit(0);
			use_input = 0;
#endif
		}
		create_new_screen();
		new_window(main_screen);
	}

	setup_pid();
	init_keys();
	init_keys2();
	init_variables();
	init_dcc_table();

#if defined(THREAD) && defined(WANT_NSLOOKUP)
	start_dns();
#endif
	
	if (!dumb_mode)
	{
		build_status(current_window, NULL, 0);
		update_input(UPDATE_ALL);
	}

	start_identd();

#ifdef WANT_TCL
	tcl_init();
#ifdef WANT_TK
	Tk_Init(tcl_interp);
#endif
	add_tcl_vars();
#endif
#ifdef HAVE_SSL
	{
		char *entropy = malloc(100);
		int i;

		for(i=0;i<100;i++)
			entropy[i] = (char) getrandom(0, 255);

		/* Many systems don't have /dev/random so we seed */
		RAND_seed(entropy, 100);
		SSL_library_init();
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		OpenSSL_add_all_algorithms();
		free(entropy);
	}
#endif

#ifdef CLOAKED
	initsetproctitle(argc, argv, envp);
	sprintf(proctitlestr, CLOAKED);
	setproctitle("%s", proctitlestr);
#endif

	/* We move from run level 0 to run level 1
	 * signifying that we have completed the initial
	 * startup and are now ready to load scripts.
	 */
	run_level = 1;

	display_bitchx(-1);
	if (bflag)
		load_scripts();

#ifdef GUI
	gui_font_init();
#endif

	reinit_autoresponse(current_window, NULL, 0);
	if (auto_connect)
		get_connected(0, -1);
	else
		display_server_list();
	start_memdebug();	
		
	set_input(empty_string);
	set_input_prompt(current_window, get_string_var(INPUT_PROMPT_VAR), 0);

	/* We now move from run level 1 to run level 2
	 * signifying that we are in normal operation mode.
	 */
	run_level = 2;

	for (;;)
		io("main");
#ifdef GUI
	gui_exit();
#else
	ircpanic("get_line() returned");
#endif
	restore_term_env_var ();
	return (-((int)0xdead));
}
