/*  Note: You are free to use whatever license you want.
    Eventually you will be able to edit it within Glade. */

/*  gum
 *  Copyright (C) <YEAR> <AUTHORS>
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

#include <gtk/gtk.h>

gboolean
on_gum_delete_event                    (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_Open_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Reload_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Save_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Save_As_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_About_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_listener_port_list_select_row       (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_listener_port_list_unselect_row     (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_listener_entry_port_changed         (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_listener_entry_program_changed      (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_listener_entry_process_changed      (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_listener_entry_arguments_changed    (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_listener_port_add_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_port_update_clicked        (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_port_delete_clicked        (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_port_save_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_enable_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_start_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_restart_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_listener_stop_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_add_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_update_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_delete_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_save_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_motd_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_reload_clicked                      (GtkButton       *button,
                                        gpointer         user_data);

void
on_save_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_quit_clicked                        (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_motd_delete_event                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_motd_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_motd_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_message_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_message_ok                          (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_open_delete_event                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_open_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_open_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_saveas_delete_event                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_saveas_ok_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_saveas_cancel_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_list_select_row          (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_metaserver_list_unselect_row        (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_metaserver_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_metaserver_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_metaserver_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_metaserver_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);
