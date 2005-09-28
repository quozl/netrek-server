/*
things to do some day

splash

metaserver list
player stats

in gum.xml via glade ...
maxload should be GtkEntry not spin button
add signal on column headers on port list for sorting
dogfight enable should be twin radio button not entry

in here ...
start/restart/stop buttons to _know_ if they are allowed, based on pid 
timeout statusbar messages so they go away after a while
on port list add, refuse if port already in list
on port list update, refuse if port number changed to one already in list
click on column headers in port list to re-sort
remove double quotes on process name when displayed/entered
implement starting planets properly ... more than one can be enabled per team
 */

/*  gum
 *  Copyright (C) 1999 James Cameron
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gdk/gdkprivate.h> /* for gdk_display */
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "signals.h"
#include "support.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "sysdefaults.h"
#include "solicit.h"

/* pointers to the top level widgets we will manage */
GtkWidget *gummain, *gumopen, *gumsaveas, *gummotd;

#define MAXLINES 2048		/* maximum size of input file   */
static char *lines[MAXLINES];	/* array of lines in input file	*/
static int keys[MAXLINES];	/* links from lines to keywords	*/
static int defs[MAXLINES];	/* links from keywords to lines	*/
static char filename[256];

static struct {
  GtkWidget *widget;
  guint context;
} statusbar;

/* replacement for Glade's lookup_widget that operates silently, so
that we can have options added to configuration without a widget yet
existing, and so that the code can check for the existence of a widget
without errors being displayed. */
GtkWidget*
widgie                                 (GtkWidget       *widget,
                                        gchar           *widget_name)
{
  GtkWidget *found_widget;

  if (widget->parent)
    widget = gtk_widget_get_toplevel (widget);
  found_widget = (GtkWidget*) gtk_object_get_data (GTK_OBJECT (widget),
                                                   widget_name);
  return found_widget;
}

/* given a title and a message text, pop up a message dialog */
void message ( char *title, char *text )
{
  GtkWidget *w, *x;

  w = create_message ();
  gtk_window_set_title (GTK_WINDOW (w), title);
  x = widgie(w, "message_label");
  gtk_label_set (GTK_LABEL(x), text);
  gtk_widget_show (w);
}

/* set a widget to a specific value or state */
int setwidget ( char *key, char *value, int state )
{
  GtkWidget *w;
  w = widgie (gummain, key);
  if (w != NULL) {
    if (GTK_IS_ENTRY(w)) {
      gtk_entry_set_text (GTK_ENTRY(w), value);
      return 1;
    }
    if (GTK_IS_SCALE(w)) {
      GtkAdjustment *adjustment;
      adjustment = gtk_range_get_adjustment(GTK_RANGE(w));
      gtk_adjustment_set_value (adjustment, (float) atoi(value));
      gtk_range_set_adjustment(GTK_RANGE(w), adjustment);
      return 1;
    }
    if (GTK_IS_OPTION_MENU(w)) {
      gtk_option_menu_set_history(GTK_OPTION_MENU(w), atoi(value));
      return 1;
    }
    printf("... %s is an unhandled GtkWidget\n", key );
    return 0;
  } else {
    char name[64];

    sprintf (name, "%s_%s", key, value);
    w = widgie (gummain, name);
    if (w != NULL) {
      if (GTK_IS_TOGGLE_BUTTON(w)) {
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(w), state);
	return 1;
      } else {
	printf("... %s is an unhandled GtkWidget\n", name);
	return 0;
      }
    }
  }
  return 0;
}


/*
gint
gtk_menu_get_active_index (GtkMenu *menu)
{
  GtkWidget *child;

  g_return_val_if_fail (menu != NULL, -1);
  g_return_val_if_fail (GTK_IS_MENU (menu), -1);

  child = gtk_menu_get_active (menu);

  return g_list_position (menu->children, (GList *) child);
}
*/

/* get a widget value, assuming a scalar parameter */
char *getwidget_scalar ( char *key )
{
  static char result[128];
  GtkWidget *w;

  w = widgie (gummain, key);
  if (w != NULL) {
    if (GTK_IS_ENTRY(w)) {
      strcpy (result, gtk_entry_get_text (GTK_ENTRY(w)));
      return result;
    }
    if (GTK_IS_SCALE(w)) {
      GtkAdjustment *adjustment;
      adjustment = gtk_range_get_adjustment(GTK_RANGE(w));
      sprintf (result, "%.0f", adjustment->value);
      return result;
    }
    if (GTK_IS_TOGGLE_BUTTON(w)) {
      GtkToggleButton *x = GTK_TOGGLE_BUTTON(w);
      return x->active ? "1" : "0";
    }
    /* disabled; doesn't work
    if (GTK_IS_OPTION_MENU(w)) {
      GtkOptionMenu *x = GTK_OPTION_MENU(w);
      GtkMenu *m = GTK_MENU(x->menu);
      gint i = gtk_menu_get_active_index (m);
      sprintf (result, "%d", i);
      printf ("getwidget: GtkOptionMenu %s returns %s\n", key, result);
      return result;
    }
    */
    printf("... %s is an unhandled GtkWidget\n", key );
    return NULL;
  } else {
    char name[64];
    int j;

    for(j=0;j<50;j++) {
      sprintf (name, "%s_%d", key, j);
      w = widgie (gummain, name);
      if (w == NULL) break;
      if (GTK_IS_TOGGLE_BUTTON(w)) {
	GtkToggleButton *x = GTK_TOGGLE_BUTTON(w);
	if (x->active) {
	  sprintf (result, "%d", j);
	  return result;
	}
      } else {
	printf("... %s is an unhandled GtkWidget\n", name);
      }
    }
    return "";
  }
  return "";
}

