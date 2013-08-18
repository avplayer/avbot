#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include <cxxabi.h>
#include <string.h>
#include <ucontext.h>

static void avbot_seg_handler(int sig_num, siginfo_t * info, void * ucontext)
{
	ucontext_t * uc = (ucontext_t *)ucontext;

	void * array[50];
	int size = backtrace(array, 50);

	void * caller_address = (void *)(array[2]);

	std::cerr << "Oooooops,  avbot has recived signal " << sig_num
			  << " (" << strsignal(sig_num) << ") when accessing "
			  << info->si_addr << " at address " << caller_address
			  << std::endl << std::endl;

	std::cerr <<  "Need help ?  then past and mail the output below to microcai: \n-----Stack Dump Begin------" << std::endl;
	char ** messages = backtrace_symbols(array, size);

	// skip first stack frame (points here)
	for(int i = 1; i < size && messages != NULL; ++i)
	{
		char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

		// find parantheses and +address offset surrounding mangled name
		for(char *p = messages[i]; *p; ++p)
		{
			if(*p == '(')
			{
				mangled_name = p;
			}
			else if(*p == '+')
			{
				offset_begin = p;
			}
			else if(*p == ')')
			{
				offset_end = p;
				break;
			}
		}

		// if the line could be processed, attempt to demangle the symbol
		if(mangled_name && offset_begin && offset_end &&
				mangled_name < offset_begin)
		{
			*mangled_name++ = '\0';
			*offset_begin++ = '\0';
			*offset_end++ = '\0';

			int status;
			char * real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

			// if demangling is successful, output the demangled function name
			if(status == 0)
			{
				std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
						  << real_name << "+" << offset_begin << offset_end
						  << std::endl;

			}
			// otherwise, output the mangled function name
			else
			{
				std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
						  << mangled_name << "+" << offset_begin << offset_end
						  << std::endl;
			}

			free(real_name);
		}
		// otherwise, print the whole line
		else
		{
			std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
		}
	}

	std::cerr <<  "-----Stack Dump Complete------" << std::endl;
	std::cerr << std::endl;

	exit(EXIT_FAILURE);
}

extern "C" void avbot_setup_seghandler()
{
	struct sigaction handler = {0};
	handler.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
	handler.sa_sigaction =  avbot_seg_handler ;
	sigaction(SIGSEGV, &handler, NULL);   // install our handler
	sigaction(SIGABRT, &handler, NULL);   // install our handler
}
