/*
** Copyright (C) 2011 Dirk-Jan C. Binnema <djcb@djcbsoftware.nl>
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 3, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation,
** Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
*/

#include <mu-msg.h>
#include <mu-query.h>
#include <mu-runtime.h>

#include "mu-guile-msg.h"
#include "mu-guile-common.h"

struct _MuMsgWrapper {
	MuMsg   *_msg;
	gboolean _unrefme;
};
typedef struct _MuMsgWrapper MuMsgWrapper;

static long MSG_TAG;

static int
mu_guile_scm_is_msg (SCM scm)
{
	return SCM_NIMP(scm) && (long) SCM_CAR (scm) == MSG_TAG;
}


SCM
mu_guile_msg_to_scm (MuMsg *msg)
{
	MuMsgWrapper *msgwrap;

	g_return_val_if_fail (msg, SCM_UNDEFINED);
	
	msgwrap = scm_gc_malloc (sizeof (MuMsgWrapper), "msg");
	msgwrap->_msg = msg;
	msgwrap->_unrefme = FALSE;
	
	SCM_RETURN_NEWSMOB (MSG_TAG, msgwrap);
}

SCM_DEFINE (msg_make_from_file, "mu:msg:make-from-file", 1, 0, 0,
	    (SCM PATH),
	    "Create a message object based on the message in PATH.\n")
#define FUNC_NAME s_msg_make_from_file
{
	MuMsg *msg;
	GError *err;

	SCM_ASSERT (scm_is_string (PATH), PATH, SCM_ARG1, FUNC_NAME);
	
	err = NULL;
	msg = mu_msg_new_from_file (scm_to_utf8_string (PATH), NULL, &err);
	
	if (err) {
		mu_guile_g_error (FUNC_NAME, err);
		g_error_free (err);
	}
	
	return msg ? mu_guile_msg_to_scm (msg) : SCM_UNDEFINED;
}
#undef FUNC_NAME


static SCM
msg_str_field (SCM msg_smob, MuMsgFieldId mfid)
{
	const char *val, *endptr;
	
	MuMsgWrapper *msgwrap;
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);

	val = mu_msg_get_field_string(msgwrap->_msg, mfid);

	if (val && !g_utf8_validate (val, -1, &endptr)) {
		//return scm_from_utf8_string("<invalid>");
		gchar *part;
		SCM scm;
		part = g_strndup (val, (endptr-val));
		scm = scm_from_utf8_string(part);
		g_free (part);
		return scm;
	} else
		return val ? scm_from_utf8_string(val) : SCM_UNSPECIFIED;
}

static gint64
msg_num_field (SCM msg_smob, MuMsgFieldId mfid)
{
	MuMsgWrapper *msgwrap;
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);

	return mu_msg_get_field_numeric(msgwrap->_msg, mfid);
}


SCM_DEFINE (msg_date, "mu:msg:date", 1, 0, 0,
	    (SCM MSG),
	    "Get the date (time in seconds since epoch) for MSG.\n")
#define FUNC_NAME s_msg_date
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return scm_from_unsigned_integer
		(msg_num_field (MSG, MU_MSG_FIELD_ID_DATE));	
}
#undef FUNC_NAME



SCM_DEFINE (msg_size, "mu:msg:size", 1, 0, 0,
	    (SCM MSG),
	    "Get the size in bytes for MSG.\n")
#define FUNC_NAME s_msg_size
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return scm_from_unsigned_integer
		(msg_num_field (MSG, MU_MSG_FIELD_ID_SIZE));	
}
#undef FUNC_NAME



SCM_DEFINE (msg_prio, "mu:msg:priority", 1, 0, 0,
	    (SCM MSG),
	    "Get the priority of MSG (low, normal or high).\n")
