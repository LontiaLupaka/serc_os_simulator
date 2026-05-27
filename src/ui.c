#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/ui.h"
#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/ipc.h"
#include "../include/deadlock.h"
#include "../include/memory.h"
#include "../include/logger.h"

GtkWidget *window, *main_box;
GtkWidget *live_process_grid;
GtkWidget *scheduler_output_label;

static guint scheduler_timer = 0;

/* INPUT FIELDS */
GtkWidget *e_type, *e_sev, *e_lives, *e_loc;
GtkWidget *e_urg, *e_people, *e_damage;

/* ===================== PROCESS SCREEN DECLARATION ===================== */
void process_screen();
void log_screen();

/* -------- UTIL -------- */

GtkWidget* title(const char *text) {

    GtkWidget *l = gtk_label_new(text);

    gtk_widget_add_css_class(l, "title");

    return l;
}

GtkWidget* colored_label(const char *text, const char *class_name) {

    GtkWidget *l = gtk_label_new(text);

    gtk_widget_add_css_class(l, class_name);

    return l;
}

static const char *ui_log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARNING: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/* -------- SCREEN MANAGEMENT -------- */

void clear() {

    GtkWidget *child =
        gtk_widget_get_first_child(main_box);

    while (child) {

        GtkWidget *next =
            gtk_widget_get_next_sibling(child);

        gtk_box_remove(GTK_BOX(main_box), child);

        child = next;
    }
}

void show(GtkWidget *w) {

    clear();

    gtk_box_append(GTK_BOX(main_box), w);
}

/* -------- HOME -------- */

void home();

static void go_home(GtkButton *btn, gpointer user_data) {

    (void)btn;
    (void)user_data;

    /* STOP SCHEDULER TIMER IF RUNNING */
    if (scheduler_timer != 0) {
        g_source_remove(scheduler_timer);
        scheduler_timer = 0;
    }

    home();
}

/* -------- ADD PROCESS -------- */

void add_proc_real(GtkButton *btn, gpointer user_data) {

    (void)btn;
    (void)user_data;

    const char *type =
        gtk_editable_get_text(GTK_EDITABLE(e_type));

    int sev =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_sev)));

    int lives =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_lives)));

    int loc =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_loc)));

    int urg =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_urg)));

    int people =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_people)));

    int damage =
        atoi(gtk_editable_get_text(GTK_EDITABLE(e_damage)));

    if (strlen(type) == 0) {

        GtkAlertDialog *dialog =
            gtk_alert_dialog_new(
                "%s",
                "Missing Emergency Type"
            );

        gtk_alert_dialog_set_detail(
            dialog,
            "Please enter an emergency type."
        );

        gtk_alert_dialog_show(
            dialog,
            GTK_WINDOW(window)
        );

        return;
    }

    if (sev <= 0 || sev > 10 ||
        urg <= 0 || urg > 5) {

        GtkAlertDialog *dialog =
            gtk_alert_dialog_new(
                "%s",
                "Invalid Input Parameters"
            );

        gtk_alert_dialog_set_detail(
            dialog,
            "Severity must be between 1-10\n"
            "Urgency must be between 1-5"
        );

        gtk_alert_dialog_show(
            dialog,
            GTK_WINDOW(window)
        );

        return;
    }

    int created = add_process_auto(
        type,
        sev,
        lives,
        loc,
        urg,
        people,
        damage
    );

    GtkAlertDialog *dialog;

    if (!created) {

        dialog =
            gtk_alert_dialog_new(
                "%s",
                "Process Creation Failed"
            );

        gtk_alert_dialog_set_detail(
            dialog,
            "The process could not be admitted.\n"
            "Check memory capacity and Banker resource safety."
        );
    }
    else {

        dialog =
            gtk_alert_dialog_new(
                "%s",
                "Process Created Successfully"
            );

        gtk_alert_dialog_set_detail(
            dialog,
            "Emergency process added successfully."
        );
    }

    gtk_alert_dialog_show(
        dialog,
        GTK_WINDOW(window)
    );

    /* CLEAR INPUTS */

    gtk_editable_set_text(GTK_EDITABLE(e_type), "");
    gtk_editable_set_text(GTK_EDITABLE(e_sev), "");
    gtk_editable_set_text(GTK_EDITABLE(e_lives), "");
    gtk_editable_set_text(GTK_EDITABLE(e_loc), "");
    gtk_editable_set_text(GTK_EDITABLE(e_urg), "");
    gtk_editable_set_text(GTK_EDITABLE(e_people), "");
    gtk_editable_set_text(GTK_EDITABLE(e_damage), "");
}

