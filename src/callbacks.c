#include <time.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <tox/tox.h>

#include "misc.h"
#include "fileop.h"
#include "filesend.h"

static int group_chat_number = -1;
static Tox *group_messenger;

/* START COMMANDS */
#define MAX_ARGS_SIZE 20
#define MAX_ARGS_NUM 3
#define	SYNTAX_ERR_MSG "Invalid syntax!"
#define NOTFND_ERR_MSG "Pack number not found"
static void cmd_invite(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
	if(group_chat_number != -1)
	{
		int rc = tox_invite_friend(m, friendnum, group_chat_number);
		if(rc == -1)
			yerr("Failed to invite friend n°%i to groupchat n°%i", friendnum, group_chat_number);
	}		
}

static void cmd_help(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
	char helpstr[] = "No helping!";
	tox_send_message(m, friendnum, (uint8_t *) helpstr, strlen(helpstr) + 1);
}

static void cmd_info(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
	const char formatstr[] = "\n Pack Info for Pack #%i:\n\
 Filename       %s\n\
 Filesize       %lu [%s]\n\
 Last Modified  %s GMT\n\
 sha256sum      %s\n\
 md5sum         %s\n\
 crc32          %.2X";
	
	/* formatstring + packn + filename + (size + numan_size) + gmtime + sha256sum + md5sum + crc32 */
	char buf[sizeof(formatstr) + 20 + PATH_MAX + 28 + 26 + 64 + 32 + 8];
	char hu_size[8];
	FileNode *fn;
	FileNode **fnode = file_get_shared();
	int fnode_num = file_get_shared_len();
	char *sha256, *md5, *timestr;
	int rc;
	
	if(argc != 1)
	{
		tox_send_message(m, friendnum, (uint8_t *) SYNTAX_ERR_MSG, strlen(SYNTAX_ERR_MSG) + 1);
		return;
	}

	int packn = atoi(argv[1]);
	if((packn == 0 && strcmp(argv[1], "0") != 0) || packn < 0 || packn > fnode_num)
	{
		tox_send_message(m, friendnum, (uint8_t *) NOTFND_ERR_MSG, strlen(NOTFND_ERR_MSG) + 1);
		return;
	}
	
	fn = fnode[packn];
	sha256 = human_readable_id(fn->info->sha256, 32);
	md5 = human_readable_id(fn->info->md5, 16);
	human_readable_filesize(hu_size, fn->size);
	timestr = asctime(gmtime(&fn->mtime));
	
	rc = snprintf(buf, sizeof(buf), formatstr, packn, gnu_basename(fn->file), fn->size,
			hu_size, timestr, sha256, md5, fn->info->crc32);
	
	if(rc < 0 || tox_send_message(m, friendnum, (uint8_t *) buf, rc + 1) == -1)
	{
		yerr("Failed to send info msg to friend n°%i", friendnum);
	}

	free(md5);
	free(sha256);
}

static void cmd_send(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
	FileNode **fnode = file_get_shared();
	int fnode_num = file_get_shared_len();
	int rc;
	
	if(argc != 1)
	{
		tox_send_message(m, friendnum, (uint8_t *) SYNTAX_ERR_MSG, strlen(SYNTAX_ERR_MSG) + 1);
		return;
	}

	int packn = atoi(argv[1]);
	if((packn == 0 && strcmp(argv[1], "0") != 0) || packn < -1 || packn > fnode_num)
	{
		tox_send_message(m, friendnum, (uint8_t *) NOTFND_ERR_MSG, strlen(NOTFND_ERR_MSG) + 1);
		return;
	}
	
	if(packn == -1)
	{
		//TODO: send list
		yerr("Uninplemented");
		return;
	}
	
	rc = file_sender_new(friendnum, fnode[packn], m);
	switch(rc)
	{
		case FILESEND_OK:
			yinfo("Sending file request to friend n°%i: %s", friendnum, fnode[packn]->file);
			break;
		case FILESEND_ERR_FILEIO:
			yerr("I/O Error accessing file for friend n°%i: %s", friendnum, fnode[packn]->file);
			break;
		case FILESEND_ERR_FULL:
			yerr("File queue full, could not send to friend n°%i: %s", friendnum, fnode[packn]->file);
			break;
		case FILESEND_ERR_SENDING:
			yerr("Error creating new file request for friend n°%i: %s", friendnum, fnode[packn]->file);
			break;
		default:
			yerr("Error sending file request to friend n°%i: %s", friendnum, fnode[packn]->file);
	}
}