#define FUNC_NAME s_msg_prio
{
	MuMsgPrio prio;
	MuMsgWrapper *msgwrap;

	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);

	msgwrap = (MuMsgWrapper*) SCM_CDR(MSG);
	
	prio = mu_msg_get_prio (msgwrap->_msg);

	switch (prio) {
	case MU_MSG_PRIO_LOW:    return scm_from_utf8_symbol("low");
	case MU_MSG_PRIO_NORMAL: return scm_from_utf8_symbol("normal");
	case MU_MSG_PRIO_HIGH:   return scm_from_utf8_symbol("high");
	default:
		g_return_val_if_reached (SCM_UNDEFINED);
	}	
}
#undef FUNC_NAME

struct _FlagData {
	MuMsgFlags flags;
	SCM lst;
};
typedef struct _FlagData FlagData;


static void
check_flag (MuMsgFlags flag, FlagData *fdata)
{
	if (fdata->flags & flag) {
		SCM item;
		item = scm_list_1 (scm_from_utf8_symbol(mu_msg_flag_name(flag)));
		fdata->lst = scm_append_x (scm_list_2(fdata->lst, item));
	}	
}


SCM_DEFINE (msg_flags, "mu:msg:flags", 1, 0, 0,
	    (SCM MSG),
	    "Get the flags for MSG (one or or more of new, passed, replied, "
	    "seen, trashed, draft, flagged, unread, signed, encrypted, "
	    "has-attach).\n")
#define FUNC_NAME s_msg_flags
{
	MuMsgWrapper *msgwrap;
	FlagData fdata;
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	msgwrap = (MuMsgWrapper*) SCM_CDR(MSG);
	
	fdata.flags = mu_msg_get_flags (msgwrap->_msg);
	fdata.lst = SCM_EOL;
	mu_msg_flags_foreach ((MuMsgFlagsForeachFunc)check_flag,
			      &fdata);

	return fdata.lst;
}
#undef FUNC_NAME


SCM_DEFINE (msg_subject, "mu:msg:subject", 1, 0, 0,
	    (SCM MSG), "Get the subject of MSG.\n")
#define FUNC_NAME s_msg_subject
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);

	return msg_str_field (MSG, MU_MSG_FIELD_ID_SUBJECT);
}
#undef FUNC_NAME


SCM_DEFINE (msg_from, "mu:msg:from", 1, 0, 0,
	    (SCM MSG), "Get the sender of MSG.\n")
#define FUNC_NAME s_msg_from
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return msg_str_field (MSG, MU_MSG_FIELD_ID_FROM);
}
#undef FUNC_NAME

struct _EachContactData {
	SCM lst;
	MuMsgContactType ctype;
};
typedef struct _EachContactData EachContactData;

static void
contacts_to_list (MuMsgContact *contact, EachContactData *ecdata)
{
	if (mu_msg_contact_type (contact) == ecdata->ctype) {
		SCM item;
		const char *addr, *name;

		addr = mu_msg_contact_address(contact);
		name = mu_msg_contact_name(contact);
		
		item = scm_list_1
			(scm_list_2 (
				name ? scm_from_utf8_string(name) : SCM_UNSPECIFIED,
				addr ? scm_from_utf8_string(addr) : SCM_UNSPECIFIED));

		ecdata->lst = scm_append_x (scm_list_2(ecdata->lst, item));	
	}
}
	

static SCM
contact_list_field (SCM msg_smob, MuMsgFieldId mfid)
{
	MuMsgWrapper *msgwrap;
	EachContactData ecdata;

	ecdata.lst = SCM_EOL;
	
	switch (mfid) {
	case MU_MSG_FIELD_ID_TO: ecdata.ctype = MU_MSG_CONTACT_TYPE_TO; break;
	case MU_MSG_FIELD_ID_CC: ecdata.ctype = MU_MSG_CONTACT_TYPE_CC; break;
	case MU_MSG_FIELD_ID_BCC: ecdata.ctype = MU_MSG_CONTACT_TYPE_BCC; break;
	default: g_return_val_if_reached (SCM_UNDEFINED);
	}
	
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);
	
	mu_msg_contact_foreach (msgwrap->_msg,
				(MuMsgContactForeachFunc)contacts_to_list,
				&ecdata);
	return ecdata.lst;
}


