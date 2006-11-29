/* ip.c */
void ip_lookup(char *ip, char *p_full_hostname, int len);
void ip_waitpid();
int ip_whitelisted(char *ip);
int ip_hide(char *ip);
int ip_mute(char *ip);
int ip_ignore(char *ip);
