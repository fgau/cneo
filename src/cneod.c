/*
 * Copyright (C) 2013-2016
 * Authors (alphabetical) :
 *	F. Gau <fgau@pyneo.org>
 * License: GPLv3
 *
 * gcc cneod.c -o cneod `pkg-config --cflags dbus-1` `pkg-config --libs dbus-1`
 *
 * Example:
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod / org.cneo.System.GetDevice
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.SetBrightness int32:60
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.GetBrightness
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.Shutdown
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.Suspend
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.Reboot
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/Netbook org.cneo.System.SetAudioState string:asound

 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.SetBrightness int32:60
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.GetBrightness
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.Shutdown
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.Suspend
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.Reboot
 * dbus-send --system --print-reply --type=method_call --dest=org.cneo.cneod /org/cneo/GTA02 org.cneo.System.SetAudioState string:stereoout
*/

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

#define DBUS_NAME  "org.cneo.cneod"
#define DBUS_IFACE "org.cneo.System"
#define DBUS_ERROR "org.cneo.System.Error"

#define NETBOOK_BRIGHTNESS_FILE "/sys/class/backlight/intel_backlight/brightness"
#define NETBOOK_MAX_BRIGHTNESS_FILE "/sys/class/backlight/intel_backlight/max_brightness"
#define NETBOOK_ACT_BRIGHTNESS_FILE "/sys/class/backlight/intel_backlight/actual_brightness"

#define GTA02_BRIGHTNESS_FILE "/sys/class/backlight/pcf50633-backlight/brightness"
#define GTA02_MAX_BRIGHTNESS_FILE "/sys/class/backlight/pcf50633-backlight/max_brightness"
#define GTA02_ACT_BRIGHTNESS_FILE "/sys/class/backlight/pcf50633-backlight/actual_brightness"

#define SUSPEND_FILE "/sys/power/state"

#define NETBOOK_ALSA_PATH "/var/lib/alsa"
#define GTA02_ALSA_PATH "/usr/share/ecdial/data/alsastates/neo1973gta02"

#define BUFFER_SIZE 5

DBusConnection *conn;
DBusObjectPathVTable *dbus_vtable;
char DBUS_PATH[256];

void
reply_to_set_audio_state(DBusMessage* msg, DBusConnection* conn)
{
	DBusError error;
	DBusMessage* reply;
	DBusMessage* reply_err = NULL;
	DBusMessageIter args;
	const char *path;
	char state[256];

	dbus_error_init(&error);
	reply = dbus_message_new_method_return(msg);

	if (!dbus_message_iter_init(msg, &args)) {
		reply_err = dbus_message_new_error(msg, DBUS_ERROR,
							"No Arguments");

	} else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
		reply_err = dbus_message_new_error(msg, DBUS_ERROR,
							"Invalid Arguments");

	} else {
		dbus_message_iter_get_basic(&args, &path);

		if (strcmp(DBUS_PATH, "/org/cneo/Netbook") == 0) {
			snprintf(state, sizeof(state),
					"alsactl -f %s/%s.state restore",
					NETBOOK_ALSA_PATH, path);
		}

		else if (strcmp(DBUS_PATH, "/org/cneo/GTA02") == 0) {
			snprintf(state, sizeof(state),
					"alsactl -f %s/%s.state restore",
					GTA02_ALSA_PATH, path);
		}

		if (system(state) != 0) {
			reply_err = dbus_message_new_error(msg, DBUS_ERROR,
								"Failed");
		}

	}

	if (reply_err != NULL) {
		dbus_connection_send(conn, reply_err, NULL);
		dbus_message_unref(reply_err);
	}

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	dbus_error_free(&error);
}

int
get_sys_path_value(const char *file)
{
	int ret;
	char buffer[BUFFER_SIZE];
	FILE *f = fopen(file, "r");

	if (!f || !fgets(buffer, BUFFER_SIZE, f)){
		ret = -1;
		fprintf(stderr, "Could not read %s\n", file);
	} else {
		ret = atoi(buffer);
	}

	if (f) fclose(f);

	return ret;
}