struct cmd_func
{
	const char *name;
	void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE]);
};

#define COMMAND_NUM 4
static struct cmd_func bot_commands[] = {
	{"invite", cmd_invite},
	{"help", cmd_help},
	{"info", cmd_info},
	{"send", cmd_send}
};

/* Parses input command and puts args into arg array. 
   Returns number of arguments on success, -1 on failure. */
static int parse_command(char *cmd, char (*args)[MAX_ARGS_SIZE])
{
	int num_args = 0;
	int cmd_end = FALSE;    // flags when we get to the end of cmd
	char *end;               // points to the end of the current arg

	while (!cmd_end && num_args < MAX_ARGS_NUM)
	{
		end = strchr(cmd, ' ');
		cmd_end = end == NULL;
		if (!cmd_end)
			*end++ = '\0';    /* mark end of current argument */
		/* Copy from start of current arg to where we just inserted the null byte */
		strcpy(args[num_args++], cmd);
		cmd = end;
	}
	
	return num_args;
}

void execute(Tox *m, char *cmd, int friendnum)
{
	char args[MAX_ARGS_NUM][MAX_ARGS_SIZE];
	memset(args, 0, sizeof(args));
	int num_args = parse_command(cmd, args);
	int i;

	for (i = 0; i < COMMAND_NUM; ++i)
	{
		if (strcmp(args[0], bot_commands[i].name) == 0)
		{
			(bot_commands[i].func)(m, friendnum, num_args - 1, args);
			return;
		}
	}
}
/* END COMMANDS */


int group_chat_init(Tox *m)
{
	group_messenger = m;
	group_chat_number = tox_add_groupchat(m);
	if(group_chat_number == -1)
		yerr("Failed to initialize groupchat");
	return group_chat_number;
}

void on_request(uint8_t *key, uint8_t *data, uint16_t length, void *m)
{
	char *keystr;
	data[length - 1] = '\0'; /* make sure the message is null terminated */
	
	keystr = human_readable_id(key, TOX_CLIENT_ID_SIZE);
	yinfo("Friend request from %s (%s)", keystr, data);
	int friendnum = tox_add_friend_norequest(m, key);
	
	if(friendnum == -1)
		yerr("Failed to add %s", keystr);
	else
		yinfo("Added %s as friend n° %i", keystr, friendnum);
		
	free(keystr);
}

void on_message(Tox *m, int friendnum, uint8_t *string, uint16_t length, void *userdata)
{
	int rc;
	uint8_t fnick[TOX_MAX_NAME_LENGTH];
	rc = tox_get_name(m, friendnum, fnick);
	if(rc == -1)
		strcpy((char *) fnick, "Unknown");
	
	ydebug("Message from friend n°%i [%s]: %s", friendnum, fnick, string);
	
	execute(m, (char *) string, friendnum);
}

void on_new_file(FileNode *fn, int packn)
{
	uint8_t groupmsg[sizeof("New file: # [9999  b]: %s") + PATH_MAX + 32];
	char hu_size[8];
	int rc;
	if(group_chat_number == -1)
		return;
	
	human_readable_filesize(hu_size, fn->size);
	
	rc = snprintf((char *)groupmsg, sizeof(groupmsg), "New file: #%i [%s] %s", packn, hu_size, gnu_basename(fn->file));
	if(rc < 0)
		goto errormsg;
	
	rc = tox_group_message_send(group_messenger, group_chat_number, groupmsg, rc + 1); /* length needs to include the terminator */
	if(rc == -1 && tox_group_number_peers(group_messenger, group_chat_number) > 1)
		goto errormsg;
	
	return;
	
	errormsg:
		yerr("Failed to notify new file %s on groupchat", fn->file);
}
