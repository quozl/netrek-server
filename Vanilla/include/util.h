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
char *my_metaserver_type();
int is_guest(char *name);
struct player *p_no(int i);
void p_x_y_box(struct player *pl, int p_x_min, int p_y_min, int p_x_max, int p_y_max);
void p_x_y_unbox(struct player *pl);
void p_x_y_set(struct player *pl, int p_x, int p_y);
void p_x_y_commit(struct player *j);
void p_x_y_to_internal(struct player *j);
void p_x_y_join(struct player *j, struct player *pl);
void t_x_y_set(struct torp *k, int t_x, int t_y);