void
reply_to_set_brightness(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;
	DBusMessageIter args;
	dbus_int32_t level = 0;
	char path[128];
	FILE* f;

	if (!dbus_message_iter_init(msg, &args))
		fprintf(stderr, "Message has no arguments!\n");
	else if (DBUS_TYPE_INT32 != dbus_message_iter_get_arg_type(&args))
		fprintf(stderr, "Argument is not int32!\n");
	else {
		dbus_message_iter_get_basic(&args, &level);

		reply = dbus_message_new_method_return(msg);

		dbus_message_iter_init_append(reply, &args);

		if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &level)) {
			fprintf(stderr, "Out Of Memory!\n");
			return;
		}

		if (strcmp(DBUS_PATH, "/org/cneo/Netbook") == 0) {
			f = fopen(NETBOOK_BRIGHTNESS_FILE, "w");

			if (!f) {
				fprintf(stderr, "Unable to open path for writing\n");
				return;
			}

			snprintf(path, sizeof(path), "%d\n",
				get_sys_path_value(NETBOOK_MAX_BRIGHTNESS_FILE) * level / 100);
		}

		else if (strcmp(DBUS_PATH, "/org/cneo/GTA02") == 0) {
			f = fopen(GTA02_BRIGHTNESS_FILE, "w");

			if (!f) {
				fprintf(stderr, "Unable to open path for writing\n");
				return;
			}

			snprintf(path, sizeof(path), "%d\n",
				get_sys_path_value(GTA02_MAX_BRIGHTNESS_FILE) * level / 100);
		}

		fprintf(f, path);
		fclose(f);

		if (!dbus_connection_send(conn, reply, NULL)) {
			fprintf(stderr, "Out Of Memory!\n");
			return;
		}

	}

	dbus_connection_flush(conn);
	dbus_message_unref(reply);
}

int
roundNo(float num)
{
	int k = num < 0 ? num / 10.0 - 0.5 : num / 10.0 + 0.5;
	return k * 10.0;
}

void
reply_to_get_brightness(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;
	DBusMessageIter args;
	dbus_int32_t level = 0;
//	FILE* f = fopen(NETBOOK_BRIGHTNESS_FILE, "w");
//	const char *path;

	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);

	if (strcmp(DBUS_PATH, "/org/cneo/Netbook") == 0) {
		level = roundNo(get_sys_path_value(NETBOOK_ACT_BRIGHTNESS_FILE) * 100.0 /
			get_sys_path_value(NETBOOK_MAX_BRIGHTNESS_FILE));
	}

	else if (strcmp(DBUS_PATH, "/org/cneo/GTA02") == 0) {
		level = roundNo(get_sys_path_value(GTA02_ACT_BRIGHTNESS_FILE) * 100.0 /
			get_sys_path_value(GTA02_MAX_BRIGHTNESS_FILE));
	}

	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &level)) {
		fprintf(stderr, "Out Of Memory!\n");
		return;
	}

	if (!dbus_connection_send(conn, reply, NULL)) {
		fprintf(stderr, "Out Of Memory!\n");
		return;
	}

	dbus_connection_flush(conn);
	dbus_message_unref(reply);
}

void
reply_to_shutdown(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;

	reply = dbus_message_new_method_return(msg);
	system("shutdown -h now");
	dbus_connection_send(conn, reply, NULL);
}

void
reply_to_suspend(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;
	char path[128];
	FILE* f = fopen(SUSPEND_FILE, "w");

	reply = dbus_message_new_method_return(msg);

	if (!f) {
		fprintf(stderr, "Unable to open path for writing\n");
		return;
	}

	snprintf(path, sizeof(path), "mem\n");

	fprintf(f, path);
	fclose(f);

	dbus_connection_send(conn, reply, NULL);
}

void
reply_to_reboot(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;

	reply = dbus_message_new_method_return(msg);
	system("shutdown -r now");
	dbus_connection_send(conn, reply, NULL);
}

void
reply_to_get_device(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;
	DBusMessageIter args;
	FILE* f = fopen(NETBOOK_BRIGHTNESS_FILE, "w");
	const char *path;

	if (!f)
		path = "/org/cneo/GTA02";
	else
		path = "/org/cneo/Netbook";

	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);

	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &path)) {
		fprintf(stderr, "Out Of Memory!\n");
		return;
	}

	if (!dbus_connection_send(conn, reply, NULL)) {
		fprintf(stderr, "Out Of Memory!\n");
		return;
	}

	dbus_connection_flush(conn);
	dbus_message_unref(reply);
}

