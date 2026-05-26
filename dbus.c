#include <dbus/dbus.h>
#include "dat.h"
#include "fn.h"

static DBusConnection *conn;
static u32int notifyid = 1;

static u32int
sendnoti(DBusMessage *msg)
{
	DBusMessageIter args;
	Noti n;
	char *summary, *body;
	u32int replaces;

	memset(&n, 0, sizeof n);
	summary = "";
	body = "";
	replaces = 0;

	if(dbus_message_iter_init(msg, &args)){
		dbus_message_iter_next(&args);
		if(dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32)
			dbus_message_iter_get_basic(&args, &replaces);
		dbus_message_iter_next(&args);
		dbus_message_iter_next(&args);
		if(dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
			dbus_message_iter_get_basic(&args, &summary);
		dbus_message_iter_next(&args);
		if(dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
			dbus_message_iter_get_basic(&args, &body);
	}

	if(replaces != 0){
		n.id = replaces;
		if(replaces >= notifyid)
			notifyid = replaces + 1;
	} else {
		n.id = notifyid++;
	}
	sinit(&n.summary, summary, strlen(summary));
	sinit(&n.body, body, strlen(body));
	send(notic, &n);
	return n.id;
}

static void
emitclosed(u32int id, u32int reason)
{
	DBusMessage *sig;

	sig = dbus_message_new_signal("/org/freedesktop/Notifications",
		"org.freedesktop.Notifications", "NotificationClosed");
	if(sig == nil)
		return;
	dbus_message_append_args(sig,
		DBUS_TYPE_UINT32, &id,
		DBUS_TYPE_UINT32, &reason,
		DBUS_TYPE_INVALID);
	dbus_connection_send(conn, sig, nil);
	dbus_message_unref(sig);
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
		id = sendnoti(msg);
		reply = dbus_message_new_method_return(msg);
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
		id = 0;
		if(dbus_message_get_args(msg, nil, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID) && id != 0)
			nbsend(closereqc, &id);
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
	CloseEv ev;
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
	while(dbus_connection_read_write_dispatch(conn, 100)){
		while(nbrecv(closec, &ev) > 0)
			emitclosed(ev.id, ev.reason);
	}
	die("dbus: connection lost");
}
