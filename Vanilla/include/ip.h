/* ip.c */
void ip_lookup(char *ip, char *p_full_hostname, char *p_dns_hostname, int len);
void ip_waitpid();
int ip_whitelisted(char *ip);
int ip_hide(char *ip);
int ip_mute(char *ip);
int ip_ignore(char *ip);
int ip_ignore_doosh(char *ip);
void ip_ignore_doosh_set(char *ip);
void ip_ignore_doosh_clear(char *ip);
int ip_ignore_multi(char *ip);
void ip_ignore_multi_set(char *ip);
void ip_ignore_multi_clear(char *ip);
int ip_ignore_ip(char *us, char *them);
void ip_ignore_ip_update(char *us, char *them, int mask);
void ip_ignore_initial(struct player *me);
void ip_ignore_login(struct player *me, struct player *pl);
