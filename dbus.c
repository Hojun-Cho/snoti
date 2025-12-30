#include <dbus/dbus.h>
#include "dat.h"
#include "fn.h"

static DBusConnection *conn;
static u32int notifyid = 1;

static void
sendnoti(DBusMessage *msg)
{
	DBusMessageIter args;
	Noti n;
	char *summary, *body;

	if(!dbus_message_iter_init(msg, &args))
		return;
	dbus_message_iter_next(&args);
	dbus_message_iter_next(&args);
	dbus_message_iter_next(&args);
	if(dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
		return;
	dbus_message_iter_get_basic(&args, &summary);
	dbus_message_iter_next(&args);
	if(dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
		return;
	dbus_message_iter_get_basic(&args, &body);
	notifyid++;
	sinit(&n.summary, summary, strlen(summary));
	sinit(&n.body, body, strlen(body));
	send(notic, &n);
}

static DBusHandlerResult
filter(DBusConnection *c, DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	DBusMessageIter iter, arr;
	u32int id;
	char *s;

	USED(c);
	USED(data);
	if(dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")){
		sendnoti(msg);
		reply = dbus_message_new_method_return(msg);
		id = notifyid - 1;
		dbus_message_append_args(reply, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
		dbus_connection_send(conn, reply, nil);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if(dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetCapabilities")){
		reply = dbus_message_new_method_return(msg);
		dbus_message_iter_init_append(reply, &iter);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &arr);
		s = "body"; dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &s);
		s = "body-markup"; dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &s);
		s = "icon-static"; dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &s);
		s = "actions"; dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &s);
		s = "persistence"; dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &s);
		dbus_message_iter_close_container(&iter, &arr);
		dbus_connection_send(conn, reply, nil);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if(dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetServerInformation")){
		reply = dbus_message_new_method_return(msg);
		s = "snoti"; dbus_message_append_args(reply, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
		s = "plan9"; dbus_message_append_args(reply, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
		s = "1.0"; dbus_message_append_args(reply, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
		s = "1.2"; dbus_message_append_args(reply, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
		dbus_connection_send(conn, reply, nil);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if(dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "CloseNotification")){
		reply = dbus_message_new_method_return(msg);
		dbus_connection_send(conn, reply, nil);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
dbusthread(void*)
{
	DBusError err;
	int ret;

	dbus_error_init(&err);
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if(dbus_error_is_set(&err))
		die("dbus: %s", err.message);
	ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications",
		DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if(ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		die("dbus: can't get name");
	dbus_connection_add_filter(conn, filter, nil, nil);
	while(dbus_connection_read_write_dispatch(conn, 100))
		;
	die("dbus: connection lost");
}
