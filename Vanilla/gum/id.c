#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "id.h"

int
search_4group(gid_t group, int count, gid_t *supp_groups)
{
  int i;

   for (i = 0; i < count; i++) {
    if (group == supp_groups[i])
      return 1;
   }
   return 0;
}

int
get_all_groups(char *username, gid_t **groups)
{
  int max_groups;
  gid_t *g;

  if ((max_groups = getugroups(0, NULL, username)) < 0) {
    return -1;
  }

  g = (gid_t *) xmalloc(max_groups * sizeof(gid_t) + 1);

  if ((getugroups(max_groups, g, username)) < 0) {
    return -1;
  }

  *groups = g;
  return max_groups;
}
   
/* Same as getgroups by uses a username instead of uid */
int
getugroups (int maxcount, gid_t *grouplist, char *username)
{
  struct group *grp;
  register char **cp;
  register int count = 0;

  setgrent ();
  while ((grp = getgrent ()) != 0)
    for (cp = grp->gr_mem; *cp; ++cp)
      if (!strcmp (username, *cp)) {
	if (maxcount != 0) {
	  if (count >= maxcount) {
	    endgrent ();
	    return count;
	  }
	  grouplist[count] = grp->gr_gid;
	}
	count++;
      }
  endgrent ();
  return count;
}

/* Send me a uid and I'll print you the username */

void print_user(uid_t uid) 
{
  printf("%s", uid_to_string(uid));
}

char *uid_to_string(uid_t uid)
{
  struct passwd *pwd = NULL;

  pwd = getpwuid(uid);
  if (pwd == NULL) {
     return NULL;
  }
  return pwd->pw_name;
}

/* Send me a guid and I'll print you the group name */

void print_group(gid_t gid)
{
  printf("%s", gid_to_string(gid));
}

char *gid_to_string(gid_t gid)
{
  struct group *grp = NULL;

  grp = getgrgid(gid);
  if (grp == NULL) {
    return NULL;
  }
  
  return grp->gr_name;
}  
