/*
	ioregs.cpp - read-only IO registers tool for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	The GTK3 version shows each register as a set of check buttons and combo
	boxes that are updated (never edited) every frame.  GTK4/GTK5 remove the
	old event-box/checkbox-editing style, so this version presents the same
	information as a read-only GtkColumnView: one row per register with its
	address, width, hexadecimal value and the individual bits.  The register
	enum mirrors gtk/tools/ioregsView.cpp so both front ends show the same set.
*/

#include "ioregs.h"
#include "../emulation.h"
#include "rowmodel.h"

/* ------------------------------------------------------------------ register table */

typedef struct
{
	const char *name;
	guint32 address;
	int bits;                 /* 8, 16 or 32 */
	DesmumeIOReg reg;
	gboolean arm7_only;
} IORegEntry;

static const IORegEntry sRegs[] =
{
	{ "REG_IME",        0x04000208, 16, DESMUME_IOREG_IME,        FALSE },
	{ "REG_IE",         0x04000210, 32, DESMUME_IOREG_IE,         FALSE },
	{ "REG_IF",         0x04000214, 32, DESMUME_IOREG_IF,         FALSE },
	{ "REG_IPCFIFOCNT", 0x04000184, 16, DESMUME_IOREG_IPCFIFOCNT, FALSE },
	{ "POWER_CR",       0x04000304, 16, DESMUME_IOREG_POWCNT,     FALSE },
	{ "REG_SPICNT",     0x040001C0, 16, DESMUME_IOREG_SPICNT,     TRUE  }, /* ARM7 only */
};

/* ----------------------------------------------------------------------- state */

static GtkWidget *sWindow = NULL;
static GtkWidget *sCpuDropDown = NULL;
static GListStore *sRows = NULL;
static DesmumeRow *sRowRefs[G_N_ELEMENTS(sRegs)] = { NULL };

static const char *const sColumnTitles[] =
{
	"Register", "Address", "Bits", "Value", "Binary"
};

static void
ClearTool(void)
{
	for (guint i = 0; i < G_N_ELEMENTS(sRowRefs); ++i)
	{
		if (sRowRefs[i] != NULL)
		{
			g_object_unref(sRowRefs[i]);
			sRowRefs[i] = NULL;
		}
	}
	if (sRows != NULL)
	{
		g_list_store_remove_all(sRows);
		g_object_unref(sRows);
		sRows = NULL;
	}
	sCpuDropDown = NULL;
	sWindow = NULL;
}

/* The window owns the widgets; a weak pointer keeps sWindow from dangling
 * after the user closes it, so update() can simply check for NULL. */
static void
OnWindowCloseRequest(GtkWindow *window, gpointer user_data)
{
	(void)window;
	(void)user_data;

	ClearTool();
	gtk_window_destroy(GTK_WINDOW(window));
}

static void
BuildRows(void)
{
	sRows = desmume_row_model_new();
	for (guint i = 0; i < G_N_ELEMENTS(sRegs); ++i)
	{
		const IORegEntry *entry = &sRegs[i];
		char address[16];
		g_snprintf(address, sizeof(address), "0x%08X", entry->address);
		char bits[8];
		g_snprintf(bits, sizeof(bits), "%d", entry->bits);

		DesmumeRow *row = desmume_row_new(entry->name, address, bits, "", "");
		sRowRefs[i] = row; /* one ref for our cache, one added to the store */
		g_list_store_append(sRows, G_OBJECT(row));
	}
}