/* get a widget value, for an array parameter type */
char *getwidget_array (struct sysdef_keywords *sk)
{
  int k;
  struct sysdef_array *a = (struct sysdef_array *) sk->p;
  static char result[128];

  strcpy (result, "");
  for (k=0; k<a->max; k++) {
    char *subscript = a->keys[k];
    char name[64];
    char *value;

    sprintf (name, "%s_%s", sk->key, subscript);
    value = getwidget_scalar (name);
    if (value == NULL) continue;
    if (value[0] == '1') {
      if (strlen(result) > 0) strcat(result, ",");
      strcat(result, subscript);
    }
  }
  return result;
}

/* given a keyword, obtain the current widget value */
char *getwidget (struct sysdef_keywords *sk)
{
  switch (sk->type) {
  case SYSDEF_INT:
  case SYSDEF_FLOAT:
  case SYSDEF_CHAR:
  case SYSDEF_ROBOT:
    return getwidget_scalar (sk->key);
    break;
  case SYSDEF_ARRAY:
    return getwidget_array (sk);
    break;
  default:
    printf("getwidget: unhandled keyword %s\n", sk->key);
  }
  return "";
}

/* set the labels on the widgets */
void setlabels ( )
{
  int i;

  for (i=0; sysdef_keywords[i].type != SYSDEF_END; i++) {
    GtkWidget *w;
    char *k = sysdef_keywords[i].key;
    char key[64];

    sprintf (key, "%s_LABEL", k);
    w = widgie (gummain, key);
    if (w == NULL) {
      /* disable keywords that aren't on screen */
      sysdef_keywords[i].type = SYSDEF_NOTHING;
      continue;
    }
    gtk_label_set (GTK_LABEL(w), sysdef_keywords[i].text);
  }
}


static void rowtoentry (GtkCList *clist, int row, int column, char *name)
{
  char *value;

  GtkEntry *entry = GTK_ENTRY(widgie(gummain, name));
  gtk_clist_get_text (clist, row, column, &value);
  gtk_entry_set_text (entry, value);
}


gboolean
on_gum_delete_event                    (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gtk_main_quit ();
  return FALSE;
}


void
on_Open_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gumopen == NULL) {
    gumopen = create_open ();
    gtk_widget_show (gumopen);
  }
}

