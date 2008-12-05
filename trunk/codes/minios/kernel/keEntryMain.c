#include <platform.h>
#include <device.h>
#include <kernel.h>
#include <string.h>

uint32 htol(char* hexstr)
{
	int r=0;
	while(*hexstr)
	{
		if(*hexstr>='0' && *hexstr<='9')
		{
			r<<=4;
			r += *hexstr-'0';
		}else if(*hexstr>='A' && *hexstr<='F')
		{
			r<<=4;
			r += *hexstr-'A'+10;
		}else if(*hexstr>='a' && *hexstr<='f')
		{
			r<<=4;
			r += *hexstr-'a'+10;
		}else
			return r;
		hexstr++;
	}
	return r;
}

void dumpMemory(uint32 addr)
{
	uint8 buf[256];
	int i,j;
	uint8 * p;
	memcpy(buf, (uint8*)addr, 256);
	p = (uint8*)buf;
	for(j=0;j<16;j++)
	{
		printf("%08X  ", addr+j*16);
		for(i=0;i<16;i++)
		{
			printf("%02X ", p[j*16+i]);
		}

		for(i=0;i<16;i++)
		{
			uint8 c=p[j*16+i];

			if(c>=0x20 && c<=126)
				printf("%c", p[j*16+i]);
			else
				printf(".");
		}
		printf("\n");
	}
}

struct semaphore debugevent;
int debugret=BPSTATE_RESUME;

int breakfunc(struct break_regstat_t * regs)
{
	int i;
	printf("Break Point Asserted\n");
	printf("eip=%08x ecs=%08x flag=%08x\n", regs->eip, regs->ecs, regs->eflags);
	printf("eax=%08x ebx=%08x ecx=%08x edx=%08x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
	printf("esp=%08x ebp=%08x esi=%08x edi=%08x\n", regs->esp, regs->ebp, regs->esi, regs->edi);
	for(i=0;i<32;i++)
		printf("%08X  ", ((uint32*)(regs->esp))[i]);
	wait(&debugevent);
	return debugret;
}


void kdebug(uint32 param)
{
	char buffer[80];
	initsemaphore(&debugevent, 0);
	while(1)
	{
		printf("> ");
		gets(buffer);
		
		switch(buffer[0])
		{
			case 'd':
			{
				uint32 addr=htol(&buffer[2]);
				printf("Dump Memory Address=%08x\n", addr);
				dumpMemory(addr);
				break;
			}
			case 'i':
			{
				uint32 addr=htol(&buffer[2]);
				printf("IO port %04x = %02x \n", addr, _in(addr));
				break;
			}
			case 'b':
			{
				int b;
				uint32 procid=htol(&buffer[2]);
				uint32 addr=htol(&buffer[4]);
				b=setbreadpoint(procid, addr, breakfunc);
				break;
			}
			case 'n':
			{
				debugret=BPSTATE_STEP;
				release(&debugevent);
				break;
			}
			case 'c':
			{
				debugret=BPSTATE_RESUME;
				release(&debugevent);
				break;
			}
			case 'r':
			{
				debugret=BPSTATE_DISCARD;
				release(&debugevent);
				break;
			}
			case 'a':
			{
				uint32 len=htol(&buffer[2]);
				uint32 addr=(uint32)keMalloc(len);
				printf("Alloc Memory from kernel heap: %08x[%08x]\n", addr, len);
				break;
			}
			case 'f':
			{
				uint32 addr=htol(&buffer[2]);
				keFree((void*)addr);
				printf("Free Memory from kernel heap, pointer=%08x\n", addr);
				break;
			}
			case 'h':
				keKernelHeapDump();
				break;
			case '?':
				puts("a size: Alloc size Memory From Kernel Heap\n");
				puts("d addr: Dump Memory Around Address addr\n");
				puts("f addr: Free Memory at addr From Kernel Heap\n");
				puts("h: View Kernel Heap Assignmemt\n");
				puts("k id: Terminate The Task id\n");
				puts("t: Show Task List\n");
				break;
			case 'k':
				{
					uint32 id=htol(&buffer[2]);
					keKillTask(id);
					break;
				}
			case 't':
				keShowTasks();
				break;
			default :
				puts("Invalid command, type '?' to see commands list\n");
				break;
		}
	}
}

struct semaphore s;

void test1(uint32 param)
{
	int r;
	r=ide_readdma(0x10000, 0, 128);
	printf("test ide dma: %d bytes readed\n", r);

	while(1)
	{
		wait(&s);
		printf("Task test1 Still Active\n");
	}
}

int f()
{
//	printf("Test Debug system\n");
	return 1;
}

void keEntryMain(uint32 param)
{
	int r;
	_sti();

	keLoadDriver(&keyboard_driver_object);
	keLoadDriver(&console_driver_object);
	initKeDebug();

	printf("booting...\n");
	keNewTask("dpcmain", keDpcProc, 0, 6, 0x4000);
	pciInit();

	r=keLoadDriver(&ide_driver_object);
	r=keLoadDriver(&eth_driver_object);

	initsemaphore(&s, 0);
	keNewTask("kdebug", kdebug, 0, 8, 0x4000);
	keNewTask("test1", test1, 0, 7, 0x4000);

	while(1)
	{
		f();
		keDelay(200);
//		release(&s);
	}
}
