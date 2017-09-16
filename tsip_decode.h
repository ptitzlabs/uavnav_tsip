/** @file tsip_decode.h
 * \brief Header containing the decoding functions.
 * The main decoder code is contained here
*/
#ifndef DATA_H
#define DATA_H

#include "string.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "tsip_read.h"

  //! [Setup parameters]
/**
 * \brief Number of concurrent threads.
 */
#define N_THREADS 2
/**
 * \brief Data block size to be processed by a single thread.
 */
#define BLOCK_SIZE 2048
/**
 * \brief Maximum allowable raw data size.
 */
#define MAX_COM_SIZE 512
/**
 * \brief Maximum allowable decoded data size.
 */
#define MAX_DATA_SIZE 256
  //! [Setup parameters]
/**
 * \brief Debug flag.
 */
#define DEBUG false

  //! [Setup Parameters]
/**
 * \brief Data read mutex locks.
 */
static pthread_mutex_t g_data_read_lock;
static uint8_t g_data_read_seq;
static uint32_t packet_counter;

/**
 * \brief Data parse mutex locks.
 */
static pthread_mutex_t g_data_parse_lock;
static uint8_t g_data_parse_seq;

/**
 * \brief Trailing data buffer.
 */
static uint8_t g_inter_buffer[MAX_DATA_SIZE + 6];
static uint32_t g_inter_buffer_len = 0;
static bool g_inter_buffer_dle = false;

/**
 * \brief Validation error types.
 */
enum validate_error { none, size_mismatch, illegal_id, chksum_mismatch };

/**
 * \brief Special character flag types.
 */
enum flag_type_enum { start_flag, end_flag };

/**
* \brief Validate the packet.
*
* Check the packet integrity, correct size and parameters. Run a CRC check.
*
* \param buffer 	Pointer to packet source.
* \param start 		Packet data start.
* \param end 		Packet data end.
* \return 			Error type, 0 if no errors are found.
*/
uint8_t validate_packet(const uint8_t *buffer, const uint32_t start,
                        const uint32_t end) {
  uint32_t len = end - start;
  //! [Validating packet]
  // packet size
  if (len < 6 || len > MAX_DATA_SIZE + 6) {
    return size_mismatch;
  }
  // ID
  if (buffer[start] == DLE) {
    return illegal_id;
  }

  // chksum
  uint32_t chksum_start = end - 4;
  uint32_t chksum_int = arr_to_int(buffer + chksum_start, 4);
  uint32_t chksum_chk = crc32(buffer, start, chksum_start);
  if (chksum_int != chksum_chk) {
    return chksum_mismatch;
  }
  //! [Validating packet]

  // no errors found
  return 0;
}

/**
* \brief Read the recieved raw data, multithread-protected.
*
* Read the data. Only one thread is allowed to read data at a time.
*
* \param raw 		Pointer to raw data destination.
* \param raw_len 	Pointer to the size of the data read.
* \param id 		Thread id.
* \return 			Error type, 0 if no errors are found.
*/
void data_read(uint8_t *raw, uint32_t *raw_len, uint8_t id) {
  // grab the raw data
  *raw_len = 0;
  uint32_t len = 0;
  // bytes to read
  uint32_t res = BLOCK_SIZE;
  uint32_t data_size;
  while (true) {
    // make sure that it's the right turn
    pthread_mutex_lock(&g_data_read_lock);
    if (g_data_read_seq == id) {
      //! [Reading COM data]
      do {
        // keep filling up the raw buffer until it's full
        data_size = (res < MAX_COM_SIZE) ? res : MAX_COM_SIZE;
        len = uavnComRead(raw + (*raw_len), data_size);
        (*raw_len) += len;
        res -= len;
      } while (len != 0 && res != 0);
      //! [Reading COM data]

      // queue the next thread
      if (id == N_THREADS - 1) {
        g_data_read_seq = 0;
      } else {
        g_data_read_seq++;
      }
      // exit
      pthread_mutex_unlock(&g_data_read_lock);
      break;
    }
    pthread_mutex_unlock(&g_data_read_lock);
  }
}