SCM_DEFINE (msg_to, "mu:msg:to", 1, 0, 0,
	    (SCM MSG), "Get the list of To:-recipients of MSG.\n")
#define FUNC_NAME s_msg_to
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return contact_list_field (MSG, MU_MSG_FIELD_ID_TO);
}
#undef FUNC_NAME

	    

SCM_DEFINE (msg_cc, "mu:msg:cc", 1, 0, 0,
	    (SCM MSG), "Get the list of Cc:-recipients of MSG.\n")
#define FUNC_NAME s_msg_cc
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);

	return contact_list_field (MSG, MU_MSG_FIELD_ID_CC);
}
#undef FUNC_NAME


SCM_DEFINE (msg_bcc, "mu:msg:bcc", 1, 0, 0,
	    (SCM MSG), "Get the list of Bcc:-recipients of MSG.\n")
#define FUNC_NAME s_msg_bcc
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return contact_list_field (MSG, MU_MSG_FIELD_ID_BCC);
}
#undef FUNC_NAME


SCM_DEFINE (msg_path, "mu:msg:path", 1, 0, 0,
	    (SCM MSG), "Get the filesystem path for MSG.\n")
#define FUNC_NAME s_msg_path
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return msg_str_field (MSG, MU_MSG_FIELD_ID_PATH);
}
#undef FUNC_NAME


SCM_DEFINE (msg_maildir, "mu:msg:maildir", 1, 0, 0,
	    (SCM MSG), "Get the maildir where MSG lives.\n")
#define FUNC_NAME s_msg_maildir
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);

	return msg_str_field (MSG, MU_MSG_FIELD_ID_MAILDIR);
}
#undef FUNC_NAME



SCM_DEFINE (msg_msgid, "mu:msg:message-id", 1, 0, 0,
	    (SCM MSG), "Get the MSG's message-id.\n")
#define FUNC_NAME s_msg_msgid
{
	return msg_str_field (MSG, MU_MSG_FIELD_ID_MSGID);
}
#undef FUNC_NAME


SCM_DEFINE (msg_body, "mu:msg:body", 1, 1, 0,
		    (SCM MSG, SCM HTML), "Get the MSG's body. If HTML is #t, "
		    "prefer the html-version, otherwise prefer plain text.\n")
#define FUNC_NAME s_msg_body
{
	MuMsgWrapper *msgwrap;
	gboolean html;
	const char *val;

	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	msgwrap = (MuMsgWrapper*) SCM_CDR(MSG);
	html = SCM_UNBNDP(HTML) ? FALSE : HTML == SCM_BOOL_T;
	
	if (html)
		val = mu_msg_get_body_html(msgwrap->_msg);
	else
		val = mu_msg_get_body_text(msgwrap->_msg);
		
	return val ? scm_from_utf8_string (val) : SCM_UNSPECIFIED;
}
#undef FUNC_NAME


SCM_DEFINE (msg_header, "mu:msg:header", 1, 1, 0,
		    (SCM MSG, SCM HEADER), "Get an arbitary HEADER from MSG.\n")
#define FUNC_NAME s_msg_header
{
	MuMsgWrapper *msgwrap;
	const char *header;
	const char *val;

	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	SCM_ASSERT (scm_is_string (HEADER), HEADER, SCM_ARG2, FUNC_NAME);
	
	msgwrap = (MuMsgWrapper*) SCM_CDR(MSG);
	header  =  scm_to_utf8_string (HEADER);
	val     =  mu_msg_get_header(msgwrap->_msg, header);
	
	return val ? scm_from_utf8_string(val) : SCM_UNDEFINED;
}
#undef FUNC_NAME

static SCM
msg_string_list_field (SCM msg_smob, MuMsgFieldId mfid)
{
	MuMsgWrapper *msgwrap;
	SCM scmlst;
	const GSList *lst;
	
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);
	lst = mu_msg_get_field_string_list (msgwrap->_msg, mfid);
	
	for (scmlst = SCM_EOL; lst;
	     lst = g_slist_next(lst)) {	
		SCM item;
		item = scm_list_1 (scm_from_utf8_string((const char*)lst->data));
		scmlst = scm_append_x (scm_list_2(scmlst, item));
	}

	return scmlst;
}