void
on_Reload_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  FILE *file;
  struct stat stat;
  char *buffer;
  int i;

  /* clear previous array of lines */
  if (lines[0] != NULL) free (lines[0]);
  for (i=0; i<MAXLINES; i++) {
    if (lines[i] == NULL) break;
    lines[i] = NULL;
  }

  /* clear the linkage from keywords to lines */
  for (i=0; sysdef_keywords[i].type != SYSDEF_END; i++) {
    defs[i] = -1;
  }

  /* open .sysdef file */
  file = fopen (filename, "r");
  if (file == NULL) {
    char buffer[1024];
    sprintf (buffer, "Cannot open system defaults file '%s',\n"
	     "'%s'.", filename, strerror(errno));
    message ("gum cannot open", buffer);
    return;
  }

  /* find size of file */
  if (fstat (fileno(file), &stat) < 0) {
    char buffer[1024];
    sprintf (buffer, "Cannot determine size of system defaults file '%s',\n"
	     "'%s'.", filename, strerror(errno));
    message ("gum cannot size", buffer);
    fclose (file);
    return;
  }

  /* allocate a buffer for the file */
  buffer = (char *) malloc (stat.st_size+1);
  if (buffer == NULL) {
    g_warning ("Cannot allocate buffer for .sysdef file.");
    fclose (file);
    return;
  }

  /* read entire file into buffer */
  if (fread(buffer, stat.st_size, 1, file) < 0) {
    g_warning ("Could not fread() .sysdef file");
    perror ("fread");
    fclose (file);
    return;
  }
  buffer[stat.st_size--] = '\0';
  if (buffer[stat.st_size] == '\n') buffer[stat.st_size--] = '\0';

  /* close .sysdef file */
  fclose (file);

  /* split buffer into lines */
  for (i=0; i<MAXLINES; i++) {
    int j;

    /* store a pointer to this line start */
    lines[i] = buffer;

    /* clear this relationship */
    keys[i] = -1;

    /* if no more lines, stop here */
    if (buffer == NULL) break;

    /* point to new line character at end of this line and replace it */
    buffer = index(buffer, '\n');
    if (buffer != NULL) *buffer++ = '\0';
    
    /* scan for it in the keywords list */
    for (j=0; sysdef_keywords[j].type != SYSDEF_END; j++) {
      char *p = lines[i];
      char *k = sysdef_keywords[j].key;
      int l = strlen(k);

      if (sysdef_keywords[j].type == SYSDEF_NOTHING) continue;

      /* skip if bad length or bad match */
      if (p[l] != '=') continue;
      if (strncmp(p, k, l) != 0) continue;

      /* set pointer to this line */
      defs[j] = i;
      keys[i] = j;
    } 
  }

  /* ??? if value not set in file, we leave value on screen as is! */

  /* set each widget's value */
  for (i=0; sysdef_keywords[i].type != SYSDEF_END; i++) {
    int ok = 0;
    int j;
    char *s;

    j = defs[i];
    if (j == -1) continue;

    s = lines[j];
    if (s == NULL) continue;

    s = index(s, '=');
    if (s == NULL) continue;
    s++;

    switch (sysdef_keywords[i].type) {
    case SYSDEF_INT:
    case SYSDEF_FLOAT:
    case SYSDEF_CHAR:
    case SYSDEF_ROBOT:
      ok += setwidget(sysdef_keywords[i].key, s, 1);
      break;
    case SYSDEF_ARRAY:
      {
	int k;
	struct sysdef_array *a = (struct sysdef_array *) sysdef_keywords[i].p;
	char *ours, *token;

	/* turn _off_ the ones not mentioned in this load */
	for (k=0; k<a->max; k++) 
	  setwidget (sysdef_keywords[i].key, a->keys[k], 0);

	ours = strdup(s);
	token = strtok(s, ",");
	while (token != NULL) {
	  ok += setwidget (sysdef_keywords[i].key, token, 1);
	  token = strtok(NULL, ",");
	}
	free(ours);
      }
    case SYSDEF_SHIP:
    }
    if (!ok) 
      printf("%s=%s ... could NOT be displayed\n", sysdef_keywords[i].key, s);
  }
  gtk_statusbar_push (GTK_STATUSBAR(statusbar.widget), statusbar.context, 
		      "System defaults loaded");
}


void
on_Save_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int i;
  int done[MAXLINES];
  FILE *file;

  for (i=0; i<MAXLINES; i++) done[i] = 0;

  /* open .sysdef file */
  file = fopen (filename, "w");
  if (file == NULL) {
    char buffer[1024];
    sprintf (buffer, "Cannot save to system defaults file '%s',\n"
	     "'%s'.", filename, strerror(errno));
    message ("gum cannot save", buffer);
    return;
  }

  /* scan the input file buffer, writing lines as input */
  for (i=0; i<MAXLINES; i++) {
    int j = keys[i];
    char *v;

    /* end of list, stop scan */
    if (lines[i] == NULL) break;

    /* non-data lines output in order of input */
    if (j == -1) {
      fprintf (file, "%s\n", lines[i]);
      continue;
    }

    /* data line, replace with current setting */
    v = getwidget (&sysdef_keywords[j]);
    fprintf (file, "%s=%s\n", sysdef_keywords[j].key, v );
    done[j]++;
  }

  /* output keywords that have not been output */
  for (i=0; sysdef_keywords[i].type != SYSDEF_END; i++) {
    if (done[i] == 0) {
      char *v;

      if (sysdef_keywords[i].type == SYSDEF_NOTHING) continue;

      v = getwidget (&sysdef_keywords[i]);
      if (v == NULL) continue;
      if (strlen(v) == 0) continue;
      fprintf (file, "%s=%s\n", sysdef_keywords[i].key, v);
      done[i]++;
    }
  }

  fclose (file);
  gtk_statusbar_push (GTK_STATUSBAR(statusbar.widget), statusbar.context, 
		      "System defaults saved");
}


void
on_Save_As_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gumsaveas == NULL) {
    gumsaveas = create_saveas ();
    gtk_widget_show (gumsaveas);
  }
}


void
on_Quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_main_quit ();
}


void
on_About_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  message ("About Netrek Server", "\
This is gum version 0.3 and provides a graphical interface to the\n\
Vanilla Netrek Server configuration files and listener program.\n\
\n\
dictionary: gum, a noun, God, especially in 'by gum' as a mild oath.\n\
\n\
To report problems with gum, please write to quozl@us.netrek.org\n\
  ");
}

void
on_reload_clicked                      (GtkButton       *button,
                                        gpointer         user_data)
{
  on_Reload_activate ( (GtkMenuItem *) button, user_data );
}


void
on_save_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
  on_Save_activate ( (GtkMenuItem *) button, user_data );
}


void
on_quit_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
  on_Quit_activate ( (GtkMenuItem *) button, user_data );
}


void
on_motd_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
  if (gummotd == NULL) {
    gummotd = create_motd ();
    gtk_widget_show (gummotd);
  }
}


void
on_motd_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  if (gummotd != NULL) {
    gtk_widget_destroy (gummotd);
    gummotd = NULL;
  }
}


