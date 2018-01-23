#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <re.h>
#include <baresip.h>



static int module_init(void)
{
    printf("songxiuhe init \n");
}
static int module_close(void)
{
    printf("close \n");
}




const struct mod_export DECL_EXPORTS(local_socket) = {
	"local_socket",
	"ui",
	module_init,
	module_close
};