SCM_DEFINE (msg_tags, "mu:msg:tags", 1, 1, 0,
		    (SCM MSG), "Get the list of tags (contents of the "
		    "X-Label:-header) for MSG.\n")
#define FUNC_NAME s_msg_tags
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return msg_string_list_field (MSG, MU_MSG_FIELD_ID_TAGS);
}
#undef FUNC_NAME



SCM_DEFINE (msg_refs, "mu:msg:references", 1, 1, 0,
	    (SCM MSG), "Get the list of referenced message-ids "
	    "(contents of the References: and Reply-To: headers).\n")
#define FUNC_NAME s_msg_refs
{
	SCM_ASSERT (mu_guile_scm_is_msg(MSG), MSG, SCM_ARG1, FUNC_NAME);
	
	return msg_string_list_field (MSG, MU_MSG_FIELD_ID_REFS);
}
#undef FUNC_NAME

	    
static SCM
msg_mark (SCM msg_smob)
{
	MuMsgWrapper *msgwrap;
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);

	msgwrap->_unrefme = TRUE;

	return SCM_UNSPECIFIED;
}

static scm_sizet
msg_free (SCM msg_smob)
{
	MuMsgWrapper *msgwrap;	
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);
	
	if (msgwrap->_unrefme)
		mu_msg_unref (msgwrap->_msg);

	return sizeof (MuMsgWrapper);
}

static int
msg_print (SCM msg_smob, SCM port, scm_print_state * pstate)
{
	MuMsgWrapper *msgwrap;	
	msgwrap = (MuMsgWrapper*) SCM_CDR(msg_smob);

	scm_puts ("#<msg ", port);

	if (msg_smob == SCM_BOOL_F)
		scm_puts ("#f", port);
	else 
		scm_puts (mu_msg_get_path(msgwrap->_msg),
			  port);

	scm_puts (">", port);

	return 1;
}


static void
define_symbols (void)
{
	/* message priority */
	scm_c_define ("high",		scm_from_int(MU_MSG_PRIO_HIGH));
	scm_c_define ("low",		scm_from_int(MU_MSG_PRIO_LOW));
	scm_c_define ("normal",		scm_from_int(MU_MSG_PRIO_NORMAL));

	/* message flags */
	scm_c_define ("new",		scm_from_int(MU_MSG_FLAG_NEW));
	scm_c_define ("passed",		scm_from_int(MU_MSG_FLAG_PASSED));
	scm_c_define ("replied",	scm_from_int(MU_MSG_FLAG_REPLIED));
	scm_c_define ("seen",		scm_from_int(MU_MSG_FLAG_SEEN));
	scm_c_define ("trashed",	scm_from_int(MU_MSG_FLAG_TRASHED));
	scm_c_define ("draft",		scm_from_int(MU_MSG_FLAG_DRAFT));
	scm_c_define ("flagged",	scm_from_int(MU_MSG_FLAG_FLAGGED));
	scm_c_define ("unread",		scm_from_int(MU_MSG_FLAG_UNREAD));
	scm_c_define ("signed",		scm_from_int(MU_MSG_FLAG_SIGNED));
	scm_c_define ("encrypted",	scm_from_int(MU_MSG_FLAG_ENCRYPTED));
	scm_c_define ("has-attach",	scm_from_int(MU_MSG_FLAG_HAS_ATTACH));
}


void*
mu_guile_msg_init (void *data)
{
	MSG_TAG = scm_make_smob_type ("msg", sizeof(MuMsgWrapper));
		
	scm_set_smob_mark  (MSG_TAG, msg_mark);
	scm_set_smob_free  (MSG_TAG, msg_free);
	scm_set_smob_print (MSG_TAG, msg_print);	

	define_symbols ();
	
#include "mu-guile-msg.x"

	return NULL;
}

