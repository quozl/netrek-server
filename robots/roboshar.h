/* from roboshar.c */
void robonameset(struct player *myself);
void messAll(int mynum, char *name, const char *fmt, ...);
u_char getcourse2(int x1, int y1, int x2, int y2);
void messOne(int mynum, char *name, int who, const char *fmt, ...);
void robohelp(struct player *me, int oldmctl, char *roboname);

/* from ntserv/commands.c */
int check_command(struct message *mess);
