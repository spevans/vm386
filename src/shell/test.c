/* test.c */

#include <vmm/shell.h>
#include <vmm/string.h>

#ifdef TEST_FS
# include <vmm/fs.h>
# include <stdlib.h>
# include <stdio.h>
  struct shell_module *shell = &shell_module;
#endif

struct shell test_shell;

int
main(int argc, char **argv)
{
    char *prog_name = *argv;
    if(shell_init())
    {
#ifdef TEST_FS
	char *file = "test_dev.image";
	u_long blocks = 0;
	bool mkfs = FALSE;
	u_long reserved = 0;
	argc--; argv++;
	while(argc > 0)
	{
	    if(**argv == '-')
	    {
		switch(argv[0][1])
		{
		case 'f':
		    if(argc >= 2)
		    {
			argc--; argv++;
			file = argv[0];
		    }
		    break;
		case 's':
		    if(argc >= 2)
		    {
			argc--; argv++;
			blocks = atol(argv[0]);
		    }
		    break;
		case 'm':
		    mkfs = TRUE;
		    break;
		case 'r':
		    if(argc >= 2)
		    {
			argc--; argv++;
			reserved = atol(argv[0]);
		    }
		    break;
		case '?':
		    fprintf(stderr, "usage: %s [-f DEVICE-IMAGE] [-s BLOCKS] [-m [-r RESERVED-BLOCKS]]\n", prog_name);
		    return 1;
		default:
		    fprintf(stderr, "test_fs: unknown option: %s", *argv);
		    return 5;
		}
	    }
	    argv++; argc--;
	}
	fs_init();
	add_fs_commands();
	if(!open_test_dev(file, blocks, mkfs, reserved))
	    return 5;
#endif /* TEST_FS */
	init_shell_struct(&test_shell);
	shell_loop(&test_shell);
#ifdef TEST_FS
	fs_kill();
#endif
	return 0;
    }
    return 5;
}