gboolean
on_motd_delete_event                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  on_motd_cancel_clicked ((GtkButton *) widget, user_data);
  return FALSE;
}


void
on_motd_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkFileSelection *fs;

  fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel (GTK_WIDGET(button)));
  setwidget("MOTD", gtk_file_selection_get_filename (fs), 1);
  on_motd_cancel_clicked (button, user_data);
}


void
on_open_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  if (gumopen != NULL) {
    gtk_widget_destroy (gumopen);
    gumopen = NULL;
  }
}


gboolean
on_open_delete_event                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  on_open_cancel_clicked ((GtkButton *) widget, user_data);
  return FALSE;
}


void
on_open_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  char *new;
  GtkFileSelection *fs;

  fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel (GTK_WIDGET(button)));
  new = gtk_file_selection_get_filename(fs);

  if (access(new, R_OK) == 0) {
    strcpy (filename, new);
    on_Reload_activate (NULL, NULL);
    on_open_cancel_clicked (button, user_data);
  } else {
    char buffer[1024];
    sprintf (buffer, "Cannot access system defaults file '%s',\n"
	     "'%s'.", filename, strerror(errno));
    message ("gum cannot access", buffer);
  }
}


void
on_saveas_cancel_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  if (gumsaveas != NULL) {
    gtk_widget_destroy (gumsaveas);
    gumsaveas = NULL;
  }
}


gboolean
on_saveas_delete_event                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  on_saveas_cancel_clicked ((GtkButton *) widget, user_data);
  return FALSE;
}


void
on_saveas_ok_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
  char *new;
  GtkFileSelection *fs;

  fs = GTK_FILE_SELECTION(gtk_widget_get_toplevel (GTK_WIDGET(button)));
  new = gtk_file_selection_get_filename(fs);

  if (access(new, W_OK) == 0) {
    strcpy (filename, new);
    on_Save_activate (NULL, NULL);
    on_saveas_cancel_clicked (button, user_data);
  } else {
    char buffer[1024];
    sprintf (buffer, "Cannot access system defaults file '%s',\n"
	     "'%s'.", filename, strerror(errno));
    message ("gum cannot access", buffer);
  }
}




gboolean
on_message_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET(widget)));
  return FALSE;
}


void
on_message_ok                          (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET(button)));
}





gint listener_port_list_selected_row;
#define PORTLISTCOLS 7

void
on_listener_port_list_select_row       (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  rowtoentry (clist, row, 0, "listener_entry_port");
  rowtoentry (clist, row, 4, "listener_entry_program");
  rowtoentry (clist, row, 5, "listener_entry_process");
  rowtoentry (clist, row, 6, "listener_entry_arguments");

  /* enable delete and update */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_delete"), 1);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_update"), 1);

  listener_port_list_selected_row = row;
}


void
on_listener_port_list_unselect_row     (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  char *value = "";
  GtkEntry *port      = GTK_ENTRY(widgie(gummain, "listener_entry_port"));
  GtkEntry *program   = GTK_ENTRY(widgie(gummain, "listener_entry_program"));
  GtkEntry *process   = GTK_ENTRY(widgie(gummain, "listener_entry_process"));
  GtkEntry *arguments = GTK_ENTRY(widgie(gummain, "listener_entry_arguments"));

  /* clear entry fields */
  gtk_entry_set_text (port, value);
  gtk_entry_set_text (program, value);
  gtk_entry_set_text (process, value);
  gtk_entry_set_text (arguments, value);

  /* disable delete and update */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_delete"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_update"), 0);

  listener_port_list_selected_row = -1;
}


static int statistics = 0;

/* re-fetch stats from netrekd port */
gint timeout (gpointer data)
{
  GtkCList *clist = GTK_CLIST(widgie (gummain, "listener_port_list"));
  int fd;
  char buffer[1024], *line;
  struct sockaddr_in addr;
  FILE *file;
  gint row;

  if (clist == NULL) return FALSE;

  /* no statistics entry in loaded list */
  if (statistics == 0) return FALSE;

  /* create the socket */
  if ((fd = socket (AF_INET, SOCK_STREAM, 0)) == -1 ) {
    sprintf (buffer, "Cannot create socket for statistics connection,\n"
	     "'%s'.", strerror(errno));
    message ("gum cannot create socket", buffer);
    statistics = 0;
    return FALSE;
  }

  /* load the address */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(statistics);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  /* connect to the port */
  if (connect (fd, &addr, sizeof (addr))) {
    /* if we can't, quietly ignore it and come back again later */
    close (fd);
    return FALSE;
  }

  /* convert the file descriptor to a stdio stream */
  if ((file = fdopen (fd, "r")) == NULL) {
    sprintf (buffer, "Cannot fdopen socket\n"
	     "for statistics connection,\n"
	     "'%s'.", strerror(errno));
    message ("gum cannot create socket", buffer);
    statistics = 0;
    close (fd);
    return FALSE;
  }
  
  gtk_clist_freeze (clist);

  /* process each line of input from connection */
  while ((line = fgets (buffer, 1023, file)) != NULL) {
    char *port, *accepts, *denials, *forks;
    
    line[strlen(line)-1] = '\0';
    if (strlen(line) <= 7) break;
    
    port = strtok(line, "\t ");
    if (port == NULL) continue;
    if (!strcmp(port, "port")) continue;
    
    accepts = strtok(NULL, "\t ");
    if (accepts == NULL) continue;
    
    denials = strtok(NULL, "\t ");
    if (denials == NULL) continue;

    forks = strtok(NULL, "\t ");
    if (forks == NULL) continue;

    /* suppress superfluous zeroes */
    if (!strcmp(accepts, "0")) accepts = "";
    if (!strcmp(denials, "0")) denials = "";
    if (!strcmp(forks, "0")) forks = "";

    /* find the entry in the port list and update the statistics shown */
    for (row=0; ;row++) {
      char *found;
      
      if (gtk_clist_get_cell_type (clist, row, 0) != GTK_CELL_TEXT) break;
      gtk_clist_get_text (clist, row, 0, &found);
      if (!strcmp(found, port)) {
	gtk_clist_set_text (clist, row, 1, accepts);
	gtk_clist_set_text (clist, row, 2, denials);
	gtk_clist_set_text (clist, row, 3, forks);
      }
    }
  }
  gtk_clist_thaw (clist);

  fclose (file);
  close (fd);
  return TRUE;
}


