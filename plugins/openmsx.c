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
	GtkWidget		*sock;		/* OpenMSX plugin window */
	GtkWidget		*vbox;
	GtkWidget		*name_label;
}
OpenMSXWindow;

static EditWindow openmsx_window = {NULL, NULL, NULL, NULL};


static void on_stop(GtkMenuItem *menuitem, gpointer user_data);


static GtkWidget *main_menu_item = NULL;
/* default configuration stuff */
static gchar *welcome_text = NULL;
static gchar *openmsx_command = "/usr/bin/openmsx";


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


/* Callback when the menu item is clicked. */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *dialog;
	GeanyPlugin *plugin = gdata;
	GeanyData *geany_data = plugin->geany_data;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(geany_data->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"%s", welcome_text);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), plugin->info->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* Called by Geany to initialize the plugin */
static gboolean openmsx_init(GeanyPlugin *plugin, gpointer data)
{
	GtkWidget *openmsx_item;
	GeanyData *geany_data = plugin->geany_data;

	/* Add an item to the Tools menu */
	openmsx_item = gtk_menu_item_new_with_mnemonic(_("_OpenMSX"));
	gtk_widget_show(openmsx_item);
	gtk_container_add(GTK_CONTAINER(geany_data->main_widgets->tools_menu), openmsx_item);
	g_signal_connect(openmsx_item, "activate", G_CALLBACK(item_activate), plugin);

	/* make the menu item sensitive only when documents are open */
	ui_add_document_sensitive(openmsx_item);
	/* keep a pointer to the menu item, so we can remove it when the plugin is unloaded */
	main_menu_item = openmsx_item;

	welcome_text = g_strdup(_("Hello World!"));

	/* This might seem strange but is a method to get the GeanyPlugin pointer passed to
	 * on_editor_notify(). PluginCallback functions get the same data that was set via
	 * GEANY_PLUING_REGISTER_FULL() or geany_plugin_set_data() by default (unless the data pointer
	 * was set to non-NULL at compile time).
	 * This is really only done for demoing PluginCallback. Actual plugins will use real custom
	 * data and perhaps embed the GeanyPlugin or GeanyData pointer their if they also use
	 * PluginCallback. */
	//geany_plugin_set_data(plugin, plugin, NULL);
	return TRUE;
}


/* Callback connected in openmsx_configure(). */
static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	/* catch OK or Apply clicked */
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		g_log("[OpenMSX]", G_LOG_LEVEL_DEBUG, "on_configure_response called.");

		/* We only have one pref here, but for more you would use a struct for user_data */
		GtkWidget *entry = GTK_WIDGET(user_data);

		welcome_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		/* maybe the plugin should write here the settings into a file
		 * (e.g. using GLib's GKeyFile API)
		 * all plugin specific files should be created in:
		 * geany->app->configdir G_DIR_SEPARATOR_S plugins G_DIR_SEPARATOR_S pluginname G_DIR_SEPARATOR_S
		 * e.g. this could be: ~/.config/geany/plugins/Demo/, please use geany->app->configdir */
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
	label = gtk_label_new(_("Welcome text to show:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();
	if (welcome_text != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), welcome_text);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);

	gtk_widget_show_all(vbox);

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), entry);
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

void plugin_init(GeanyData *data)
{
	GtkWidget *item, *menu;
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
	//key_group = plugin_set_key_group(geany_plugin, "split_window", KB_COUNT, NULL);
	//keybindings_set_item(key_group, KB_SPLIT_HORIZONTAL, kb_activate,
		//0, 0, "split_horizontal", _("Side by Side"), menu_items.horizontal);
	//keybindings_set_item(key_group, KB_SPLIT_VERTICAL, kb_activate,
		//0, 0, "split_vertical", _("Top and Bottom"), menu_items.vertical);
	//keybindings_set_item(key_group, KB_SPLIT_UNSPLIT, kb_activate,
		//0, 0, "split_unsplit", _("_Unsplit"), menu_items.unsplit);
}
