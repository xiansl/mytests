#ifndef __RECONFIG_H__
#define __RECONFIG_H__

#define HEADER_ID 0x9955123456781014ULL
#define RET_REF_DONE		0
#define RET_REF_HELP		1
#define RET_DEV_ERROR		2
#define RET_REF_ERROR		3
#define RET_FILE_ERROR		4

struct extra_header_t {
	unsigned long long header_id;		// 8 Bytes
	char acc_name [32];					// 32 Bytes
	unsigned long long fpga_id;
	unsigned long long platform_id;
	unsigned long long platform_version;
	unsigned long long compiling_id;
	unsigned long long acc_id;
	unsigned long long acc_version;
	unsigned int port_id;
	unsigned int file_len;				// real lenght of the bit file
	unsigned int frequency;				// the Max clock frequency (MHz) to run this accelerator
	char pad [8];
	unsigned int header_md5 [4];
	unsigned int next_header;
};


#define myswap(a,b)	((a)^=(b));((b)^=(a));((a)^=(b));
#if __BYTE_ORDER == __BIG_ENDIAN
union union64_8
{
	unsigned long long ll;
	unsigned char c [8];
};

union union32_8
{
	unsigned int ll;
	unsigned char c [4];
};

union union16_8
{
	unsigned short ll;
	unsigned char c [2];
};

static inline unsigned long long myntohll (unsigned long long a)
{
	union union64_8 tmp;
	tmp.ll = a;
	myswap (tmp.c [0], tmp.c [7]);
	myswap (tmp.c [1], tmp.c [6]);
	myswap (tmp.c [2], tmp.c [5]);
	myswap (tmp.c [3], tmp.c [4]);
	return tmp.ll;
}

static inline unsigned int myntohl (unsigned int a)
{
	union union32_8 tmp;
	tmp.ll = a;
	myswap (tmp.c [0], tmp.c [3]);
	myswap (tmp.c [1], tmp.c [2]);
	return tmp.ll;
}

static inline unsigned short myntohs (unsigned short a)
{
	union union16_8 tmp;
	tmp.ll = a;
	myswap (tmp.c [0], tmp.c [1]);
	return tmp.ll;
}
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline unsigned long long myntohll (unsigned long long a) {return a;}
static inline unsigned int myntohl (unsigned int a) {return a;}
static inline unsigned short myntohs (unsigned short a) {return a;}
#endif


#endif