void
on_listener_port_reload_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  FILE *ports;
  static gint tag = 0;
  GtkCList *clist = GTK_CLIST(widgie (gummain, "listener_port_list"));
  char buffer[256], *line;

  if (clist == NULL) return;

  if (tag != 0) {
    gtk_timeout_remove (tag);
  }

  ports = fopen(N_PORTS, "r");
  if (ports == NULL) {
    char buffer[1024];

    gtk_clist_freeze (clist);
    gtk_clist_clear (clist);
    gtk_clist_thaw (clist);
    sprintf (buffer, "Cannot open " N_PORTS " file,\n"
	     "'%s'.", strerror(errno));
    message ("gum cannot open", buffer);
    return;
  }

  gtk_clist_freeze (clist);
  while ((line = fgets (buffer, 255, ports))!= NULL) {
    char *port, *program, *process, *arguments, *columns[PORTLISTCOLS];

    if (line[0] == '#') continue;
    line[strlen(line)-1] = '\0';
    port = strtok (line, " ");
    if (port == NULL) continue;
    program = strtok (NULL, " ");
    if (program == NULL) continue;
    process = strtok (NULL, "\" ");
    if (process == NULL) continue;
    arguments = strtok (NULL, "\"\n");
    if (arguments == NULL) arguments = "";

    columns[0] = port;
    columns[1] = "";
    columns[2] = "";
    columns[3] = "";
    columns[4] = program;
    columns[5] = process;
    columns[6] = arguments;
    (void) gtk_clist_append (clist, columns);

    if (!strcmp(columns[4], "special"))
      if (!strcmp(columns[5], "statistics"))
	statistics = atoi(columns[0]);
  }
  fclose (ports);

  gtk_clist_thaw (clist);
  timeout (NULL);
  tag = gtk_timeout_add (10000, timeout, NULL);
}



void
on_listener_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data)
{
  GtkEntry *entry = GTK_ENTRY(editable);
  char *value = gtk_entry_get_text (entry);
  if (!listener_port_list_selected_row) {
    if (strlen(value) > 0) {
      gtk_widget_set_sensitive (widgie(gummain, "listener_port_add"), 1);
    } else {
      gtk_widget_set_sensitive (widgie(gummain, "listener_port_add"), 0);
    }
  }
}


void
on_listener_entry_port_changed         (GtkEditable     *editable,
                                        gpointer         user_data)
{
  on_listener_entry_changed (editable, user_data);
}


void
on_listener_entry_program_changed      (GtkEditable     *editable,
                                        gpointer         user_data)
{
  on_listener_entry_changed (editable, user_data);
}


void
on_listener_entry_process_changed      (GtkEditable     *editable,
                                        gpointer         user_data)
{
  on_listener_entry_changed (editable, user_data);
}


void
on_listener_entry_arguments_changed    (GtkEditable     *editable,
                                        gpointer         user_data)
{
}