/* -------- CREATE -------- */

void create() {

    /* STOP SCHEDULER TIMER IF RUNNING */
    if (scheduler_timer != 0) {
        g_source_remove(scheduler_timer);
        scheduler_timer = 0;
    }

    GtkWidget *box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("🚨 Emergency Process Creation"));

    GtkWidget *info_label =
        gtk_label_new("Configure emergency response parameters below");

    gtk_widget_add_css_class(info_label, "info-text");

    gtk_box_append(GTK_BOX(box), info_label);

    GtkWidget *form_box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);

    /* TYPE */
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(type_box, "form-group");

    GtkWidget *type_label = gtk_label_new("Emergency Type");
    gtk_widget_add_css_class(type_label, "form-label");

    e_type = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_type), "e.g., Ambulance, Fire, Police");

    gtk_box_append(GTK_BOX(type_box), type_label);
    gtk_box_append(GTK_BOX(type_box), e_type);
    gtk_box_append(GTK_BOX(form_box), type_box);

    /* SEVERITY */
    GtkWidget *sev_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(sev_box, "form-group");

    GtkWidget *sev_label = gtk_label_new("Severity Level (1-10)");
    gtk_widget_add_css_class(sev_label, "form-label");

    e_sev = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_sev), "1-10 (10 = Critical)");

    gtk_box_append(GTK_BOX(sev_box), sev_label);
    gtk_box_append(GTK_BOX(sev_box), e_sev);
    gtk_box_append(GTK_BOX(form_box), sev_box);

    /* LIVES */
    GtkWidget *lives_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(lives_box, "form-group");

    GtkWidget *lives_label = gtk_label_new("Lives at Risk");
    gtk_widget_add_css_class(lives_label, "form-label");

    e_lives = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_lives), "0 = No, 1 = Yes");

    gtk_box_append(GTK_BOX(lives_box), lives_label);
    gtk_box_append(GTK_BOX(lives_box), e_lives);
    gtk_box_append(GTK_BOX(form_box), lives_box);

    /* LOCATION */
    GtkWidget *loc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(loc_box, "form-group");

    GtkWidget *loc_label = gtk_label_new("Location Type");
    gtk_widget_add_css_class(loc_label, "form-label");

    e_loc = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_loc), "1 = Urban, 2 = Remote");

    gtk_box_append(GTK_BOX(loc_box), loc_label);
    gtk_box_append(GTK_BOX(loc_box), e_loc);
    gtk_box_append(GTK_BOX(form_box), loc_box);

    /* URGENCY */
    GtkWidget *urg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(urg_box, "form-group");

    GtkWidget *urg_label = gtk_label_new("Response Urgency (1-5)");
    gtk_widget_add_css_class(urg_label, "form-label");

    e_urg = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_urg), "1-5 (5 = Immediate)");

    gtk_box_append(GTK_BOX(urg_box), urg_label);
    gtk_box_append(GTK_BOX(urg_box), e_urg);
    gtk_box_append(GTK_BOX(form_box), urg_box);

    /* PEOPLE */
    GtkWidget *people_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(people_box, "form-group");

    GtkWidget *people_label = gtk_label_new("People Affected");
    gtk_widget_add_css_class(people_label, "form-label");

    e_people = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_people), "Number of people affected");

    gtk_box_append(GTK_BOX(people_box), people_label);
    gtk_box_append(GTK_BOX(people_box), e_people);
    gtk_box_append(GTK_BOX(form_box), people_box);

    /* DAMAGE */
    GtkWidget *damage_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(damage_box, "form-group");

    GtkWidget *damage_label = gtk_label_new("Damage Level (1-10)");
    gtk_widget_add_css_class(damage_label, "form-label");

    e_damage = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_damage), "1-10 (10 = Total destruction)");

    gtk_box_append(GTK_BOX(damage_box), damage_label);
    gtk_box_append(GTK_BOX(damage_box), e_damage);
    gtk_box_append(GTK_BOX(form_box), damage_box);

    gtk_box_append(GTK_BOX(box), form_box);

    /* BUTTONS */
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);

    GtkWidget *add = gtk_button_new_with_label("➕ Create Emergency Process");

    g_signal_connect(add, "clicked", G_CALLBACK(add_proc_real), NULL);

    GtkWidget *back = gtk_button_new_with_label("⬅ Back to Main");
    gtk_widget_add_css_class(back, "secondary");

    g_signal_connect(back, "clicked", G_CALLBACK(go_home), NULL);

    gtk_box_append(GTK_BOX(button_box), back);
    gtk_box_append(GTK_BOX(button_box), add);

    gtk_box_append(GTK_BOX(box), button_box);

    show(box);
}

