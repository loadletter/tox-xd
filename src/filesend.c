#include <stdio.h>
#include <time.h>
#include <tox/tox.h>

#include "filesend.h"

uint8_t file_senders_index;
FileSender file_senders[FSEND_PIECE_SIZE];

void file_sender_close(int i)
{
	fclose(file_senders[i].file);
	memset(&file_senders[i], 0, sizeof(FileSender));

	int j;

	for (j = file_senders_index; j > 0; --j)
	{
		if (file_senders[j-1].active)
			break;
	}

	file_senders_index = j;
}


void file_senders_do(Tox *m)
{
	int i, rs;

	for (i = 0; i < file_senders_index; ++i)
	{
		if (!file_senders[i].active)
			continue;

		uint8_t *pathname = (uint8_t *) file_senders[i].details->file;
		uint8_t filenum = file_senders[i].filenum;
		int friendnum = file_senders[i].friendnum;
		FILE *fp = file_senders[i].file;
		time_t current_time = time(NULL);

		/* If file transfer has timed out kill transfer and send kill control */
		if ((file_senders[i].timestamp + FSEND_TIMEOUT) <= current_time)
		{
			WARNING("Transfer timed out: %s", pathname);			
			tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_KILL, 0, 0);
			file_sender_close(i);
			continue;
		}

		int pieces = 0;

		while (pieces++ < FSEND_MAX_PIECES)
		{
			if(tox_file_send_data(m, friendnum, filenum, file_senders[i].nextpiece, file_senders[i].piecelen) == -1)
				break;

			file_senders[i].timestamp = current_time;
			rs = tox_file_data_size(m, friendnum);
			file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1, rs, fp);

			if (file_senders[i].piecelen == 0)
			{
				INFO("Transfer completed: %s", pathname);
				tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
				file_sender_close(i);
				break;
			}
		}
	}
}

/* cut absolute pathname to filename */
char *gnu_basename(char *path)
{
	char *base = strrchr(path, '/');
	return base ? base+1 : path;
}

int file_sender_new(int friendnum, FileNode *fileinfo, Tox *m)
{
	if (file_senders_index >= (FSEND_MAX_FILES - 1))
	{
		return FILESEND_ERR_FULL;
	}


	FILE *file_to_send = fopen(fileinfo->file, "r");
	if (file_to_send == NULL)
	{
		perror("fopen");
		return FILESEND_ERR_FILEIO;
	}

	uint64_t filesize = (uint64_t) fileinfo->size;
	char *filename = gnu_basename(fileinfo->file);

	int filenum = tox_new_file_sender(m, friendnum, filesize, (uint8_t *) filename, strlen(filename));
	if (filenum == -1)
	{
		return FILESEND_ERR_SENDING;
	}

	int i;

	for (i = 0; i < FSEND_MAX_FILES; ++i)
	{
		if (!file_senders[i].active)
		{
			file_senders[i].details = fileinfo;
			file_senders[i].active = TRUE;
			file_senders[i].file = file_to_send;
			file_senders[i].filenum = (uint8_t) filenum;
			file_senders[i].friendnum = friendnum;
			file_senders[i].timestamp = time(NULL);
			file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
											 tox_file_data_size(m, friendnum), file_to_send);

			if (i == file_senders_index)
				++file_senders_index;

			return FILESEND_OK;
		}
	}
	
	return FILESEND_ERR;
}


/* This should only be called on exit */
void file_transfers_close(void)
{
	int i;

	for (i = 0; i < file_senders_index; ++i)
	{
		if (file_senders[i].active)
			fclose(file_senders[i].file);
	}
}
