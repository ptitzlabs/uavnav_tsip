/** @file tsip_read.h
 * \brief Header containing the testing functions.
 * These functions are designed to simulate the UAV parser and the COM interface
*/
#ifndef TSIP_H
#define TSIP_H 

#include "stdio.h"
#include "util.h"
#include "stdbool.h"
/**
 * \brief DLE flag
 */
#define DLE 0x10
/**
 * \brief ETX flag
 */
#define ETX 0x03

/**
 * \brief Test data array
 */
extern unsigned char * g_test_data;
/**
 * \brief Test data length
 */
extern uint32_t g_test_data_len;
/**
 * \brief Start of the test data
 */
extern uint32_t g_test_data_start;

/**
 * \brief Verbose output parameter
 */
extern bool g_verbose_output;

/**
* \brief Read received raw data from COM port.
*
* This is a non-blocking read function. This means that only available received
* data will be served. User may decide to call this function within a loop
* until the desired amount of data is received.
*
* \param buffer Pointer to destination buffer where to copy received data.
* \param count Maximum number of bytes to be read.
* \return Number of read bytes. This value will always be <= count.
*/
uint32_t uavnComRead(uint8_t* const buffer, const uint32_t count) {
//    static uint32_t start = 0;

    // end of test data
    if (g_test_data_start == g_test_data_len) return 0;

    uint32_t end = min(g_test_data_start + count, g_test_data_len);

    // fill the buffer from the test data array
    for (uint32_t i = g_test_data_start; i < end; i++) {
        buffer[i - g_test_data_start] = g_test_data[i];
    }
    int32_t len = end - g_test_data_start;
    g_test_data_start = end;
    return len;
}
/**
* \brief Parse a TSIP data.
*
* \param[in] buffer Const pointer where data is located.
* \param[in] numberOfBytes It is filled with the number bytes in the TSIP
* packet.
*/
void ParseTsipData(const uint8_t* const buffer, const uint32_t numberOfBytes) {
    if(g_verbose_output){
    printf("ID1: %u ID2: %u CHKSUM: %#08x\n",buffer[0],buffer[1], crc32(buffer,0,numberOfBytes));
    int style_counter = 0;
    for (int i = 2; i < numberOfBytes; i++) {
            printf("%02x ", buffer[i]);
        style_counter++;
        if(style_counter == 4){
        style_counter = 0;
    printf("\n");
        }

    }
    printf("\n\n");

    }
}
#endif