static DBusHandlerResult
dbus_incoming_message(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	DBusMessage *reply;

	if (strcmp(DBUS_IFACE, dbus_message_get_interface(msg)) == 0) {
		DBusError error;

		dbus_error_init(&error);

		if (strcmp("SetAudioState", dbus_message_get_member(msg)) == 0)
			reply_to_set_audio_state(msg, conn);

		if (strcmp("SetBrightness", dbus_message_get_member(msg)) == 0)
			reply_to_set_brightness(msg, conn);

		if (strcmp("GetBrightness", dbus_message_get_member(msg)) == 0)
			reply_to_get_brightness(msg, conn);

		if (strcmp("Shutdown", dbus_message_get_member(msg)) == 0)
			reply_to_shutdown(msg, conn);

		if (strcmp("Suspend", dbus_message_get_member(msg)) == 0)
			reply_to_suspend(msg, conn);

		if (strcmp("Reboot", dbus_message_get_member(msg)) == 0)
			reply_to_reboot(msg, conn);

		if (strcmp("GetDevice", dbus_message_get_member(msg)) == 0)
			reply_to_get_device(msg, conn);

		else {
			reply = dbus_message_new_error(msg,
					"org.cneo.System.UnknownMethod",
					"Unknown method");

			dbus_connection_send(conn, reply, NULL);
			dbus_message_unref(reply);
		}

	} else {
		fprintf(stderr, "Ignoring message with %s.%s\n",
				dbus_message_get_interface(msg),
				dbus_message_get_member(msg));

		reply = dbus_message_new_error(msg,
					"org.cneo.System.UnknownMethod",
					"Unknown method");

		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

int
init_dbus()
{
	const char *path;
	FILE* f = fopen(NETBOOK_BRIGHTNESS_FILE, "w");

	if (!f)
		path = "/org/cneo/GTA02";
	else
		path = "/org/cneo/Netbook";

	snprintf(DBUS_PATH, sizeof(DBUS_PATH), "%s", path);

	DBusError error;
	dbus_error_init(&error);

	conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error);

	if (dbus_error_is_set(&error)) {
		fprintf(stderr, "Couldn't initialize DBus: %s\n", error.message);

		return -1;
	}

	switch (dbus_bus_request_name(conn, DBUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error)) {
		case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
			printf("Registered on DBus as %s\n", DBUS_NAME);
			break;
		default:
			if (dbus_error_is_set(&error)) {
				fprintf(stderr, "Couldn't get DBus name: %s\n", error.message);
			} else {
				fprintf(stderr, "Couldn't get DBus name\n");
			}
			return -1;
			break;
	}

	dbus_vtable = malloc(sizeof(DBusObjectPathVTable));
	dbus_vtable->unregister_function = NULL;
	dbus_vtable->message_function = dbus_incoming_message;

	if ((!dbus_connection_register_object_path(conn, DBUS_PATH, dbus_vtable, NULL)) ||
		(!dbus_connection_register_object_path(conn, "/", dbus_vtable, NULL))) {
		fprintf(stderr, "Couldn't register object path\n");
		return -1;
	}

	return 0;
}

void
shutdown_dbus()
{
	if (conn)
		dbus_connection_close(conn);
}

void
signal_handler(int signo)
{
	printf("Received signal %d\n", signo);

	switch(signo) {
		case SIGTERM:
		case SIGINT:
			shutdown_dbus();
			conn = NULL;
			break;
		case SIGPIPE:
			exit(1);
		default:
			break;
	}
}

void
init_signals()
{
	struct sigaction act;
	sigset_t empty_mask;

	sigemptyset(&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;

	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
}

int
main(int argc, char **argv)
{
	init_signals();

	if (init_dbus() < 0) {
		fprintf(stderr, "Cannot initialize DBus\n");
		return 1;
	}

	while (dbus_connection_read_write(conn, -1)) {
		while (dbus_connection_dispatch(conn) != DBUS_DISPATCH_COMPLETE) {
		}
	}

	shutdown_dbus();

	return 0;
}
