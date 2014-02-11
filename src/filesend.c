#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tox/tox.h>

#include "filesend.h"

static uint8_t file_senders_index;
static FileSender file_senders[FSEND_MAX_FILES];
char packlist_filename[] = "packlist.txt";

static void file_sender_close(int i)
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
		
		/* if fileinfo is NULL it's a packlist */
		uint8_t *pathname = (uint8_t *) packlist_filename;
		if(file_senders[i].details != NULL)
			pathname = (uint8_t *) file_senders[i].details->file;
		
		uint8_t filenum = file_senders[i].filenum;
		int friendnum = file_senders[i].friendnum;
		FILE *fp = file_senders[i].file;
		time_t current_time = time(NULL);

		/* If file transfer has timed out kill transfer and send kill control */
		if ((file_senders[i].timestamp + FSEND_TIMEOUT) <= current_time)
		{
			yerr("Transfer timed out: %s", pathname);			
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
				yinfo("Transfer completed: %s", pathname);
				tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
				file_sender_close(i);
				break;
			}
		}
	}
}

static FILE *file_list_create(void)
{
	FileNode **fnode = file_get_shared();
	int fnode_len = file_get_shared_len();
	char hu_size[8]; 
	int i, n = 0;
	
	FILE *fp = tmpfile();
	if(fp == NULL)
	{
		perrlog("tmpfile");
		return NULL;
	}
	
	for (i = 0; i < FSEND_MAX_FILES; ++i)
		if(file_senders[i].active)
			n++;
	
	fprintf(fp, "** %i Packs ** %i/%i Slots open **\n", fnode_len, n, FSEND_MAX_FILES);
	
	for(i = 0; i < fnode_len; i++)
	{
		human_readable_filesize(hu_size, fnode[i]->size);
		fprintf(fp, "#%-10i [%s] %s\n", i, hu_size, gnu_basename(fnode[i]->file));
	}
	
	return fp;
}

int file_sender_new(int friendnum, FileNode **shrfiles, int packn, Tox *m)
{
	FileNode *fileinfo = NULL;
	FILE *file_to_send;
	uint64_t filesize;
	char *filename;
	
	if (file_senders_index >= (FSEND_MAX_FILES - 1))
	{
		return FILESEND_ERR_FULL;
	}
	
	if(packn < 0)
	{
		/* create packlist */
		file_to_send = file_list_create();
		if(file_to_send == NULL)
			return FILESEND_ERR_FILEIO;
		
		filesize = ftell(file_to_send);
		fseek(file_to_send, 0, SEEK_SET);
		filename = packlist_filename;
	}
	else
	{
		/* use existing file */
		fileinfo = shrfiles[packn];
		file_to_send = fopen(fileinfo->file, "r");
		if (file_to_send == NULL)
		{
			perrlog("fopen");
			return FILESEND_ERR_FILEIO;
		}

		filesize = (uint64_t) fileinfo->size;
		filename = gnu_basename(fileinfo->file);
	}
	
	int filenum = tox_new_file_sender(m, friendnum, filesize, (uint8_t *) filename, strlen(filename) + 1); /* need to include terminator in the filaname */
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
