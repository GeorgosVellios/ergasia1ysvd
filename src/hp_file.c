#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define HP_OK 0
#define HP_ERROR -1

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HP_ERROR;        \
    }                         \
  }

int HP_CreateFile(char *fileName)
{
  int file_desc;
  BF_CreateFile(fileName);

  BF_OpenFile(fileName, &file_desc);
  BF_Block *block;
  BF_Block_Init(&block);

  BF_AllocateBlock(file_desc, block);

  HP_info hp_info;
  hp_info.last_block_id = 0;
  hp_info.max_records_per_block = 7; // we suppose that every block has 7 max records

  char *data = BF_Block_GetData(block);
  memcpy(data, &hp_info, sizeof(HP_info));

  BF_Block_SetDirty(block);
  BF_UnpinBlock(block);

  BF_CloseFile(file_desc);

  BF_Block_Destroy(&block);

  return HP_OK;
}

HP_info *HP_OpenFile(char *fileName, int *file_desc)
{
  if (BF_OpenFile(fileName, file_desc) != BF_OK)
    return NULL;

  BF_Block *block;
  BF_Block_Init(&block);
  if (BF_GetBlock(*file_desc, 0, block) != BF_OK)
    return NULL;

  char *data = BF_Block_GetData(block);
  HP_info *hp_info = (HP_info *)data;

  if (BF_UnpinBlock(block) != BF_OK)
    return NULL;
  return hp_info;
}

int HP_CloseFile(int file_desc, HP_info *hp_info)
{
  if (BF_CloseFile(file_desc) != BF_OK)
    return HP_ERROR;

  return HP_OK;
}

int HP_InsertEntry(int file_desc, HP_info *hp_info, Record record)
{
  BF_Block *block;
  char *data;
  int records_num;

  BF_Block_Init(&block);

  int last_block_num = hp_info->last_block_id;
  if (last_block_num >= 0)
  {
    // If there is at least one block we read it
    if (BF_GetBlock(file_desc, last_block_num, block) != BF_OK)
      return -1;

    data = BF_Block_GetData(block);
    memcpy(&records_num, data, sizeof(int));

    int offset = sizeof(int) + records_num * sizeof(Record);

    if (records_num >= hp_info->max_records_per_block || (offset + sizeof(Record) > BF_BLOCK_SIZE))
    {
      // Current block is full or doesn't fit any records, so we make a new block
      if (BF_AllocateBlock(file_desc, block) != BF_OK)
      {
        return -1;
      }
      hp_info->last_block_id++;
      data = BF_Block_GetData(block);
      records_num = 0; // We reset the records_num to 0 because its a new block.
      offset = sizeof(int);
      memcpy(data, &records_num, sizeof(int));
    }

    // We now have enough space to add a new record
    BF_Block_SetDirty(block);
    memcpy(data + offset, &record, sizeof(Record));
    records_num++;
    memcpy(data, &records_num, sizeof(int));
    CALL_BF(BF_UnpinBlock(block));
  }
  else
  {
    // If there isn't a block, we make one and we insert a record there

    if (BF_AllocateBlock(file_desc, block) != BF_OK)
      return -1;

    data = BF_Block_GetData(block);
    hp_info->last_block_id = 0;
    records_num = 1;
    memcpy(data, &records_num, sizeof(int));
    int offset = sizeof(int);
    memcpy(data + offset, &record, sizeof(Record));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
  }

  BF_Block_Destroy(&block);

  return hp_info->last_block_id;
}

int HP_GetAllEntries(int file_desc, HP_info *hp_info, int value)
{
  int blocks_read = 0; // Αρχικοποίηση του μετρητή των διαβασμένων block.
  BF_Block *block;
  BF_Block_Init(&block);

  int last_block_num = hp_info->last_block_id;

  // We make a buffer type.
  char *data;

  // We read date for every block
  for (int i = 0; i <= last_block_num; i++)
  {
    if (BF_GetBlock(file_desc, i, block) != BF_OK)
      return -1;
    blocks_read++;

    data = BF_Block_GetData(block);

    // Number of entries in a block
    int records_num;
    memcpy(&records_num, data, sizeof(int));

    // We check in every block for the value of the id.
    for (int j = 0; j < records_num; j++)
    {
      Record *record = (Record *)(data + sizeof(int) + j * sizeof(Record));
      if (record->id == value)
      {
        // Βρέθηκε εγγραφή με το επιθυμητό id, εκτυπώστε την.
        printf("\nID: %d, Name: %s, Surname: %s, City: %s \n", record->id, record->name, record->surname, record->city);
      }
    }

    if (BF_UnpinBlock(block) != BF_OK)
      return -1;
  }

  BF_Block_Destroy(&block);
  return blocks_read;
}