/**
* \brief Process raw data.
*
* Process the raw data, removing the escape characters and marking packet start
* and end points
*
* \param processed 		Pointer to destination.
* \param processed_len 	Size of processed data.
* \param raw 			Pointer to source raw data.
* \param raw_len 		Size of source data.
* \param flag 			Pointer to flags, contains flag locations.
* \param flag_type 		Types of marked flags.
* \param flag_count 	Number of flags.
* \param hanging_dle 	A flag to indicate a hanging DLE character.
* \return
*/
void packet_decode(uint8_t *processed, uint32_t *processed_len, uint8_t *raw,
                   uint32_t *raw_len, uint32_t *flag, uint8_t *flag_type,
                   uint32_t *flag_count, bool *hanging_dle) {

  *flag_count = 0;
  *processed_len = 0;
  uint32_t i = 0;
  *hanging_dle = false;
  //! [Removing escape characters]
  while (i < *raw_len) {
    // check for DLE
    if (raw[i] == DLE) {
      // check if there are still bytes left in the buffer
      if (i + 1 < *raw_len) {
        // check the byte after the DLE
        i++;
        switch (raw[i]) {
        case ETX:
          // end of packet
          flag[*flag_count] = *processed_len;
          flag_type[*flag_count] = end_flag;
          (*flag_count)++;
          break;
        case DLE:
          // escape flag, don't do anything
          processed[(*processed_len)++] = raw[i];
          break;
        default:
          // data start
          flag[*flag_count] = *processed_len;
          flag_type[*flag_count] = start_flag;
          (*flag_count)++;
          processed[(*processed_len)++] = raw[i];
          break;
        }
        i++;
      } else {
        // end of raw data, just store it as-is and figure it out
        // later
        *hanging_dle = true;
        processed[(*processed_len)++] = raw[i];
        i++;
      }
    } else {
      processed[(*processed_len)++] = raw[i];
      i++;
    }
  }
  //! [Removing escape characters]
}

/**
* \brief Test for special condition that the previous block ended on a DLE.
*
* Check if the previous raw data block ended on a DLE. Correct the following
* data block structure and data to match the trailing DLE character.
*
* \param[in] processed 		Pointer to processed data
* \param[in] processed_len 	Size of processed data
* \param flag 			Pointer to flags, contains flag locations
* \param flag_type 		Types of marked flags
* \param flag_count 	Number of flags
* \return
*/
void hanging_dle_test(uint8_t *processed, uint32_t processed_len,
                      uint32_t *flag, uint8_t *flag_type,
                      uint32_t *flag_count) {

  //! [Checking hanging DLE flags]
  // check if the last block ended on a DLE flag
  if (g_inter_buffer_len > 0 && g_inter_buffer_dle == true) {
    g_inter_buffer_dle = false;

    if ((*flag_count) > 0 && flag[0] == 0 && flag_type[0] == start_flag) {
      // the first start flag is false
      memcpy(flag, flag + 1, ((*flag_count) - 1) * sizeof(uint32_t));
      memcpy(flag_type, flag_type + 1, ((*flag_count) - 1) * sizeof(uint8_t));
      (*flag_count)--;
    } else if (processed_len > 0) {
      if (processed[0] == ETX) {
        // the first byte indicates an end flag
        memcpy(flag + 1, flag, (*flag_count) * sizeof(uint32_t));
        memcpy(flag_type + 1, flag_type, (*flag_count) * sizeof(uint8_t));
        flag[0] = 0;
        flag_type[0] = end_flag;
        (*flag_count)++;
        g_inter_buffer_len--;
      } else if (processed[0] == DLE) {
        // the first byte is escaped
        g_inter_buffer_len--;
      } else {
        // the first byte indicates a start flag
        memcpy(flag + 1, flag, (*flag_count) * sizeof(uint32_t));
        memcpy(flag_type + 1, flag_type, (*flag_count) * sizeof(uint8_t));
        flag[0] = 0;
        flag_type[0] = start_flag;
        (*flag_count)++;
        g_inter_buffer_len--;
      }
    }
  }
  //! [Checking hanging DLE flags]
}