void
on_listener_port_add_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
  char *columns[PORTLISTCOLS];
  GtkCList *clist     = GTK_CLIST(widgie(gummain, "listener_port_list"));
  GtkEntry *port      = GTK_ENTRY(widgie(gummain, "listener_entry_port"));
  GtkEntry *program   = GTK_ENTRY(widgie(gummain, "listener_entry_program"));
  GtkEntry *process   = GTK_ENTRY(widgie(gummain, "listener_entry_process"));
  GtkEntry *arguments = GTK_ENTRY(widgie(gummain, "listener_entry_arguments"));

  columns[0] = gtk_entry_get_text(port);
  columns[1] = "";
  columns[2] = "";
  columns[3] = "";
  columns[4] = gtk_entry_get_text(program);
  columns[5] = gtk_entry_get_text(process);
  columns[6] = gtk_entry_get_text(arguments);

  if (strlen(columns[0]) == 0) return;
  if (strlen(columns[4]) == 0) return;
  if (strlen(columns[5]) == 0) return;

  /* move fields to clist */
  (void) gtk_clist_append (clist, columns);

  /* check for added statistics entry */
  if (!strcmp(columns[4], "special"))
    if (!strcmp(columns[5], "statistics"))
      statistics = atoi(columns[0]);

  /* clear entry fields */
  gtk_entry_set_text (port, "");
  gtk_entry_set_text (program, "");
  gtk_entry_set_text (process, "");
  gtk_entry_set_text (arguments, "");

  /* disable add, enable save */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_add"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_save"), 1);
}


void
on_listener_port_update_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  char *columns[PORTLISTCOLS];
  GtkCList *clist     = GTK_CLIST(widgie(gummain, "listener_port_list"));
  GtkEntry *port      = GTK_ENTRY(widgie(gummain, "listener_entry_port"));
  GtkEntry *program   = GTK_ENTRY(widgie(gummain, "listener_entry_program"));
  GtkEntry *process   = GTK_ENTRY(widgie(gummain, "listener_entry_process"));
  GtkEntry *arguments = GTK_ENTRY(widgie(gummain, "listener_entry_arguments"));
  int row = listener_port_list_selected_row;

  columns[0] = gtk_entry_get_text(port);
  columns[4] = gtk_entry_get_text(program);
  columns[5] = gtk_entry_get_text(process);
  columns[6] = gtk_entry_get_text(arguments);

  if (strlen(columns[0]) == 0) return;
  if (strlen(columns[4]) == 0) return;
  if (strlen(columns[5]) == 0) return;

  /* move fields to clist */
  gtk_clist_freeze (clist);
  gtk_clist_set_text (clist, row, 0, columns[0]);
  gtk_clist_set_text (clist, row, 4, columns[4]);
  gtk_clist_set_text (clist, row, 5, columns[5]);
  gtk_clist_set_text (clist, row, 6, columns[6]);
  gtk_clist_thaw (clist);

  /* clear fields */
  gtk_entry_set_text (port, "");
  gtk_entry_set_text (program, "");
  gtk_entry_set_text (process, "");
  gtk_entry_set_text (arguments, "");

  /* disable update, disable delete, enable save button  */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_update"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_delete"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_save"), 1);

  /* unselect row */
  gtk_clist_unselect_row (clist, row, 0);
}


void
on_listener_port_delete_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkCList *clist     = GTK_CLIST(widgie(gummain, "listener_port_list"));
  GtkEntry *port      = GTK_ENTRY(widgie(gummain, "listener_entry_port"));
  GtkEntry *program   = GTK_ENTRY(widgie(gummain, "listener_entry_program"));
  GtkEntry *process   = GTK_ENTRY(widgie(gummain, "listener_entry_process"));
  GtkEntry *arguments = GTK_ENTRY(widgie(gummain, "listener_entry_arguments"));
  int row = listener_port_list_selected_row;

  /* unselect row */
  gtk_clist_unselect_row (clist, row, 0);

  /* delete selected row */
  gtk_clist_remove (clist, row);

  /* clear fields */
  gtk_entry_set_text (port, "");
  gtk_entry_set_text (program, "");
  gtk_entry_set_text (process, "");
  gtk_entry_set_text (arguments, "");

  /* disable update, disable delete, enable save button  */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_update"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_delete"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_save"), 1);
}


void
on_listener_port_save_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkCList *clist     = GTK_CLIST(widgie(gummain, "listener_port_list"));
  gint row;
  FILE *ports;

  ports = fopen (N_PORTS, "w");
  if (ports == NULL) {
    char buffer[1024];
    sprintf (buffer, "Cannot open " N_PORTS " file,\n"
	     "'%s'.", strerror(errno));
    message ("gum cannot open", buffer);
    return;
  }
  fprintf (ports, "# .ports written by gum\n");
  fprintf (ports, "# see docs/sample_ports in source kit for format\n");

  /* write list to .ports file */
  for (row=0; ;row++) {
    char *port, *program, *process, *arguments;

    if (gtk_clist_get_cell_type (clist, row, 0) != GTK_CELL_TEXT) break;

    gtk_clist_get_text (clist, row, 0, &port);
    gtk_clist_get_text (clist, row, 4, &program);
    gtk_clist_get_text (clist, row, 5, &process);
    gtk_clist_get_text (clist, row, 6, &arguments);

    fprintf (ports, "%s %s \"%s\" %s\n", port, program, process, arguments);
  }
  fprintf (ports, "# end of .ports\n");
  fclose (ports);

  /* disable save button */
  gtk_widget_set_sensitive (widgie(gummain, "listener_port_save"), 0);
  gtk_statusbar_push (GTK_STATUSBAR(statusbar.widget), statusbar.context, 
		      "Ports list saved");
}


void reaper ()
{
  int child, us;
  us = wait(&child);
}