/* ===================== PROCESS VIEW SCREEN ===================== */

void process_screen() {

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("📋 Process Manager"));

    static char buffer[15000];
    memset(buffer, 0, sizeof(buffer));

    print_process_state(buffer);

    GtkWidget *label = gtk_label_new(buffer);
    gtk_widget_add_css_class(label, "output-panel");

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *back = gtk_button_new_with_label("⬅ Back to Main");
    gtk_widget_add_css_class(back, "secondary");

    g_signal_connect(back, "clicked", G_CALLBACK(go_home), NULL);

    gtk_box_append(GTK_BOX(box), back);

    show(box);
}

void log_screen() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("📄 System Logs"));

    LogEntry logs[100];
    int log_count = 0;

    logger_get_recent_logs(logs, 100, &log_count);

    static char buffer[20000];
    buffer[0] = '\0';

    if (log_count == 0) {
        strcpy(buffer, "No system logs available.\n");
    } else {
        for (int i = 0; i < log_count; i++) {
            struct tm tm_info;
            localtime_r(&logs[i].timestamp, &tm_info);

            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

            char line[1024];
            snprintf(
                line,
                sizeof(line),
                "[%s][%s][%s] %s\n",
                time_str,
                ui_log_level_to_string(logs[i].level),
                logs[i].component,
                logs[i].message
            );

            strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
        }
    }

    GtkWidget *label = gtk_label_new(buffer);
    gtk_widget_add_css_class(label, "output-panel");
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *back = gtk_button_new_with_label("⬅ Back to Main");
    gtk_widget_add_css_class(back, "secondary");
    g_signal_connect(back, "clicked", G_CALLBACK(go_home), NULL);
    gtk_box_append(GTK_BOX(box), back);

    show(box);
}