/**
* \brief Store the trailing data
*
* If no DLE/ETX combo is found at the end of the packet, pass on it's trailing
* part to the next evaluation.
*
* \param[in] processed 		Pointer to processed data.
* \param[in] processed_len 	Size of processed data.
* \param[in] flag 			Pointer to flags, contains flag
* locations.
* \param[in] flag_type 		Types of marked flags.
* \param[in] flag_count 	Number of flags.
* \param[in] hanging_dle 	Hanging DLE character indicator.
* \return
*/
void trailing_data_store(uint8_t *processed, uint32_t processed_len,
                         uint32_t *flag, uint8_t *flag_type,
                         uint32_t *flag_count, bool hanging_dle) {
  //! [Storing trailing data]
  // store the tail of the processed data into the inter buffer, from the
  // last start flag to the end
  if (flag_type[(*flag_count) - 1] == start_flag) {
    uint32_t loc = flag[(*flag_count) - 1];
    uint32_t len = processed_len - loc;
    if (g_inter_buffer_len + processed_len <= MAX_DATA_SIZE + 6) {
      memcpy(g_inter_buffer, processed + loc, len * sizeof(uint16_t));
      g_inter_buffer_len = len;
      g_inter_buffer_dle = hanging_dle;
    } else {
      g_inter_buffer_len = 0;
      g_inter_buffer_dle = false;
    }
  } else {
    // if the last byte is a DLE flag then it could be a hanging delimiter
    if (processed[processed_len - 1] == DLE && hanging_dle) {
      g_inter_buffer_len = 1;
      g_inter_buffer[0] = DLE;
      g_inter_buffer_dle = hanging_dle;
    } else {
      // if the last flag isnt a start flag just discard it
      g_inter_buffer_len = 0;
      g_inter_buffer_dle = false;
    }
  }
  //! [Storing trailing data]
}

