/* gtd-new-task-row.c
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "GtdNewTaskRow"

#include "gtd-debug.h"
#include "gtd-manager.h"
#include "gtd-new-task-row.h"
#include "gtd-provider.h"
#include "gtd-rows-common-private.h"
#include "gtd-task.h"
#include "gtd-task-list.h"
#include "gtd-task-list-popover.h"
#include "gtd-task-list-view.h"
#include "gtd-utils.h"

#include <glib/gi18n.h>
#include <math.h>

struct _GtdNewTaskRow
{
  GtkBin              parent;

  GtkEntry           *entry;
  GtdTaskListPopover *tasklist_popover;

  gboolean            show_list_selector;
};

G_DEFINE_TYPE (GtdNewTaskRow, gtd_new_task_row, GTK_TYPE_BIN)

enum
{
  ENTER,
  EXIT,
  NUM_SIGNALS
};

enum
{
  PROP_0,
  N_PROPS
};

static guint          signals [NUM_SIGNALS] = { 0, };

/*
 * Auxiliary methods
 */

static void
update_secondary_icon (GtdNewTaskRow *self)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GdkRGBA) color = NULL;
  g_autofree gchar *tooltip = NULL;
  GtdTaskList *selected_list;

  if (!self->show_list_selector)
    {
      gtk_entry_set_icon_from_paintable (self->entry, GTK_ENTRY_ICON_SECONDARY, NULL);
      return;
    }

  selected_list = gtd_task_list_popover_get_task_list (GTD_TASK_LIST_POPOVER (self->tasklist_popover));

  if (!selected_list)
    return;

  color = gtd_task_list_get_color (selected_list);
  paintable = gtd_create_circular_paintable (color, 12);

  gtk_entry_set_icon_from_paintable (self->entry, GTK_ENTRY_ICON_SECONDARY, paintable);

  /* Translators: %1$s is the task list name, %2$s is the provider name */
  tooltip = g_strdup_printf (_("%1$s \t <small>%2$s</small>"),
                             gtd_task_list_get_name (selected_list),
                             gtd_provider_get_description (gtd_task_list_get_provider (selected_list)));
  gtk_entry_set_icon_tooltip_markup (self->entry, GTK_ENTRY_ICON_SECONDARY, tooltip);
}

static void
show_task_list_selector_popover (GtdNewTaskRow *self)
{
  GdkRectangle rect;

  gtk_entry_get_icon_area (self->entry, GTK_ENTRY_ICON_SECONDARY, &rect);
  gtk_popover_set_pointing_to (GTK_POPOVER (self->tasklist_popover), &rect);
  gtk_popover_popup (GTK_POPOVER (self->tasklist_popover));
}

/*
 * Callbacks
 */

static void
entry_activated_cb (GtdNewTaskRow *self)
{
  GtdTaskListView *view;
  GtdTaskList *list;
  GListModel *model;

  /* Cannot create empty tasks */
  if (gtk_entry_get_text_length (self->entry) == 0)
    return;

  view = GTD_TASK_LIST_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (self), GTD_TYPE_TASK_LIST_VIEW));

  /* If there's a task list set, always go for it */
  model = gtd_task_list_view_get_model (view);
  list = GTD_IS_TASK_LIST (model) ? GTD_TASK_LIST (model) : NULL;

  /*
   * If there is no current list set, use the default list from the
   * default provider.
   */
  if (!list)
    list = gtd_task_list_popover_get_task_list (GTD_TASK_LIST_POPOVER (self->tasklist_popover));

  if (!list)
    {
      GtdProvider *provider = gtd_manager_get_default_provider (gtd_manager_get_default ());
      list = gtd_provider_get_default_task_list (provider);
    }

  g_return_if_fail (GTD_IS_TASK_LIST (list));

  gtd_provider_create_task (gtd_task_list_get_provider (list),
                            list,
                            gtk_editable_get_text (GTK_EDITABLE (self->entry)),
                            gtd_task_list_view_get_default_date (view));

  gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
}

static void
on_entry_icon_released_cb (GtkEntry             *entry,
                           GtkEntryIconPosition  position,
                           GtdNewTaskRow        *self)
{
  switch (position)
    {
    case GTK_ENTRY_ICON_PRIMARY:
      entry_activated_cb (self);
      break;

    case GTK_ENTRY_ICON_SECONDARY:
      show_task_list_selector_popover (self);
      break;
    }
}

static void
on_focus_in_cb (GtkEventControllerKey *event_controller,
                GdkCrossingMode        mode,
                GdkNotifyType          detail,
                GtdNewTaskRow         *self)
{
  GTD_ENTRY;

  g_signal_emit (self, signals[ENTER], 0);

  GTD_EXIT;
}

static void
on_tasklist_popover_changed_cb (GtdTaskListPopover *popover,
                                GParamSpec         *pspec,
                                GtdNewTaskRow      *self)
{
  GTD_ENTRY;

  update_secondary_icon (self);

  GTD_EXIT;
}

static void
on_tasklist_popover_closed_cb (GtdTaskListPopover *popover,
                               GtdNewTaskRow      *self)
{
  GTD_ENTRY;

  //gtk_entry_grab_focus_without_selecting (self->entry);
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));

  GTD_EXIT;
}


static void
gtd_new_task_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtd_new_task_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtd_new_task_row_class_init (GtdNewTaskRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gtd_new_task_row_get_property;
  object_class->set_property = gtd_new_task_row_set_property;

  widget_class->measure = gtd_row_measure_with_max;

  /**
   * GtdNewTaskRow::enter:
   *
   * Emitted when the row is focused and in the editing state.
   */
  signals[ENTER] = g_signal_new ("enter",
                                 GTD_TYPE_NEW_TASK_ROW,
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL,
                                 NULL,
                                 NULL,
                                 G_TYPE_NONE,
                                 0);

  /**
   * GtdNewTaskRow::exit:
   *
   * Emitted when the row is unfocused and leaves the editing state.
   */
  signals[EXIT] = g_signal_new ("exit",
                                GTD_TYPE_NEW_TASK_ROW,
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE,
                                0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/task-list-view/gtd-new-task-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GtdNewTaskRow, entry);
  gtk_widget_class_bind_template_child (widget_class, GtdNewTaskRow, tasklist_popover);

  gtk_widget_class_bind_template_callback (widget_class, entry_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_icon_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_focus_in_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_tasklist_popover_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_tasklist_popover_closed_cb);

  gtk_widget_class_set_css_name (widget_class, "newtaskrow");

  g_type_ensure (GTD_TYPE_TASK_LIST_POPOVER);
}

static void
gtd_new_task_row_init (GtdNewTaskRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_secondary_icon (self);
}

GtkWidget*
gtd_new_task_row_new (void)
{
  return g_object_new (GTD_TYPE_NEW_TASK_ROW, NULL);
}

gboolean
gtd_new_task_row_get_active (GtdNewTaskRow *self)
{
  g_return_val_if_fail (GTD_IS_NEW_TASK_ROW (self), FALSE);

  return gtk_widget_has_focus (GTK_WIDGET (self->entry));
}

void
gtd_new_task_row_set_active (GtdNewTaskRow *self,
                             gboolean       active)
{
  g_return_if_fail (GTD_IS_NEW_TASK_ROW (self));

  if (active)
    gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

void
gtd_new_task_row_set_show_list_selector (GtdNewTaskRow *self,
                                         gboolean       show_list_selector)
{
  g_return_if_fail (GTD_IS_NEW_TASK_ROW (self));

  if (self->show_list_selector == show_list_selector)
    return;

  self->show_list_selector = show_list_selector;
  update_secondary_icon (self);
}
