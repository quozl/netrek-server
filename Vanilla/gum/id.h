void print_user __P ((uid_t uid));
char* uid_to_string(uid_t uid);
void print_group __P ((gid_t gid));
char* gid_to_string(uid_t gid);
int getugroups (int, gid_t *, char *);
int get_all_groups(char*, gid_t **);
int search_4group(gid_t, int, gid_t *);
void *xmalloc (size_t n);