void
on_listener_enable_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
  static int x = 0;

  x = ! x;
  gtk_widget_set_sensitive (widgie(gummain, "listener_start"), x);
  gtk_widget_set_sensitive (widgie(gummain, "listener_restart"), x);
  gtk_widget_set_sensitive (widgie(gummain, "listener_stop"), x);
  gtk_label_set (GTK_LABEL(button->child), x ? "Disable" : "Enable" );
}


void
on_listener_start_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  FILE *file;
  pid_t pid;
  char *program = BINDIR"/netrekd";

  file = fopen(N_NETREKDPID, "r");
  if (file != NULL) {
    fscanf (file, "%d", &pid);
    fclose (file);
    if (kill (pid, SIGUSR1) == 0) {
      char buffer[1024];
      sprintf (buffer, "Listener was not started, "
	       "as it is already running as process id %d" , pid);
      message ("gum netrekd running", buffer);
      return;
    }
  }

  /* prevent X socket from being copied to forked process */
  if (fcntl(ConnectionNumber(gdk_display), F_SETFD, FD_CLOEXEC) < 0) {
    char buffer[1024];
    sprintf (buffer, "Listener was not started,\n"
	     "gum failed to set the X socket to close on exec(),\n"
	     "fcntl F_SETFD FD_CLOEXEC failure,\n'%s'",
	     strerror(errno));
    message ("gum fcntl failure", buffer);
    return;
  }

  signal(SIGCHLD, reaper);

  if (access(program, X_OK) != 0) {
    char buffer[1024];
    sprintf (buffer, 
	     "Listener was not started,\n"
	     "no execute access to\n"
	     "%s,\n"
	     "'%s'.", program, strerror(errno));
    message ("gum netrekd access", buffer);
  } else {
    if (fork() == 0) {
      (void) chdir (BINDIR);
      (void) execlp (program, "netrekd", NULL);
      exit(0);
    }
  }

  gtk_statusbar_push (GTK_STATUSBAR(statusbar.widget), statusbar.context, 
		      "Listener started" );
}


void listener_signal (int sig, char *result)
{
  FILE *file;
  pid_t pid;
  char *pidfile = N_NETREKDPID;

  file = fopen(pidfile, "r");
  if (file == NULL) {
    char buffer[1024];
    sprintf (buffer, "Cannot open %s file,\n"
	     "'%s',\n"
	     "(If you have never started it, this is normal)", 
	     pidfile, strerror(errno));
    message ("gum cannot find listener", buffer);
    return;
  }

  fscanf (file, "%d", &pid);
  fclose (file);

  if (kill (pid, sig) != 0) {
    char buffer[1024];
    sprintf (buffer, "Cannot send signal %s to listener pid %d,\n"
	     "'%s',\n", sig == SIGHUP ? "SIGHUP" : "SIGINT",
	     pid, strerror(errno));
    message ("gum cannot kill listener", buffer);
    return;
  }

  gtk_statusbar_push (GTK_STATUSBAR(statusbar.widget), statusbar.context, 
		      result);
}


void
on_listener_restart_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  listener_signal (SIGHUP, "Listener restarted");
}


void
on_listener_stop_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  listener_signal (SIGINT, "Listener stopped!");
}


gint metaserver_selected_row;
#define METALISTCOLS 9

void
on_metaserver_list_select_row          (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  metaserver_selected_row = row;

  rowtoentry (clist, row, 0, "metaserver_entry_host_name");
  rowtoentry (clist, row, 1, "metaserver_entry_port_number");
  rowtoentry (clist, row, 2, "metaserver_entry_minimum_update_time");
  rowtoentry (clist, row, 3, "metaserver_entry_maximum_update_time");
  rowtoentry (clist, row, 4, "metaserver_entry_server_host_name");
  rowtoentry (clist, row, 5, "metaserver_entry_server_type");
  rowtoentry (clist, row, 6, "metaserver_entry_player_port");
  rowtoentry (clist, row, 7, "metaserver_entry_observer_port");
  rowtoentry (clist, row, 8, "metaserver_entry_additional_comments");

  gtk_widget_set_sensitive (widgie(gummain, "metaserver_delete"), 1);
  gtk_widget_set_sensitive (widgie(gummain, "metaserver_update"), 1);
}


void metaserver_entry_clear ( )
{
  setwidget ("metaserver_entry_host_name",           "",  0);
  setwidget ("metaserver_entry_port_number",         "",  0);
  setwidget ("metaserver_entry_minimum_update_time", "",  0);
  setwidget ("metaserver_entry_maximum_update_time", "",  0);
  setwidget ("metaserver_entry_server_host_name",    "",  0);
  setwidget ("metaserver_entry_server_type",         "",  0);
  setwidget ("metaserver_entry_player_port",         "",  0);
  setwidget ("metaserver_entry_observer_port",       "",  0);
  setwidget ("metaserver_entry_additional_comments", "",  0);
}


