/* util.c */
int angdist(u_char x, u_char y);
#ifdef DEFINE_NINT
int nint(double x);
#endif
int realNumShips(int owner);
#ifdef LTD_STATS
void setEnemy(int myteam, struct player *me);
#endif /* LTD_STATS */
int find_slot_by_host(char *host, int j);
int mprintf(char *format, ...);
char *team_name(int team);
char *team_verb(int team);
char *team_code(int team);
int team_find(char *name);
int team_opposing(int team);
struct player *my();
int is_robot(const struct player *pl);
int is_local(const struct player *p);
int is_only_one_ship_type_allowed(int *type);