void refresh_live_process_table() {

    GtkWidget *child =
        gtk_widget_get_first_child(
            live_process_grid
        );

    while (child) {

        GtkWidget *next =
            gtk_widget_get_next_sibling(child);

        gtk_grid_remove(
            GTK_GRID(live_process_grid),
            child
        );

        child = next;
    }

    const char *headers[] = {
        "PID",
        "TYPE",
        "STATE",
        "PRIORITY",
        "REMAINING",
        "MEMORY",
        "EVENT"
    };

    for (int i = 0; i < 7; i++) {

        GtkWidget *h = gtk_label_new(headers[i]);

        gtk_widget_add_css_class(h, "table-header");

        gtk_grid_attach(
            GTK_GRID(live_process_grid),
            h,
            i,
            0,
            1,
            1
        );
    }

    int row = 1;

    for (int i = 0; i < process_count; i++) {

        char state[32];

        get_process_state_string(
            processes[i].state,
            state
        );

        char pid[32];
        char priority[32];
        char remaining[32];
        char memory[32];

        sprintf(pid, "P%d", processes[i].pid);
        sprintf(priority, "%d", processes[i].priority);
        sprintf(remaining, "%d", processes[i].remaining_time);
        sprintf(memory, "%d MB", processes[i].memory_allocated);

        GtkWidget *labels[7];

        labels[0] = gtk_label_new(pid);
        labels[1] = gtk_label_new(processes[i].type);
        labels[2] = gtk_label_new(state);
        labels[3] = gtk_label_new(priority);
        labels[4] = gtk_label_new(remaining);
        labels[5] = gtk_label_new(memory);
        labels[6] = gtk_label_new(processes[i].last_event);

        for (int c = 0; c < 7; c++) {

            gtk_widget_add_css_class(
                labels[c],
                "table-cell"
            );

            gtk_grid_attach(
                GTK_GRID(live_process_grid),
                labels[c],
                c,
                row,
                1,
                1
            );
        }

        row++;
    }
}

static gboolean scheduler_tick(gpointer data) {

    (void)data;

    static char output[20000];

    scheduler_step(output);

    if (!scheduler_has_active_processes())
        print_scheduler_report(output);

    gtk_label_set_text(
        GTK_LABEL(scheduler_output_label),
        output
    );

    refresh_live_process_table();

    if (!scheduler_has_active_processes()) {

        scheduler_timer = 0;

        return FALSE;
    }

    return TRUE;
}

/* -------- SCHEDULER -------- */

void run_sched() {

    GtkWidget *box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(
        GTK_BOX(box),
        title("⚡ LIVE PROCESS SCHEDULER")
    );

    scheduler_output_label =
        gtk_label_new("Scheduler Starting...");

    gtk_widget_add_css_class(
        scheduler_output_label,
        "output-panel"
    );

    gtk_label_set_xalign(
        GTK_LABEL(scheduler_output_label),
        0.0
    );

    gtk_box_append(
        GTK_BOX(box),
        scheduler_output_label
    );

    live_process_grid = gtk_grid_new();

    gtk_grid_set_row_spacing(
        GTK_GRID(live_process_grid),
        8
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(live_process_grid),
        16
    );

    gtk_widget_add_css_class(
        live_process_grid,
        "live-table"
    );

    refresh_live_process_table();

    gtk_box_append(
        GTK_BOX(box),
        live_process_grid
    );

    GtkWidget *back =
        gtk_button_new_with_label(
            "⬅ Back to Main"
        );

    gtk_widget_add_css_class(back, "secondary");

    g_signal_connect(
        back,
        "clicked",
        G_CALLBACK(go_home),
        NULL
    );

    gtk_box_append(GTK_BOX(box), back);

    show(box);

    reset_scheduler_for_run();

    if (scheduler_timer != 0)
        g_source_remove(scheduler_timer);

    scheduler_timer =
        g_timeout_add(
            1000,
            scheduler_tick,
            NULL
        );
}

/* -------- IPC -------- */

void ipc_screen() {

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("📡 Inter-Process Communication"));

    const char *coordination_report =
        simulate_process_coordination(1, "MAJOR_EMERGENCY");

    GtkWidget *coordination_label = gtk_label_new(coordination_report);
    gtk_widget_add_css_class(coordination_label, "output-panel");

    gtk_label_set_wrap(GTK_LABEL(coordination_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(coordination_label), TRUE);

    gtk_box_append(GTK_BOX(box), coordination_label);

    GtkWidget *back = gtk_button_new_with_label("⬅ Back to Main");
    gtk_widget_add_css_class(back, "secondary");

    g_signal_connect(back, "clicked", G_CALLBACK(go_home), NULL);

    gtk_box_append(GTK_BOX(box), back);

    show(box);
}

/* -------- DEADLOCK -------- */

