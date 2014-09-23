/*
 * Copyright (C) 2010-2014 jeanfi@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <stdlib.h>

#include <gtk/gtk.h>

#include <cfg.h>
#include <temperature.h>
#include <ui_pref.h>
#include <ui_sensorlist.h>
#include <ui_sensorpref.h>
#include <ui_color.h>

#if defined(HAVE_APPINDICATOR)
#include <ui_appindicator.h>
#endif

enum {
	COL_NAME = 0,
	COL_SENSOR_PREF
};

struct sensor_pref {
	struct psensor *sensor;
	char *name;
	int graph_enabled;
	struct color *color;
	int alarm_enabled;
	int alarm_high_threshold;
	int alarm_low_threshold;
	unsigned int appindicator_enabled;
	unsigned int appindicator_label_enabled;
	unsigned int display_enabled;
};

struct cb_data {
	struct ui_psensor *ui;
	GtkBuilder *builder;
};

static struct sensor_pref *
sensor_pref_new(struct psensor *s, struct config *cfg)
{
	struct sensor_pref *p;

	p = malloc(sizeof(struct sensor_pref));

	p->sensor = s;
	p->name = strdup(s->name);
	p->graph_enabled = s->graph_enabled;
	p->alarm_enabled = s->alarm_enabled;
	p->color = color_dup(s->color);
	p->display_enabled = config_is_sensor_enabled(s->id);

	if (cfg->temperature_unit == CELSIUS) {
		p->alarm_high_threshold = s->alarm_high_threshold;
		p->alarm_low_threshold = s->alarm_low_threshold;
	} else {
		p->alarm_high_threshold
			= celsius_to_fahrenheit(s->alarm_high_threshold);
		p->alarm_low_threshold
			= celsius_to_fahrenheit(s->alarm_low_threshold);
	}

	p->appindicator_enabled = s->appindicator_enabled;
	p->appindicator_label_enabled
		= config_is_appindicator_label_enabled(s->id);

	return p;
}

static void sensor_pref_free(struct sensor_pref *p)
{
	if (!p)
		return;

	free(p->name);
	free(p->color);

	free(p);
}

static struct sensor_pref *get_selected_sensor_pref(GtkTreeView *tree)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct sensor_pref *pref;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(tree);

	pref = NULL;
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, COL_SENSOR_PREF, &pref, -1);

	return pref;
}

void ui_sensorpref_name_changed_cb(GtkEntry *entry, gpointer data)
{
	struct sensor_pref *p;
	const char *str;

	str = gtk_entry_get_text(entry);

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p && strcmp(p->name, str)) {
		free(p->name);
		p->name = strdup(str);
	}
}

void ui_sensorpref_draw_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->graph_enabled = gtk_toggle_button_get_active(btn);
}

void ui_sensorpref_display_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->display_enabled = gtk_toggle_button_get_active(btn);
}

void ui_sensorpref_alarm_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->alarm_enabled = gtk_toggle_button_get_active(btn);
}

void
ui_sensorpref_appindicator_menu_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->appindicator_enabled = gtk_toggle_button_get_active(btn);
}

void
ui_sensorpref_appindicator_label_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->appindicator_label_enabled
			= gtk_toggle_button_get_active(btn);
}

void ui_sensorpref_color_set_cb(GtkColorButton *widget, gpointer data)
{
	struct sensor_pref *p;
	GdkRGBA color;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
		color_set(p->color, color.red, color.green, color.blue);
	}
}

void
ui_sensorpref_alarm_high_threshold_changed_cb(GtkSpinButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->alarm_high_threshold = gtk_spin_button_get_value(btn);
}

void
ui_sensorpref_alarm_low_threshold_changed_cb(GtkSpinButton *btn, gpointer data)
{
	struct sensor_pref *p;

	p = get_selected_sensor_pref(GTK_TREE_VIEW(data));

	if (p)
		p->alarm_low_threshold = gtk_spin_button_get_value(btn);
}

static void
update_pref(struct sensor_pref *p, struct config *cfg, GtkBuilder *builder)
{
	GtkLabel *w_id, *w_type, *w_high_threshold_unit, *w_low_threshold_unit,
		*w_chipname;
	GtkEntry *w_name;
	GtkToggleButton *w_draw, *w_alarm, *w_appindicator_enabled,
		*w_appindicator_label_enabled, *w_display;
	GtkColorButton *w_color;
	GtkSpinButton *w_high_threshold, *w_low_threshold;
	GdkRGBA color;
	struct psensor *s;
	int use_celsius;

	s = p->sensor;

	w_id = GTK_LABEL(gtk_builder_get_object(builder, "sensor_id"));
	gtk_label_set_text(w_id, s->id);

	w_type = GTK_LABEL(gtk_builder_get_object(builder, "sensor_type"));
	gtk_label_set_text(w_type, psensor_type_to_str(s->type));

	w_name = GTK_ENTRY(gtk_builder_get_object(builder, "sensor_name"));
	gtk_entry_set_text(w_name, p->name);

	w_chipname = GTK_LABEL(gtk_builder_get_object(builder, "chip_name"));
	if (s->chip)
		gtk_label_set_text(w_chipname, s->chip);
	else
		gtk_label_set_text(w_chipname, _("Unknown"));

	w_draw = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
							  "sensor_draw"));
	gtk_toggle_button_set_active(w_draw, p->graph_enabled);

	w_display = GTK_TOGGLE_BUTTON(gtk_builder_get_object
				      (builder,
				       "sensor_enable_checkbox"));
	gtk_toggle_button_set_active(w_display, p->display_enabled);

	color = color_to_GdkRGBA(p->color);
	w_color = GTK_COLOR_BUTTON(gtk_builder_get_object(builder,
							  "sensor_color"));
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w_color), &color);

	w_alarm = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
							   "sensor_alarm"));
	w_high_threshold = GTK_SPIN_BUTTON(gtk_builder_get_object
					  (builder,
					   "sensor_alarm_high_threshold"));
	w_low_threshold = GTK_SPIN_BUTTON(gtk_builder_get_object
					 (builder,
					  "sensor_alarm_low_threshold"));

	w_high_threshold_unit = GTK_LABEL(gtk_builder_get_object
					 (builder,
					  "sensor_alarm_high_threshold_unit"));
	w_low_threshold_unit = GTK_LABEL(gtk_builder_get_object
					(builder,
					 "sensor_alarm_low_threshold_unit"));

	use_celsius = cfg->temperature_unit == CELSIUS ? 1 : 0;
	gtk_label_set_text(w_high_threshold_unit,
			   psensor_type_to_unit_str(s->type,
						    use_celsius));
	gtk_label_set_text(w_low_threshold_unit,
			   psensor_type_to_unit_str(s->type,
						    use_celsius));

	w_appindicator_enabled = GTK_TOGGLE_BUTTON
		(gtk_builder_get_object(builder, "indicator_checkbox"));
	w_appindicator_label_enabled = GTK_TOGGLE_BUTTON
		(gtk_builder_get_object(builder, "indicator_label_checkbox"));


#if !HAVE_APPINDICATOR
	gtk_widget_set_sensitive(GTK_WIDGET(w_appindicator_label_enabled),
				 FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(w_appindicator_enabled), FALSE);
	gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_label_enabled),
				   TRUE);
	gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_enabled), TRUE);
#else
	gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_label_enabled),
				   FALSE);
	gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_enabled), FALSE);
#endif

	gtk_toggle_button_set_active(w_alarm, p->alarm_enabled);
	gtk_spin_button_set_value(w_high_threshold, p->alarm_high_threshold);
	gtk_spin_button_set_value(w_low_threshold, p->alarm_low_threshold);
	gtk_widget_set_sensitive(GTK_WIDGET(w_alarm), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(w_high_threshold), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(w_low_threshold), TRUE);

	gtk_toggle_button_set_active(w_appindicator_enabled,
				     p->appindicator_enabled);

	gtk_toggle_button_set_active(w_appindicator_label_enabled,
				     p->appindicator_label_enabled);
}

static void on_changed(GtkTreeSelection *selection, gpointer data)
{
	struct cb_data *cbdata = data;
	struct ui_psensor *ui = cbdata->ui;
	struct sensor_pref *p;
	GtkTreeView *tree;

	tree = GTK_TREE_VIEW(gtk_builder_get_object(cbdata->builder,
						    "sensors_list"));
	p = get_selected_sensor_pref(tree);
	update_pref(p, ui->config, cbdata->builder);
}

static void
select_sensor(struct psensor *s, struct psensor **sensors, GtkTreeView *tree)
{
	struct psensor **s_cur;
	int i;
	GtkTreePath *p;

	p = NULL;
	for (s_cur = sensors, i = 0; *s_cur; s_cur++, i++)
		if (s == *s_cur) {
			p = gtk_tree_path_new_from_indices(i, -1);
			break;
		}

	if (p) {
		GtkTreeSelection *s = gtk_tree_view_get_selection(tree);

		gtk_tree_selection_select_path(s, p);
		gtk_tree_path_free(p);
	}
}

static void apply_pref(struct sensor_pref *p, int pos, struct config *cfg)
{
	struct psensor *s;

	s = p->sensor;

	if (strcmp(p->name, s->name)) {
		free(s->name);
		s->name = strdup(p->name);
		config_set_sensor_name(s->id, s->name);
	}

	if (s->graph_enabled != p->graph_enabled) {
		s->graph_enabled = p->graph_enabled;
		config_set_sensor_graph_enabled(s->id, s->graph_enabled);
	}

	if (is_temp_type(s->type) && cfg->temperature_unit == FAHRENHEIT) {
		s->alarm_high_threshold
			= fahrenheit_to_celsius(p->alarm_high_threshold);
		s->alarm_low_threshold
			= fahrenheit_to_celsius(p->alarm_low_threshold);
	} else {
		s->alarm_high_threshold = p->alarm_high_threshold;
		s->alarm_low_threshold = p->alarm_low_threshold;
	}

	config_set_sensor_alarm_high_threshold(s->id, s->alarm_high_threshold);
	config_set_sensor_alarm_low_threshold(s->id, s->alarm_low_threshold);

	if (s->alarm_enabled != p->alarm_enabled) {
		s->alarm_enabled = p->alarm_enabled;
		config_set_sensor_alarm_enabled(s->id, s->alarm_enabled);
	}

	color_set(s->color,
		  p->color->red,
		  p->color->green,
		  p->color->blue);
	config_set_sensor_color(s->id, s->color);

	if (s->appindicator_enabled != p->appindicator_enabled) {
		s->appindicator_enabled = p->appindicator_enabled;
		config_set_appindicator_enabled(s->id, s->appindicator_enabled);
	}

	config_set_appindicator_label_enabled(s->id,
					      p->appindicator_label_enabled);

	config_set_sensor_position(s->id, pos);

	config_set_sensor_enabled(s->id, p->display_enabled);
}

static void apply_prefs(GtkTreeModel *model, struct config *cfg)
{
	gboolean valid;
	struct sensor_pref *spref;
	GtkTreeIter iter;
	int i;

	valid = gtk_tree_model_get_iter_first(model, &iter);
	i = 0;
	while (valid) {
		gtk_tree_model_get(model, &iter, COL_SENSOR_PREF, &spref, -1);
		apply_pref(spref, i, cfg);
		valid = gtk_tree_model_iter_next(model, &iter);
		i++;
	}
	config_sync();
}

void ui_sensorpref_dialog_run(struct psensor *sensor, struct ui_psensor *ui)
{
	GtkDialog *diag;
	gint result;
	guint ok;
	GtkBuilder *builder;
	GError *error;
	GtkTreeView *w_sensors_list;
	GtkListStore *store;
	struct psensor **s_cur, *s, **ordered_sensors;
	GtkTreeSelection *selection;
	struct cb_data cbdata;
	GtkTreeIter iter;
	struct sensor_pref *spref;
	gboolean valid;
	GtkTreeModel *model;

	cbdata.ui = ui;

	builder = gtk_builder_new();
	cbdata.builder = builder;

	error = NULL;
	ok = gtk_builder_add_from_file
		(builder,
		 PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "sensor-edit.glade",
		 &error);

	if (!ok) {
		log_printf(LOG_ERR, error->message);
		g_error_free(error);
		return;
	}

	w_sensors_list
		= GTK_TREE_VIEW(gtk_builder_get_object(builder,
						       "sensors_list"));
	gtk_builder_connect_signals(builder, w_sensors_list);

	store = GTK_LIST_STORE(gtk_builder_get_object(builder,
						      "sensors_liststore"));

	ordered_sensors = ui_get_sensors_ordered_by_position(ui);
	for (s_cur = ordered_sensors; *s_cur; s_cur++) {
		s = *s_cur;
		gtk_list_store_append(store, &iter);

		spref = sensor_pref_new(s, ui->config);
		gtk_list_store_set(store, &iter,
				   COL_NAME, s->name,
				   COL_SENSOR_PREF, spref,
				   -1);

		if (s == sensor)
			update_pref(spref, ui->config, builder);
	}

	selection = gtk_tree_view_get_selection(w_sensors_list);
	g_signal_connect(selection, "changed", G_CALLBACK(on_changed), &cbdata);
	select_sensor(sensor, ordered_sensors, w_sensors_list);

	free(ordered_sensors);

	diag = GTK_DIALOG(gtk_builder_get_object(builder, "dialog1"));
	result = gtk_dialog_run(diag);

	model = gtk_tree_view_get_model(w_sensors_list);

	if (result == GTK_RESPONSE_ACCEPT) {
		apply_prefs(model, ui->config);
		ui_sensorlist_update(ui, 1);
#if defined(HAVE_APPINDICATOR)
		ui_appindicator_update_menu(ui);
#endif
	}

	valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid) {
		gtk_tree_model_get(model, &iter, COL_SENSOR_PREF, &spref, -1);
		sensor_pref_free(spref);
		valid = gtk_tree_model_iter_next(model, &iter);
	}

	g_object_unref(G_OBJECT(builder));

	gtk_widget_destroy(GTK_WIDGET(diag));
}
