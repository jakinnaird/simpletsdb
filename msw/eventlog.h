//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_SYSTEM                  0x0
#define FACILITY_RUNTIME                 0x2
#define FACILITY_STUBS                   0x3
#define FACILITY_IO_ERROR_CODE           0x4


//
// Define the severity codes
//
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_ERROR            0x3


//
// MessageId: MSG_EXCEPTION
//
// MessageText:
//
// %2
//
#define MSG_EXCEPTION                    ((DWORD)0xC00203E8L)

//
// MessageId: MSG_STARTING
//
// MessageText:
//
// %2
//
#define MSG_STARTING                     ((DWORD)0x400203E9L)

//
// MessageId: MSG_RUNNING
//
// MessageText:
//
// %2
//
#define MSG_RUNNING                      ((DWORD)0x400203EAL)

//
// MessageId: MSG_STOPPING
//
// MessageText:
//
// %2
//
#define MSG_STOPPING                     ((DWORD)0x400203EBL)

//
// MessageId: MSG_STOPPED
//
// MessageText:
//
// %2
//
#define MSG_STOPPED                      ((DWORD)0x400203ECL)