void deadlock_screen() {

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("🔒 Deadlock Detection System"));

    const char *status = detect_deadlock();

    GtkWidget *label = gtk_label_new(status);
    gtk_widget_add_css_class(label, "output-panel");

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);

    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *back = gtk_button_new_with_label("⬅ Back to Main");
    gtk_widget_add_css_class(back, "secondary");

    g_signal_connect(back, "clicked", G_CALLBACK(go_home), NULL);

    gtk_box_append(GTK_BOX(box), back);

    show(box);
}

/* -------- HOME -------- */

static void reset_system_ui(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;

    reset_system();
    home();
}

void home() {

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "main-container");

    gtk_box_append(GTK_BOX(box), title("🧠 SMART OS SIMULATOR"));

    GtkWidget *subtitle =
        gtk_label_new("Emergency Response Operating System");

    gtk_widget_add_css_class(subtitle, "subtitle");

    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *button_box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    gtk_widget_add_css_class(button_box, "button-grid");

    GtkWidget *c = gtk_button_new_with_label("🚨 Create Emergency Process");
    GtkWidget *s = gtk_button_new_with_label("⚡ Run Process Scheduler");
    GtkWidget *i = gtk_button_new_with_label("📡 Inter-Process Communication");
    GtkWidget *d = gtk_button_new_with_label("🔒 Deadlock Detection");
    GtkWidget *l = gtk_button_new_with_label("📄 View System Logs");

    /* ================= NEW BUTTONS ================= */
    GtkWidget *p = gtk_button_new_with_label("📋 View Processes");
    GtkWidget *r = gtk_button_new_with_label("🔄 Reset System");
    gtk_widget_add_css_class(p, "menu-button");
    gtk_widget_add_css_class(r, "menu-button");
    gtk_widget_add_css_class(l, "menu-button");

    gtk_widget_add_css_class(c, "menu-button");
    gtk_widget_add_css_class(s, "menu-button");
    gtk_widget_add_css_class(i, "menu-button");
    gtk_widget_add_css_class(d, "menu-button");

    g_signal_connect(c, "clicked", G_CALLBACK(create), NULL);
    g_signal_connect(s, "clicked", G_CALLBACK(run_sched), NULL);
    g_signal_connect(i, "clicked", G_CALLBACK(ipc_screen), NULL);
    g_signal_connect(d, "clicked", G_CALLBACK(deadlock_screen), NULL);
    g_signal_connect(l, "clicked", G_CALLBACK(log_screen), NULL);

    /* ================= NEW CONNECTS ================= */
    g_signal_connect(p, "clicked", G_CALLBACK(process_screen), NULL);
    g_signal_connect(r, "clicked", G_CALLBACK(reset_system_ui), NULL);

    gtk_box_append(GTK_BOX(button_box), c);
    gtk_box_append(GTK_BOX(button_box), s);
    gtk_box_append(GTK_BOX(button_box), i);
    gtk_box_append(GTK_BOX(button_box), d);
    gtk_box_append(GTK_BOX(button_box), l);
    gtk_box_append(GTK_BOX(button_box), p);
    gtk_box_append(GTK_BOX(button_box), r);

    gtk_box_append(GTK_BOX(box), button_box);

    show(box);
}

/* -------- START -------- */

static void on_app_shutdown(GApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;

    save_processes_to_file();
    logger_shutdown();
}

void activate(GtkApplication *app, gpointer data) {

    (void)data;

    window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window),
        "SMART OS - Emergency Response Simulator");

    gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);

    init_deadlock_detection();
    init_ipc_system();
    initialize_scheduler();
    initialize_memory_system();
    logger_init("system_log.txt");
    load_processes_from_file();

    g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), NULL);

    GtkWidget *scrolled_window = gtk_scrolled_window_new();

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(main_box, "main-container");

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scrolled_window),
        main_box
    );

    gtk_window_set_child(GTK_WINDOW(window), scrolled_window);

    GtkCssProvider *provider = gtk_css_provider_new();

    gtk_css_provider_load_from_path(provider, "style.css");

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    home();

    gtk_window_present(GTK_WINDOW(window));
}
