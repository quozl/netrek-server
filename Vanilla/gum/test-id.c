#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
#include <gdk/gdkprivate.h> /* for gdk_display */
#include <gtk/gtk.h>
#include "id.h"
#include "main.h"

/* The real and effective IDs of the user to print. */
static uid_t ruid, euid;
static gid_t rgid, egid;

int main (int argc, char **argv) 
{
  static gid_t games = 20;
  gid_t *groups;
  int group_count;
  int i;
  struct passwd *pwd;

  pwd  = getpwnam("tanner");
  if (pwd == NULL) {
    printf("No such user");
    exit(1);
  }

  ruid = euid = pwd->pw_uid;
  rgid = egid = pwd->pw_gid;
  
  printf("ruid %d\n", ruid);
  printf("rgid %d\n", rgid);

  /* Get all the groups this user belows in and save them as gid_t in
     the groups array */

  if ((group_count = get_all_groups("tanner", &groups)) < 0) {
    perror("Problem getting supplementary groups");
    exit(1);
  }

  for (i = 0; i < group_count; i++) {
    if (groups[i] != rgid && groups[i] != egid) {
      printf("Group %d\n", groups[i]);
    }
  }

  if (search_4group(101, group_count, groups) == 1) {
    printf("User is in the group\n");
  } else {
    printf("User is not in the group\n");
  }


  free(groups);
}

void message(char *title, char *text)
{
  GtkWidget *w, *x;

  w = create_message ();
  gtk_window_set_title (GTK_WINDOW (w), title);
  x = widgie(w, "message_label");
  gtk_label_set (GTK_LABEL(x), text);
  gtk_widget_show (w);
}
