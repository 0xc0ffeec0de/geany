/*
 */

/**
 * OpenMSX plugin for Geany.
 */


#include "geanyplugin.h"	/* plugin API, always comes first */
//#include "Scintilla.h"	/* for the SCNotification struct */


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)
PLUGIN_SET_INFO(_("OpenMSX Emulator"), _("Opens instance of OpenMSX inside a window."),
	VERSION, _("Pedro de Medeiros"))


GeanyData		*geany_data;
GeanyPlugin		*geany_plugin;


/* Keybinding(s) */
enum
{
	OPENMSX_START,
	OPENMSX_STOP,
	OPENMSX_PAUSED,
	OPENMSX_RESTART,
	OPENMSX_COUNT
};

enum State
{
	STATE_STOPPED,
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_COUNT
};

static struct
{
	GtkWidget *start;
	GtkWidget *pause;
	GtkWidget *stop;
}
menu_items;

static enum State plugin_state;

static int acceleration;


typedef struct OpenMSXWindow
{
	GtkWidget		*socket;		/* OpenMSX plugin window */
	GtkWidget		*vbox;
	GtkWidget		*name_label;
}
OpenMSXWindow;

static EditWindow openmsx_window = {NULL, NULL, NULL, NULL};


typedef struct OpenMSXWindow
{
	GtkWidget		*command_entry;
	GtkWidget		*parameters_entry;
}
OpenMSXDialog;

static OpenMSXDialog openmsx_dialog = {NULL, NULL};


static void on_stop_emulator(GtkMenuItem *menuitem, gpointer user_data);


static GtkWidget *main_menu_item = NULL;

/* default configuration stuff */

typedef struct OpenMSXUserData
{
	gchar *command;
	gint socket;
	gchar *openmsx_parameters;
	gchar **saved_states;
	int num_saved_states;
}
OpenMSXUserData;

static OpenMSXUserData openmsx_userdata = {
	"/usr/bin/openmsx",
	0,
	"",
	NULL,
	0
};


static PluginCallback openmsx_callbacks[] =
{
	/* Set 'after' (third field) to TRUE to run the callback @a after the default handler.
	 * If 'after' is FALSE, the callback is run @a before the default handler, so the plugin
	 * can prevent Geany from processing the notification. Use this with care. */
	{ "document-activate", (GCallback) &on_openmsx_activate, FALSE, NULL },
	{ "document-close", (GCallback) &on_opemsx_close, FALSE, NULL },
	{ "project-before-close", (GCallback) &on_opemsx_close, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

static void start_emulator()
{

}

static void on_start_emulator(GtkMenuItem *menuitem, gpointer user_data)
{
	split_view();
	start_emulator();
}

static void on_pause_emulator(GtkMenuItem *menuitem, gpointer user_data)
{
	pause_emulator();
}

static void on_stop_emulator(GtkMenuItem *menuitem, gpointer user_data)
{
	stop_emulator();
	unsplit_view();
}

static void set_state(enum State id)
{
	gtk_widget_set_sensitive(menu_items.start,
		(id != STATE_RUNNING));
	gtk_widget_set_sensitive(menu_items.pause,
		(id != STATE_PAUSED) && (id != STATE_STOPPED));
	gtk_widget_set_sensitive(menu_items.stop,
		id != STATE_STOPPED);

	plugin_state = id;
}


static void split_view()
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *parent = gtk_widget_get_parent(notebook);
	GtkWidget *pane, *toolbar, *box, *socket;
	gint width = gtk_widget_get_allocated_width(notebook) / 2;
	gint height = gtk_widget_get_allocated_height(notebook) / 2;
	gboolean horizontal = TRUE;

	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(parent), notebook);

	pane = horizontal ? gtk_hpaned_new() : gtk_vpaned_new();
	gtk_container_add(GTK_CONTAINER(parent), pane);

	gtk_container_add(GTK_CONTAINER(pane), notebook);
	g_object_unref(notebook);

	box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pane), box);

	toolbar = create_toolbar();
	gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);
	openmsx_window.vbox = box;

	socket = gtk_socket_new();
	gtk_widget_set_size_request(socket, 640, 480);
	gtk_box_pack_start(GTK_BOX(box), toolbar, TRUE, FALSE, 0);
	openmsx_window.socket = socket;

	gtk_paned_set_position(GTK_PANED(pane), width);
	gtk_widget_show_all(pane);
}


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data)
{
	unsplit_view();
}