void
on_metaserver_list_unselect_row     (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  metaserver_selected_row = -1;
  metaserver_entry_clear();

  gtk_widget_set_sensitive (widgie(gummain, "metaserver_delete"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "metaserver_update"), 0);
}


void
on_metaserver_reload_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
  FILE *file;
  GtkCList *clist = GTK_CLIST(widgie (gummain, "metaserver_list"));
  char buffer[MAXMETABYTES], *line;

  if (clist == NULL) return;
  metaserver_selected_row = -1;

  file = fopen(SYSCONFDIR"/.metaservers", "r");
  if (file == NULL) {
    gtk_clist_freeze (clist);
    gtk_clist_clear (clist);
    gtk_clist_thaw (clist);
    return;
  }

  gtk_clist_freeze (clist);
  while ((line = fgets (buffer, MAXMETABYTES, file))!= NULL) {
    int i;
    char *p, *columns[METALISTCOLS];

    for(i=0;i<METALISTCOLS;i++) columns[i] = "";

    line[strlen(line)-1] = '\0';

    p = strtok (line, " ");
    if (p == NULL) continue;

    for(i=0;i<METALISTCOLS;i++) {
      p = strtok (NULL, " ");
      if (p == NULL) break;
      columns[i] = p;
    }
    if (p == NULL) continue;

    (void) gtk_clist_append (clist, columns);

  }
  fclose (file);

  gtk_clist_thaw (clist);
}



void
on_metaserver_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{
  GtkEntry *entry = GTK_ENTRY(editable);
  char *value = gtk_entry_get_text (entry);
  if (metaserver_selected_row == -1) {
    if (strlen(value) > 0) {
      gtk_widget_set_sensitive (widgie(gummain, "metaserver_add"), 1);
    } else {
      gtk_widget_set_sensitive (widgie(gummain, "metaserver_add"), 0);
    }
  }
}


void
on_metaserver_add_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  int i;
  char *columns[METALISTCOLS];
  GtkCList *clist     = GTK_CLIST(widgie(gummain, "metaserver_list"));

  columns[0] = strdup(getwidget_scalar ("metaserver_entry_host_name"));
  columns[1] = strdup(getwidget_scalar ("metaserver_entry_port_number"));
  columns[2] = strdup(getwidget_scalar ("metaserver_entry_minimum_update_time"));
  columns[3] = strdup(getwidget_scalar ("metaserver_entry_maximum_update_time"));
  columns[4] = strdup(getwidget_scalar ("metaserver_entry_server_host_name"));
  columns[5] = strdup(getwidget_scalar ("metaserver_entry_server_type"));
  columns[6] = strdup(getwidget_scalar ("metaserver_entry_player_port"));
  columns[7] = strdup(getwidget_scalar ("metaserver_entry_observer_port"));
  columns[8] = strdup(getwidget_scalar ("metaserver_entry_additional_comments"));

  for(i=0;i<METALISTCOLS;i++) {
    if (strlen(columns[i]) == 0) {
      for(i=0;i<METALISTCOLS;i++) free(columns[i]);
      return;
    }
  }

  /* move fields to clist */
  (void) gtk_clist_append (clist, columns);

  /* clear entry fields */
  metaserver_entry_clear();

  /* disable add, enable save */
  gtk_widget_set_sensitive (widgie(gummain, "metaserver_add"), 0);
  gtk_widget_set_sensitive (widgie(gummain, "metaserver_save"), 1);

  for(i=0;i<METALISTCOLS;i++) free(columns[i]);
}


void
on_metaserver_update_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
    message ("unimplemented", "function unimplemented at this time, edit the .metaservers file yourself.");

}


void
on_metaserver_delete_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
    message ("unimplemented", "function unimplemented at this time, edit the .metaservers file yourself.");

}


void
on_metaserver_save_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    message ("unimplemented", "function unimplemented at this time, edit the .metaservers file yourself.");

}

int
main (int argc, char *argv[])
{
  int i;
  GtkWidget *welcome;
  

  /* initialise sysdef buffer */
  for (i=0; i<MAXLINES; i++) { lines[i] = NULL; defs[i] = -1; }

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  add_pixmap_directory (PACKAGE_DATA_DIR "/share/gum/pixmaps");
  
  gumopen = NULL;
  gumsaveas = NULL;
  gummotd = NULL;

  welcome = create_welcome ();
  gtk_widget_show (welcome);
  gummain = create_gum ();
  statusbar.widget = widgie (gummain, "statusbar");
  statusbar.context =
    gtk_statusbar_get_context_id (GTK_STATUSBAR(statusbar.widget), "");
  setlabels ();
  on_listener_port_reload_clicked (NULL, NULL);
  on_metaserver_reload_clicked (NULL, NULL);
  strcpy (filename, SYSCONFDIR"/.sysdef");
  on_Reload_activate (NULL, NULL);

  while ( gtk_events_pending() ) {
    gtk_main_iteration();
  }
  sleep (1);
  while ( gtk_events_pending() ) {
    gtk_main_iteration();
  }

  gtk_widget_destroy (welcome);
  gtk_widget_show (gummain);
  gtk_main ();
  return 0;
}
