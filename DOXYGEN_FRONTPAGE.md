\mainpage TSIP Decoder

\section intro_sec Introduction

This program is designed to read and decode TSIP data packets.

\section install_sec Installation

Both .pro files to compile with QT and a simple Makefile are included. To compile the program simply run make from the main program directory.

\section Decoder

The packet is decoded first. The decoding process removes the escape characters and maps the flags, marking the packet starting and ending points.

\snippet tsip_decode.h Removing escape characters

Once the data block is processed, the previous data buffer is checked. This is done to ensure that if a packet is spread across several blocks it is still identified and parsed.


\snippet tsip_decode.h Checking previous buffer

If a complete packet is identified, it is validated and parsed. The validation includes checking that the packet size does not exceed minimum or maximum limits, that the first byte ID is legal and a CRC checksum comparison.

\snippet tsip_decode.h Validating packet

If the packet is not correct, a flag indicating the failure mode is returned. After validating the packets that might have started in the previous block, the current block is scanned for packets, which are also validated and parsed.


\snippet tsip_decode.h Checking uninterrupted packets

\section Tests

The tests included are the output and the speed tests. Two test data files are included in "data" folder: the tsip_sample with 3 valid data blocks and the tsip_sample_ext with 30000 blocks.

The output test is run on the shorter block, while the timing test is run on the longer one. The shorter output test can be run by appending a binary filename as a program parameter for running extra tests.

\snippet uavnav_main.c Loading a test file


The tests are run by executing the TaskUplink200Hz() command. This function starts the threads and keeps running until the entire data set is decoded.

\snippet tsip_decode.h Starting threads

The example program output is shown below:

\verbatim
Running verbose test
Loaded file data/tsip_sample
Size: 135
ID1: 165 ID2: 9 CHKSUM: 0xe85817fd
56 96 8a f1
cb 6e 13 50
8f 3c 84 44
c1 03 7c fc
3e 00 db 04
1c 05 00 76
1d 10 00 ff
ff 00 00 00
00 00 00 00
00 b1 46 d6
43 20 4e

ID1: 165 ID2: 10 CHKSUM: 0xc4c61b3e
db 04 1c 05
f9 f6 6d 44
00 00 c5 0a
76 1d 10 00
20 4e

ID1: 57 ID2: 2 CHKSUM: 0x476753ec
64 61 3e 0f
c4 c6 8a 40


Decoded 3 packets

Running a timed test
Loaded file data/tsip_sample_ext
Size: 1350000
Decoded 29535 packets
Time taken 0 seconds 9 milliseconds
\endverbatim

Note that the tsip_sample_ext file actually contains 30000 packet samples, so the program may still needs some improvements in that regard. The main problem comes from the fact that a single packet may span several blocks of data read from the COM port, and separate blocks must be merged without losing data. Every possible condition must be taken into account. The decoder functionality and methods are described in the following section.

The verbose test shows that the packets are interpreted correctly. The timed test allows to compare the performance for different setup parameters, defined in advance.

\snippet tsip_decode.h Setup parameters

The number of threads is defined by the variable N_THREADS. In theory spreading the load across several threads should increase the execution efficiency and reduce the execution times. In practice, a lot of this depends on the hardware architecture. On this machine (Intel(R) Core(TM) i7-3610QM CPU @ 2.30GHz running Debian GNU/Linux) two threads performed slightly faster than a single thread, and the performance started to decrease once the number of threads exceeded 4. Among the disadvantages of a threaded approach is that it adds to complexity of the code, and any performance benefits of running separate threads could be negated by the demands of thread management itself.

The BLOCK_SIZE variable specifies the data block size to be processed by a single thread. It was found that larger block size results in a better performance. Which is natural, since it takes less effort to analyze one continuous data block rather than a series of discontinuous ones.

The MAX_COM_SIZE parameter is the maximum allowable data size that is allowed to read from the COM port in one go. While the MAX_DATA_SIZE parameter is used to define the maximum packet size, and it is used for data validation and sanity checks.
