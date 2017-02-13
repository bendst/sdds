#include <unistd.h>
#include "riot_mcast1_sdds_impl.h"
#include "unistd.h"

int main()
{
	DDS_ReturnCode_t ret;

	if (sDDS_init() == SDDS_RT_FAIL) {
		return 1;
	}
	Log_setLvl(4);  // Disable logs, set to 0 for to see everything.

    static Alpha alpha_pub;
    alpha_pub.pkey = 0;
    strncpy (alpha_pub.value2,   "Es gibt im", 10);
    alpha_pub.value3 = 1;
    alpha_pub.device = NodeConfig_getNodeID(); 

    fprintf(stderr, "DeviceID: %x\n", NodeConfig_getNodeID());

    for (;;) {
        ret = DDS_AlphaDataWriter_write (g_Alpha_writer, &alpha_pub, NULL);
        if (ret != DDS_RETCODE_OK) {
            fprintf (stderr,"Failed to send an alpha sample\n");
        }
        else {
            fprintf (stderr,"Send an alpha sample\n");
        }

        alpha_pub.pkey++;
        sleep (1);
    }

    return 0;
}
