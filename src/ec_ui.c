/*
    ettercap -- user interface stuff

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    $Header: /home/drizzt/dev/sources/ettercap.cvs/ettercap_ng/src/ec_ui.c,v 1.2 2003/03/10 09:08:13 alor Exp $
*/

#include <ec.h>
#include <ec_ui.h>

#include <stdarg.h>
#include <pthread.h>

/* globals */

struct ui_message {
   char *message;
   SIMPLEQ_ENTRY(ui_message) next;
};

SIMPLEQ_HEAD(, ui_message) messages_queue = SIMPLEQ_HEAD_INITIALIZER(messages_queue);

/* default hook */

static struct ui_ops ui = {
   .ui_init = NULL,
   .ui_start = NULL,
   .ui_cleanup = NULL,
   .ui_msg = (void (*)(const char *))&printf,
   .initialized = 0,
};

/* globa mutex on interface */

static pthread_mutex_t ui_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
#define UI_MSG_LOCK     pthread_mutex_lock(&ui_msg_mutex)
#define UI_MSG_UNLOCK   pthread_mutex_unlock(&ui_msg_mutex)

/* protos... */

void ui_init(void);
void ui_start(void);
void ui_cleanup(void);
void ui_msg(const char *fmt, ...);
int ui_msg_flush(int max);
void ui_register(struct ui_ops *ops);


/*******************************************/

/* called to initialize the user interface */

void ui_init(void)
{
   DEBUG_MSG("ui_init");

   EXECUTE(ui.ui_init);

   ui.initialized = 1;
}

/* called to run the user interface */

void ui_start(void)
{
   DEBUG_MSG("ui_start");

   EXECUTE(ui.ui_start);
}

/* called to end the user interface */

void ui_cleanup(void)
{
   if (ui.initialized) {
      DEBUG_MSG("ui_cleanup");
      EXECUTE(ui.ui_cleanup);
      ui.initialized = 0;
   }
}


/*
 * this fuction enqueues the messages displayed by
 * ui_msg_flush()
 */

void ui_msg(const char *fmt, ...)
{
   va_list ap;
   struct ui_message *msg;
   int n, size = 50;


   msg = (struct ui_message *) calloc(1, sizeof(struct ui_message));
   ON_ERROR(msg, NULL, "can't allocate ui_message");

   /* 
    * we hope the message is shorter
    * than 'size', else realloc it
    */
    
   msg->message = calloc(size, sizeof(char));
   ON_ERROR(msg->message, NULL, "can't allocate memory");

   while (1) {
      /* Try to print in the allocated space. */
      va_start(ap, fmt);
      n = vsnprintf (msg->message, size, fmt, ap);
      va_end(ap);
      
      /* If that worked, we have finished. */
      if (n > -1 && n < size)
         break;
   
      /* Else try again with more space. */
      if (n > -1)    /* glibc 2.1 */
         size = n+1; /* precisely what is needed */
      else           /* glibc 2.0 */
         size *= 2;  /* twice the old size */
      
      msg->message = realloc (msg->message, size);
      ON_ERROR(msg->message, NULL, "can't allocate memory");
   }

   /* 
    * MUST use the mutex.
    * this MAY be a different thread !!
    */
   
   UI_MSG_LOCK;
   
   /* add the message to the queue */
   SIMPLEQ_INSERT_TAIL(&messages_queue, msg, next);

   UI_MSG_UNLOCK;
   
}

/* 
 * this function is used to display up to 'max' messages.
 * a user interface MUST use this to empty the message queue.
 */

int ui_msg_flush(int max)
{
   int i = 0;
   struct ui_message *msg;

   /* no messages, no actions */
   if (SIMPLEQ_FIRST(&messages_queue) == NULL)
      return 0;
   
   /* the queue is updated by other threads */
   UI_MSG_LOCK;
      
   while ( (msg = SIMPLEQ_FIRST(&messages_queue)) != NULL) {

      /* diplay the message */
      ui.ui_msg(msg->message);

      /* free the message */
      SAFE_FREE(msg->message);
      
      SIMPLEQ_REMOVE_HEAD(&messages_queue, msg, next);
      SAFE_FREE(msg);
      
      /* do not display more then 'max' messages */
      if (++i == max)
         break;
   }
   
   UI_MSG_UNLOCK;
   
   /* returns the number of displayed messages */
   return i;
   
}


/*
 * register the function pointer for
 * the user interface.
 * a new user interface MUST implement this 
 * three function and use this function 
 * to hook in the right place.
 */

void ui_register(struct ui_ops *ops)
{
        
   ON_ERROR(ops->ui_init, NULL, "BUG: ui_init is equal to NULL");
   ui.ui_init = ops->ui_init;
   
   ON_ERROR(ops->ui_cleanup, NULL, "BUG: ui_cleanup is equal to NULL");
   ui.ui_cleanup = ops->ui_cleanup;
   
   ON_ERROR(ops->ui_start, NULL, "BUG: ui_start is equal to NULL");
   ui.ui_start = ops->ui_start;
        
   ON_ERROR(ops->ui_msg, NULL, "BUG: ui_msg is equal to NULL");
   ui.ui_msg = ops->ui_msg;
   
}


/* EOF */

// vim:ts=3:expandtab

