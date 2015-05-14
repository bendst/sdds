#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include "linux_sdds_impl.h"
#include "beta-ds.h"
#include "BuiltinTopic.h"

int main()
{
    pthread_t t;
	sDDS_init();
    Log_setLvl(0);

    for (;;) {
        do
		{
			//static uint8_t foo = 67;
            DDS_DCPSParticipant p;
            p.key = 22;
           
            DDS_DCPSTopic t;
            memset(t.name, 0, DDS_TOPIC_NAME_SIZE);
            memset(t.type_name, 0, DDS_TOPIC_TYPE_SIZE);
            t.key = 33;
            t.name[0] = 'T';
            t.name[1] = 'e';
            t.name[2] = 'm';
            t.name[3] = 'p';
            t.name[4] = '1';
            t.name[5] = '2';
            t.name[6] = '3';
            t.name[7] = '\0';
            t.type_name[0] = '4';
            t.type_name[1] = '5';
            t.type_name[2] = '\0';

            Beta b;
            b.value = 44;
            memset(b.value2, 0, 11);
            memset(b.value3, 0, 10);
            b.value2[0] = 'D';
            b.value2[1] = 'e';
            b.value2[2] = 'r';
            b.value2[3] = '\0';
            b.value3[0] = 'B';
            b.value3[1] = 'a';
            b.value3[2] = 'u';
            b.value3[3] = 'm';
            b.value3[4] = '!';
            b.value3[5] = '\0';


			if (DDS_DCPSParticipantDataWriter_write(g_DCPSParticipant_writer, &p, NULL) != DDS_RETCODE_OK) {
				// handle error
				//printf("-----DEBUG-----\t %s:%d\n", __FILE__, __LINE__);
			}
            if (DDS_DCPSTopicDataWriter_write(g_DCPSTopic_writer, &t, NULL) != DDS_RETCODE_OK) {
                // handle error
            	//printf("-----DEBUG-----\t %s:%d\n", __FILE__, __LINE__);
            }
            if (DDS_BetaDataWriter_write(g_Beta_writer, &b, NULL) != DDS_RETCODE_OK) {
                // handle error
            	//printf("-----DEBUG-----\t %s:%d\n", __FILE__, __LINE__);
            }
            sleep(10);

		} while(0);

    }

}