/**
* \brief Parse the decoded data, multithread-protected
*
* Format the data, run the checks on the preceding data block. Run the
* validation checks. Parse the data if it's correct. Store the trailing part
* that has not been parsed. Only one thread can parse the data at a time to make
* sure that it's parsed in the same order as it comes in.
*
* \param[in] processed 		Pointer to processed data.
* \param[in] processed_len 	Size of processed data.
* \param[in] flag 			Pointer to flags, contains flag
* locations.
* \param[in] flag_type 		Types of marked flags.
* \param[in] flag_count 	Number of flags.
* \param[in] id 			Thread id.
* \param[in] hanging_dle 	Hanging DLE character indicator.
* \return
*/
void data_parse(uint8_t *processed, uint32_t processed_len, uint32_t *flag,
                uint8_t *flag_type, uint32_t *flag_count, uint8_t id,
                bool hanging_dle) {
  uint32_t i = 0;
  while (true) {
    // make sure that it's the right turn
    pthread_mutex_lock(&g_data_parse_lock);

    if (g_data_parse_seq == id) {
      //! [Checking previous buffer]
      // check if the last block ended on a DLE flag
      hanging_dle_test(processed, processed_len, flag, flag_type, flag_count);
      // make sure there are some flags found
      if ((*flag_count) > 0) {
        // check if there's data from before
        if (g_inter_buffer_len > 0) {

          if (flag_type[0] == end_flag) {
            // patch the data together
            memcpy(g_inter_buffer + g_inter_buffer_len, processed,
                   flag[0] * sizeof(uint8_t));
            g_inter_buffer_len += flag[0];
            // validate and parse
            if (validate_packet(g_inter_buffer, 0, g_inter_buffer_len) == 0) {
              ParseTsipData(g_inter_buffer, g_inter_buffer_len - 4);
              packet_counter++;
            }
            // reset buffer
            g_inter_buffer_len = 0;
            g_inter_buffer_dle = false;
          }
        }
        //! [Checking previous buffer]

        //! [Checking uninterrupted packets]
        // check the uninterrupted packets
        for (i = 0; i < (*flag_count) - 1; i++) {
          if (flag_type[i] == start_flag && flag_type[i + 1] == end_flag) {
            // uninterrupted packet, validate and parse
            if (validate_packet(processed, flag[i], flag[i + 1]) == 0) {
              ParseTsipData(processed + flag[i], flag[i + 1] - flag[i] - 4);
              packet_counter++;
            }
          }
        }
        //! [Checking uninterrupted packets]
        trailing_data_store(processed, processed_len, flag, flag_type,
                            flag_count, hanging_dle);
      } else {
        // no flags here, just dump the whole sequence into the inter buffer
        if (g_inter_buffer_len + processed_len <= MAX_DATA_SIZE + 6) {
          memcpy(g_inter_buffer + g_inter_buffer_len, processed,
                 processed_len * sizeof(uint8_t));
          g_inter_buffer_len += processed_len;
          g_inter_buffer_dle = hanging_dle;

        } else {
          g_inter_buffer_len = 0;
          g_inter_buffer_dle = false;
        }
      }

      // advance the thread queue
      if (id == N_THREADS - 1) {
        g_data_parse_seq = 0;
      } else {
        g_data_parse_seq++;
      }
      pthread_mutex_unlock(&g_data_parse_lock);
      break;
    }
    pthread_mutex_unlock(&g_data_parse_lock);
  }
}

/**
* \brief Data processing thread function
*
* Thread function tasked with reading, decoding and parsing the data. Only one
* thread can read or parse the data at a time, while the decoding is done
* concurrently.
*
* \param[in] tid Thread id.
* \return
*/
void *extract_data(void *tid) {
  uint8_t id = *((uint8_t *)tid);
  uint8_t processed[BLOCK_SIZE];
  uint32_t processed_len;
  uint8_t raw[BLOCK_SIZE];

  uint32_t flag[BLOCK_SIZE];
  uint8_t flag_type[BLOCK_SIZE];
  uint32_t flag_count;
  uint32_t raw_len;
  bool hanging_dle = false;

#if DEBUG
  printf("T%u: starting thread\n", id);
#endif
  while (true) {
    data_read(raw, &raw_len, id);
#if DEBUG
    printf("T%u: read %u bytes\n", id, raw_len);
#endif

    if (raw_len == 0)
      // quit once the data is all gone
      break;
    // escape characters, map flags
    packet_decode(processed, &processed_len, raw, &raw_len, flag, flag_type,
                  &flag_count, &hanging_dle);
    // wait for the right turn and parse
    data_parse(processed, processed_len, flag, flag_type, &flag_count, id,
               hanging_dle);
    // look for valid packets to interpret
  }
  return NULL;
}

/**
* \brief Decoder task periodic function
*
* This function spawns several decoder threads
*
* \return
*/
void TaskUpLink200Hz() {
  packet_counter = 0;
  uint8_t ints[N_THREADS];
  pthread_t thread[N_THREADS];
  //! [Starting threads]
  g_data_read_seq = 0;
  for (uint8_t i = 0; i < N_THREADS; i++) {
    ints[i] = i;
    pthread_create(&thread[i], NULL, extract_data, &ints[i]);
  }
  //! [Starting threads]
  for (uint8_t i = 0; i < N_THREADS; i++) {
    pthread_join(thread[i], NULL);
  }
  printf("Decoded %u packets\n", packet_counter);
}

#endif