static void
Open(void)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	sWindow = desmume_tool_window_new("IO registers : ARM9", NULL, 640, 420);
	g_object_add_weak_pointer(G_OBJECT(sWindow), (gpointer *)&sWindow);
	g_signal_connect(sWindow, "close-request",
	                 G_CALLBACK(OnWindowCloseRequest), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_window_set_child(GTK_WINDOW(sWindow), root);

	/* CPU selector: 0 = ARM9, 1 = ARM7.  A simple GtkDropDown with string
	 * objects keeps the tool free of any combo-box legacy API. */
	GtkStringList *cpus = gtk_string_list_new(NULL);
	gtk_string_list_append(cpus, "ARM9");
	gtk_string_list_append(cpus, "ARM7");
	sCpuDropDown = gtk_drop_down_new(G_LIST_MODEL(cpus), NULL);
	gtk_widget_set_halign(sCpuDropDown, GTK_ALIGN_START);
	gtk_widget_set_margin_start(sCpuDropDown, 8);
	gtk_widget_set_margin_end(sCpuDropDown, 8);
	gtk_box_append(GTK_BOX(root), sCpuDropDown);
	/* gtk_drop_down_new() is (transfer full): it already consumed the
	 * reference from gtk_string_list_new().  Unreferencing it again frees the
	 * string list while the dropdown's popup list view still holds it, so
	 * closing the window trips gtk_list_item_manager_clear_model() in GTK. */

	BuildRows();

	GtkWidget *view = desmume_column_view_new(G_LIST_MODEL(sRows),
	                                           sColumnTitles,
	                                           G_N_ELEMENTS(sColumnTitles));
	GtkWidget *scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(root), scrolled);

	gtk_window_present(GTK_WINDOW(sWindow));
}

static void
Update(void)
{
	if ((sWindow == NULL) || !gtk_widget_get_mapped(sWindow))
	{
		return;
	}

	const int cpu = gtk_drop_down_get_selected(GTK_DROP_DOWN(sCpuDropDown));
	const char *title = (cpu == 1) ? "IO registers : ARM7" : "IO registers : ARM9";
	const char *current_title = gtk_window_get_title(GTK_WINDOW(sWindow));
	if (g_strcmp0(current_title, title) != 0)
	{
		gtk_window_set_title(GTK_WINDOW(sWindow), title);
	}

	for (guint i = 0; i < G_N_ELEMENTS(sRegs); ++i)
	{
		const IORegEntry *entry = &sRegs[i];
		DesmumeRow *row = sRowRefs[i];
		if (row == NULL)
		{
			continue;
		}

		if (entry->arm7_only && (cpu != 1))
		{
			desmume_row_set(row, 0, "(ARM7 only)");
			desmume_row_set(row, 3, "-");
			desmume_row_set(row, 4, "-");
			continue;
		}

		const guint32 value = desmume_emu_read_ioreg(cpu, entry->reg);
		char hex[16];
		if (entry->bits <= 16)
		{
			g_snprintf(hex, sizeof(hex), "0x%04X", value & 0xFFFF);
		}
		else
		{
			g_snprintf(hex, sizeof(hex), "0x%08X", value);
		}
		desmume_row_set(row, 0, entry->name);
		desmume_row_set(row, 3, hex);

		/* Bit string, most significant first, grouped in nibbles. */
		char binary[40];
		int pos = 0;
		for (int bit = entry->bits - 1; bit >= 0; --bit)
		{
			if (((bit + 1) % 4) == 0 && (pos > 0))
			{
				binary[pos++] = ' ';
			}
			binary[pos++] = ((value >> bit) & 1) ? '1' : '0';
		}
		binary[pos] = '\0';
		desmume_row_set(row, 4, binary);
	}
}

static void
Close(void)
{
	if (sWindow != NULL)
	{
		gtk_window_destroy(GTK_WINDOW(sWindow));
	}
	/* OnWindowCloseRequest clears everything via the weak pointer; if no
	 * window is present (already destroyed), ClearTool is still idempotent. */
	ClearTool();
}

static gboolean
IsOpen(void)
{
	return sWindow != NULL;
}

const DesmumeTool desmume_tool_ioregs =
{
	"ioregs",
	"IO registers",
	Open,
	Update,
	Close,
	IsOpen
};
