/***** Command Interface Defines *****/
/*     The lowest byte is used for message flags. This is used mostly by the */
/*     voting code. */

#define C_PLAYER	0x0001
#define C_DESC		0x0008
#define C_GLOG          0x0010   /* Write request to God Log */

/* Prerequisites for command to succeed:  */
/* This is determined when the command is run.  It is extremely usefull
   when listing commands through help.  Commands not currently available
   will not be listed. */
#define C_PR_MASK       0xff00   /* Prereq mask */

#define C_PR_NOONE      0x0100   /* Command deactivated */
#define C_PR_GOD        0xff00   /* Only god can use the command */
#define C_PR_INPICKUP   0x0400

#define C_PR_1          0x8000   /* Future use */
#define C_PR_2          0x1000
#define C_PR_3          0x2000
#define C_PR_4          0x4000
#define C_PR_5          0x8000

#define C_VC_ALL        0x0020   /* Command only passes with Majority concensus */
#define C_VC_TEAM       0x0040   /* Command passes with agreement from Team */
#define C_PR_VOTE       0x0080   /* Command is a vote */
/*#define C_VC_PLAYER     0x0080 */  /* Each player can be voted on, like eject */

/* Integrate Voting commands and generic commands into the same
   structure.  The two only differ slightly */
struct command_handler_2 {
    char *command;
    int tag;
    char *desc;
    void (*handler)();

    /*  Data specific to voting commands. */
    int minpass;       /* Minimum required votes needed to pass vote */
    int start;         /* Offset into player voting array */
    int frequency;     /* How often the command can be used (0 = anytime) */
    int expiretime;    /* time before before votes expire */
};

extern const char myname[];
/*extern struct command_handler_2 nts_commands[];*/

#define _PROTO(r) r

extern char * addr_mess _PROTO((int who, int type));

extern int    check_2_command _PROTO((struct message *mess,
				      struct command_handler_2 *cmds,
				      int prereqs));

extern int    do_help _PROTO((char *comm,
			      struct message *mess,
			      struct command_handler_2 *cmds,
			      int cmdnum,
			      int prereqs));