static void unsplit_view()
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *pane = gtk_widget_get_parent(notebook);
	GtkWidget *parent = gtk_widget_get_parent(pane);

	set_state(STATE_UNSPLIT);

	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(pane), notebook);

	gtk_widget_destroy(pane);
	edit_window.editor = NULL;
	edit_window.sci = NULL;

	gtk_container_add(GTK_CONTAINER(parent), notebook);
	g_object_unref(notebook);
}


static gboolean openmsx_init(GeanyPlugin *plugin, gpointer data)
{
	GtkWidget *item, *menu;
	GeanyData *geany_data = plugin->geany_data;
	GeanyKeyGroup *key_group;

	menu_items.main = item = gtk_menu_item_new_with_mnemonic(_("_OpenMSX"));
	gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu), item);
	ui_add_document_sensitive(item);

	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items.main), menu);

	menu_items.start = item =
		gtk_menu_item_new_with_mnemonic(_("_Start"));
	g_signal_connect(item, "activate", G_CALLBACK(on_start_emulator), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	menu_items.pause = item =
		gtk_menu_item_new_with_mnemonic(_("_Pause"));
	g_signal_connect(item, "activate", G_CALLBACK(on_pause_emulator), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	menu_items.stop = item =
		gtk_menu_item_new_with_mnemonic(_("_Stop"));
	g_signal_connect(item, "activate", G_CALLBACK(on_stop_emulator), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu_items.main);

	set_state(STATE_STOPPED);

	/* setup keybindings */
	//key_group = plugin_set_key_group(geany_plugin, "openmsx", KB_COUNT, NULL);
	//keybindings_set_item(key_group, KB_SPLIT_HORIZONTAL, kb_activate,
		//0, 0, "split_horizontal", _("Side by Side"), menu_items.horizontal);
	//keybindings_set_item(key_group, KB_SPLIT_VERTICAL, kb_activate,
		//0, 0, "split_vertical", _("Top and Bottom"), menu_items.vertical);
	//keybindings_set_item(key_group, KB_SPLIT_UNSPLIT, kb_activate,
		//0, 0, "split_unsplit", _("_Unsplit"), menu_items.unsplit);

	/* keep a pointer to the menu item, so we can remove it when the plugin is unloaded */
	main_menu_item = openmsx_item;

	return TRUE;
}


static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	/* catch OK or Apply clicked */
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		g_log("[OpenMSX]", G_LOG_LEVEL_DEBUG, "on_configure_response called.");

		OpenMSXDialog *dialog = user_data;

		openmsx_userdata.command = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog.command_entry)));
		openmsx_userdata.parameters = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog.parameters_entry)));
	}
}

/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * openmsx_init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *       dialog. */
static GtkWidget *openmsx_configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer data)
{
	GtkWidget *label, *entry, *vbox;

	/* example configuration dialog */
	vbox = gtk_vbox_new(FALSE, 6);

	/* add a label and a text entry to the dialog */
	label = gtk_label_new(_("OpenMSX command:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();

	if (openmsx_userdata.command != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), openmsx_userdata.command);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	openmsx_dialog.command_entry = entry;

	label = gtk_label_new(_("OpenMSX parameters:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();

	if (openmsx_userdata.parameters != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), openmsx_userdata.parameters);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	openmsx_dialog.parameters_entry = entry;

	gtk_widget_show_all(vbox);

	// TODO: save state GUI elements.

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), openmsx_dialog);
	return vbox;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before openmsx_init(). */
static void openmsx_cleanup(GeanyPlugin *plugin, gpointer data)
{
	/* remove the menu item added in openmsx_init() */
	gtk_widget_destroy(main_menu_item);
	/* release other allocated strings and objects */
	g_free(welcome_text);
}

void geany_load_module(GeanyPlugin *plugin)
{
	/* main_locale_init() must be called for your package before any localization can be done */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	plugin->info->name = _("OpenMSX");
	plugin->info->description = _("OpenMSX Geany plugin.");
	plugin->info->version = "0.1";
	plugin->info->author =  _("Pedro Vaz de Mello de Medeiros");

	plugin->funcs->init = openmsx_init;
	plugin->funcs->configure = openmsx_configure;
	plugin->funcs->help = NULL; /* This demo has no help but it is an option */
	plugin->funcs->cleanup = openmsx_cleanup;
	plugin->funcs->callbacks = openmsx_callbacks;

	GEANY_PLUGIN_REGISTER(plugin, 225);
}

