#ifndef SBCP_HEADER
#define SBCP_HEADER

enum m_type {ACK=7,NAK=5,ONLINE=8,OFFLINE=6,IDLE=9,FWD=3,JOIN=2,SEND=4}; 
enum a_type {USERNAME=2,MESSAGE=4,REASON=1,COUNT=3};
enum c_type {UNICAST, MULTICAST};

#define IDLETIME 10000000

struct SBCPH{
    unsigned int vrsn:9;
    unsigned int type:7;
    int length:16;
};

struct SBCPA{
    int type:16;
    int length:16;
    char payload[512];
};

struct SBCPM{
    struct SBCPH header;
    struct SBCPA attribute[2];
};

struct client{
    char name[16];
    int fd;
};

#endif
