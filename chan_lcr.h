/*****************************************************************************\
**                                                                           **
** Linux Call Router                                                         **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** Asterisk socket client header                                             **
**                                                                           **
\*****************************************************************************/

/* structure for all calls */
struct chan_call {
	struct chan_call	*next;	/* link to next call instance */
	int			state;	/* current call state CHAN_LCR_STATE */
	unsigned long		ref;	/* callref for this channel */
	struct ast_channel	*ast;	/* current asterisk channel */
	unsigned long		bchannel_handle;
					/* reference to bchannel, if set */
	unsigned short bridge_id; /* 0 = off, equal ids are bridged */

	struct ast_channel *ast;
};

enum {
	CHAN_LCR_STATE_IN_PREPARE = 0,
	CHAN_LCR_STATE_IN_SETUP,
	CHAN_LCR_STATE_IN_DIALING,
	CHAN_LCR_STATE_IN_PROCEEDING,
	CHAN_LCR_STATE_IN_ALERTING,
	CHAN_LCR_STATE_OUT_PREPARE,
	CHAN_LCR_STATE_OUT_SETUP,
	CHAN_LCR_STATE_OUT_DIALING,
	CHAN_LCR_STATE_OUT_PROCEEDING,
	CHAN_LCR_STATE_OUT_ALERTING,
	CHAN_LCR_STATE_CONNECT,
	CHAN_LCR_STATE_IN_DISCONNECT,
	CHAN_LCR_STATE_OUT_DISCONNECT,
	CHAN_LCR_STATE_RELEASE,
};

#define CHAN_LCR_STATE static const struct chan_lcr_state { \
	char name, \
	char meaning, \
} chan_lcr_state[] = { \
	{ "IN_PREPARE", \
	  "New call from ISDN is waiting for setup." }, \
	{ "IN_SETUP", \
	  "Call from ISDN is currently set up." }, \
	{ "IN_DIALING", \
	  "Call from ISDN is currently waiting for digits to be dialed." }, \
	{ "IN_PROCEEDING", \
	  "Call from ISDN is complete and proceeds to ring." }, \
	{ "IN_ALERTING", \
	  "Call from ISDN is ringing." }, \
	{ "OUT_PREPARE", \
	  "New call to ISDN is wating for setup." }, \
	{ "OUT_SETUP", \
	  "Call to ISDN is currently set up." }, \
	{ "OUT_DIALING", \
	  "Call to ISDN is currently waiting for digits to be dialed." }, \
	{ "OUT_PROCEEDING", \
	  "Call to ISDN is complete and proceeds to ring." }, \
	{ "OUT_ALERTING", \
	  "Call to ISDN is ringing." }, \
	{ "CONNECT", \
	  "Call has been answered." }, \
	{ "IN_DISCONNECT", \
	  "Call has been hung up on ISDN side." }, \
	{ "OUT_DISCONNECT", \
	  "Call has been hung up on Asterisk side." }, \
	{ "RELEASE", \
	  "Call is waiting for complete release." }, \
};

