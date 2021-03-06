/*  =========================================================================
    DataReader - DDS DataReader representation, allows the application to
                 declare the data it wishes to receive.

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of sDDS:
    http://wwwvs.cs.hs-rm.de.
    =========================================================================
 */

#include "sDDS.h"
#include "Log.h"

#ifdef TEST_SCALABILITY_LINUX
#include <stdio.h>
static FILE* scalability_msg_count;
#endif

static Task sendTask;
static Mutex_t* mutex;

//  Internal helper functions
void
checkSendingWrapper(void* buf);

rc_t
DataWriter_init() {
    if (TimeMng_init() != SDDS_RT_OK) {
        return SDDS_RT_FAIL;
    }
    ssw_rc_t ret;
#ifdef SDDS_QOS_LATENCYBUDGET
    sendTask = Task_create();
    if (sendTask == NULL) {
        Log_error("Task_create failed\n");
        return SDDS_RT_FAIL;
    }
    ret = Task_init(sendTask, checkSendingWrapper, NULL);
    if (ret == SDDS_SSW_RT_FAIL) {
        Log_error("Task_init failed\n");
        return SDDS_RT_FAIL;
    }
#endif

    mutex = Mutex_create();
    if (mutex == NULL) {
        Log_error("Mutex_create failed\n");
        return SDDS_RT_FAIL;
    }
    ret = Mutex_init(mutex);
    if (ret == SDDS_SSW_RT_FAIL) {
        Log_error("Mutex_init failed\n");
        return SDDS_RT_FAIL;
    }

#ifdef TEST_SCALABILITY_LINUX
    scalability_msg_count = fopen(SCALABILITY_LOG, "w");
    fclose(scalability_msg_count);
#endif

    return SDDS_RT_OK;
}

#if defined (SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK) || defined (SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK)
rc_t
DataWriter_setup(Reliable_DataWriter_t* self, ReliableSample_t* samples, unsigned int depth)
{
    assert(self);
    assert(samples);
    self->samplesToKeep = samples;
    self->depthToKeep = depth;

    for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++){
        ((Reliable_DataWriter_t*) self)->samplesToKeep[index].seqNr = 0;
        ((Reliable_DataWriter_t*) self)->samplesToKeep[index].timeStamp = 0;
        ((Reliable_DataWriter_t*) self)->samplesToKeep[index].isUsed = 0;
    }
}
#endif

#if defined(SDDS_TOPIC_HAS_SUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED) \
 || defined(SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK) \
 || defined(SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK)
rc_t
DataWriter_write(DataWriter_t* self, Data data, void* handle) {

    assert (self);
    (void) handle;
#   ifdef FEATURE_SDDS_TRACING_ENABLED
#       if defined (FEATURE_SDDS_TRACING_SEND_NORMAL) || defined (FEATURE_SDDS_TRACING_SEND_ISOLATED)
#           ifdef FEATURE_SDDS_TRACING_CALL_WRITE
    Trace_point(SDDS_TRACE_EVENT_CALL_WRITE);
#           endif
#       endif
#   endif

    Mutex_lock(mutex);
    Topic_t* topic = self->topic;
    List_t* subscribers = topic->dsinks.list;
    NetBuffRef_t* out_buffer = find_free_buffer(subscribers);
    TopicSubscription_t* tSub = (TopicSubscription_t*) subscribers->first_fn(subscribers);
    while (tSub) {
        if (tSub->state == ACTIVE) {
            if (Locator_contains(out_buffer->locators, tSub->addr) != SDDS_RT_OK) {
                if (out_buffer->locators->add_fn(out_buffer->locators, tSub->addr) == SDDS_RT_OK) {
                    Locator_upRef(tSub->addr);
                }
            }
        }
        tSub = (TopicSubscription_t*) subscribers->next_fn(subscribers);
    }
    rc_t ret;

#   ifdef SDDS_QOS_LATENCYBUDGET
    //  If new deadline is earlier
    if ((out_buffer->sendDeadline == 0)) {
#       if SDDS_QOS_DW_LATBUD < 65536
        ret = Time_getTime16(&out_buffer->sendDeadline);
#       else
        ret = Time_getTime32(&out_buffer->sendDeadline);
#       endif
        out_buffer->sendDeadline += self->qos.latBudDuration;
        out_buffer->latBudDuration = self->qos.latBudDuration;
        Log_debug("sendDeadline: %u\n", out_buffer->sendDeadline);
    }
#   endif

#   ifdef FEATURE_SDDS_TRACING_ENABLED
#       if defined (FEATURE_SDDS_TRACING_SEND_NORMAL) || defined (FEATURE_SDDS_TRACING_SEND_ISOLATED)
#           ifdef FEATURE_SDDS_TRACING_PREPARE_SNPS
    Trace_point(SDDS_TRACE_EVENT_PREPARE_SNPS);
#           endif
#       endif
#   endif

    domainid_t domain = topic->domain;
    if (out_buffer->curDomain != domain) {
        SNPS_writeDomain(out_buffer, domain);
        out_buffer->curDomain = domain;
    }

    if (out_buffer->curTopic != topic) {
        SNPS_writeTopic(out_buffer, topic->id);
        out_buffer->curTopic = topic;
    }

#   ifdef SDDS_HAS_QOS_RELIABILITY

    if (self->topic->reliabilityKind > 0){ // relevant topic for qos reliability

        Reliable_DataWriter_t* dw_reliable_p = (Reliable_DataWriter_t*)self;
        bool_t newSampleHasSlot = 0;


#       if defined (SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK) || defined (SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK)
        if (self->topic->reliabilityKind == SDDS_QOS_RELIABILITY_KIND_RELIABLE) { // relevant topic has qos reliability_reliable

            // check, if sample is already in acknowledgement-list
            bool_t isAlreadyInQueue = 0;


            for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++) {
                if ((dw_reliable_p->samplesToKeep[index].isUsed == 1)
                &&  (dw_reliable_p->samplesToKeep[index].seqNr == dw_reliable_p->seqNr) ) {
                    isAlreadyInQueue = 1;
                    break;
                }
            }

            if (isAlreadyInQueue == 0) {
                // add current data of sample to acknowledgement-queue for possible re-sending
                time32_t currentTime = 0;
                Time_getTime32(&currentTime);

                // find free/replaceable slot in acknowledgement-list
                // if no slot can be found, drop new data
                for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++) {
                    if(dw_reliable_p->samplesToKeep[index].isUsed == 0
                    || (dw_reliable_p->samplesToKeep[index].timeStamp + self->topic->max_blocking_time) < currentTime ) {

                        dw_reliable_p->samplesToKeep[index].isUsed = 1;
                        topic->Data_cpy(dw_reliable_p->samplesToKeep[index].data, data);
                        dw_reliable_p->samplesToKeep[index].seqNr = dw_reliable_p->seqNr;
                        dw_reliable_p->samplesToKeep[index].timeStamp = currentTime;
                        newSampleHasSlot = 1;
                        break;
                    }
                }
            }


#           if defined (SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK)
            if (self->topic->confirmationtype == SDDS_QOS_RELIABILITY_CONFIRMATIONTYPE_ACK) {
            // send every sample which is not yet acknowledged
                for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++) {
                    if(dw_reliable_p->samplesToKeep[index].isUsed != 0) {

                        if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC) {
                            SNPS_writeSeqNr(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);

#               if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
                        } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL) {
                            SNPS_writeSeqNrSmall(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#               endif
#               if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
                        } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG) {
                            SNPS_writeSeqNrBig(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#               endif
#               if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
                        } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE) {
                            SNPS_writeSeqNrHUGE(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#               endif
                        }

                        if (SNPS_writeData(out_buffer, topic->Data_encode, dw_reliable_p->samplesToKeep[index].data) != SDDS_RT_OK) {
                            Log_error("(%d) SNPS_writeData failed\n", __LINE__);
#               ifdef SDDS_QOS_LATENCYBUDGET
                            out_buffer->bufferOverflow = true;
#               endif
                        }

                    }
                } // end of for all samples in samplesToKeep
            } // end of confirmationtype == SDDS_QOS_RELIABILITY_CONFIRMATIONTYPE_ACK
#           endif // end of ACK

        } // end of relevant topic has qos reliability_reliable
#       endif // end of if SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK || NACK

#       ifdef SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK
        if((self->topic->reliabilityKind == SDDS_QOS_RELIABILITY_KIND_BESTEFFORT)
        || (self->topic->confirmationtype == SDDS_QOS_RELIABILITY_CONFIRMATIONTYPE_NACK && newSampleHasSlot) )
#       else
        if (self->topic->reliabilityKind == SDDS_QOS_RELIABILITY_KIND_BESTEFFORT)
#       endif
        {

            if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC){
                SNPS_writeSeqNr(out_buffer, dw_reliable_p->seqNr);
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL){
                SNPS_writeSeqNrSmall(out_buffer, dw_reliable_p->seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG){
                SNPS_writeSeqNrBig(out_buffer, dw_reliable_p->seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE){
                SNPS_writeSeqNrHUGE(out_buffer, dw_reliable_p->seqNr);
#       endif
            }

        }


        if (self->topic->reliabilityKind == SDDS_QOS_RELIABILITY_KIND_RELIABLE ) {
            if (newSampleHasSlot) {
                if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC) {
                    dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0x0F;
                } else if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL){
                    dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0xFF;
                } else if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG){
                    dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0xFFFF;
                }else{
                    dw_reliable_p->seqNr++;
                }
            }
        } else {
            if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC){
                dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0x0F;
            } else if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL){
                dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0xFF;
            } else if (self->topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG){
                dw_reliable_p->seqNr = (dw_reliable_p->seqNr + 1) & 0xFFFF;
            }else{
                dw_reliable_p->seqNr++;
            }
        }

    } // end of topic is relevant for reliability

#   endif // end if SDDS_HAS_QOS_RELIABILITY

#   ifdef SDDS_HAS_QOS_RELIABILITY
#       ifdef SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK
        if (self->topic->confirmationtype != SDDS_QOS_RELIABILITY_CONFIRMATIONTYPE_ACK)
#       else
        if (self->topic->reliabilityKind != SDDS_QOS_RELIABILITY_KIND_RELIABLE)
#       endif
        {
#   endif
#   ifdef FEATURE_SDDS_SECURITY_ENABLED
            if(topic->protection) {
              if (SNPS_writeSecureData(out_buffer, topic, data) != SDDS_RT_OK) {
              	Log_error("(%d) SNPS_writeSecureData failed\n", __LINE__);
              }      
            } else {
#   endif
              ret = SNPS_writeData(out_buffer, topic->Data_encode, data);
              if (ret != SDDS_RT_OK) {
                  Log_error("(%d) SNPS_writeData failed\n", __LINE__);
#   ifdef SDDS_QOS_LATENCYBUDGET
                      if (ret == SDDS_RT_FAIL) {
                          out_buffer->bufferOverflow = true;
                      }
#   endif
              }
#   ifdef FEATURE_SDDS_SECURITY_ENABLED
            }
#   endif
#   ifdef SDDS_HAS_QOS_RELIABILITY
        }
#   endif

    Log_debug("writing to domain %d and topic %d \n", topic->domain, topic->id);

    ret = checkSending(out_buffer);

#   ifdef TEST_SCALABILITY_LINUX
    if (ret != SDDS_RT_NO_SUB && ret != SDDS_RT_FAIL) {
        scalability_msg_count = fopen(SCALABILITY_LOG, "a");
        fwrite("D", 1, 1, scalability_msg_count);
        fclose(scalability_msg_count);
    }
#   endif

#   ifdef TEST_SCALABILITY_RIOT
    if (ret != SDDS_RT_NO_SUB && ret != SDDS_RT_FAIL) {
//        fprintf(stderr,"{SCL:D}\n");
    }
    else if (ret == SDDS_RT_NO_SUB) {
        Log_debug("No Subscroption\n");
    }
    else if (ret == SDDS_RT_FAIL) {
        Log_debug("Send failed\n");
    }
    else {
        Log_debug("ret: %d\n", ret);
    }
#   endif
#   ifdef TEST_MSG_COUNT_RIOT
    if (ret != SDDS_RT_NO_SUB && ret != SDDS_RT_FAIL) {
        fprintf(stderr,"MSG_COUNT\n");
    }
    else if (ret == SDDS_RT_NO_SUB) {
        Log_debug("No Subscroption\n");
    }
#   endif

    Log_debug("writing to domain %d and topic %d \n", topic->domain, topic->id);

    Mutex_unlock(mutex);
    //  Caller doesn't understand different return codes but FAIL and OK
    /*
    if (ret == SDDS_RT_FAIL) {
        return SDDS_RT_FAIL;
    }
    return SDDS_RT_OK;
    */
    return ret;
} // end of DataWriter_write
#endif // end of SDDS_TOPIC_HAS_SUB

rc_t
DataWriter_mutex_lock() {
    if (mutex == NULL) {
        mutex = Mutex_create();
        if (mutex == NULL) {
            return SDDS_RT_FAIL;
        }
    }
    Mutex_lock(mutex);
    return SDDS_RT_OK;
}

rc_t
DataWriter_mutex_unlock() {
    if (mutex == NULL) {
        mutex = Mutex_create();
        if (mutex == NULL) {
            return SDDS_RT_FAIL;
        }
    }
    Mutex_unlock(mutex);
    return SDDS_RT_OK;
}

#ifdef FEATURE_SDDS_BUILTIN_TOPICS_ENABLED
rc_t
DataWriter_writeAddress(DataWriter_t* self,
                        castType_t cast_type,
                        addrType_t addr_type,
                        uint8_t* addr,
                        uint8_t addr_len) {
    assert (self);
    Mutex_lock(mutex);
    Topic_t* topic = self->topic;
    domainid_t domain = topic->domain;
    List_t* subscribers = topic->dsinks.list;

    NetBuffRef_t* out_buffer = find_free_buffer(subscribers);
    TopicSubscription_t* tSub = (TopicSubscription_t*) subscribers->first_fn(subscribers);
    while (tSub) {
        if (Locator_contains(out_buffer->locators, tSub->addr) != SDDS_RT_OK) {
            if (out_buffer->locators->add_fn(out_buffer->locators, tSub->addr) == SDDS_RT_OK) {
                Locator_upRef(tSub->addr);
            }
        }
        tSub = (TopicSubscription_t*) subscribers->next_fn(subscribers);
    }

    if (out_buffer->curDomain != domain) {
        SNPS_writeDomain(out_buffer, domain);
        out_buffer->curDomain = domain;
    }
    if (out_buffer->curTopic != topic) {
        SNPS_writeTopic(out_buffer, topic->id);
        out_buffer->curTopic = topic;
    }

    rc_t ret;
    ret = SNPS_writeAddress(out_buffer, cast_type, addr_type, addr, addr_len);
    Mutex_unlock(mutex);
    if (ret != SDDS_RT_OK) {
        // something went wrong oO
        Log_error("Couldn't write to address\n");
        return SDDS_RT_FAIL;
    }
    Log_debug("Writing to domain %d and topic %d \n", topic->domain, topic->id);

    return SDDS_RT_OK;
}
#endif //FEATURE_SDDS_BUILTIN_TOPICS_ENABLED

void
checkSendingWrapper(void* buf) {
    Mutex_lock(mutex);
    checkSending((NetBuffRef_t*) buf);
    Mutex_unlock(mutex);
}

#ifdef SDDS_QOS_LATENCYBUDGET
rc_t
checkSending(NetBuffRef_t* buf) {
#   if SDDS_QOS_DW_LATBUD < 65536
    time16_t time;
    Time_getTime16(&time);
    time16_t sendDeadline;
    time16_t latBudDuration;
#   else
    time32_t time;
    Time_getTime32(&time);
    time32_t sendDeadline;
    time32_t latBudDuration;
#   endif

    sendDeadline = buf->sendDeadline;
    latBudDuration = buf->latBudDuration;
    bool_t overflow = buf->bufferOverflow;

    if (((time >= sendDeadline) && (time - sendDeadline < latBudDuration)) || overflow) {
        Task_stop(sendTask);
        if (overflow) {
            Log_warn("Send Data ahead of deadline due to buffer overflow.\n");
        }
        Log_debug("time: %u, deadline: %u\n", time, sendDeadline);
        // update header
        SNPS_updateHeader(buf);

        if (buf->locators->size_fn(buf->locators) > 0) {
            rc_t ret = Network_send(buf);
            if (ret != SDDS_RT_OK) {
                Log_error("Network_send failed\n");
                return SDDS_RT_FAIL;
            }
            //  If frame was sent, free the buffer.
            NetBuffRef_renew(buf);
        }
        //  If latencyBudget is active, don't discard the buffer right away.
        //  Discard the buffer, if the buffer is full and no one there to send.
        else if (overflow) {
            NetBuffRef_renew(buf);
            Log_debug("Buffer full\n");
        }
        //  If the buffer is not full yet, just reset the deadline.
        else {
            buf->sendDeadline = 0;
            Log_debug("No subscriber\n");
        }

        return SDDS_RT_OK;
    }
    else {
        Task_stop(sendTask);
        Task_setData(sendTask, (void*) buf);
#   if SDDS_QOS_DW_LATBUD < 65536
        msecu16_t taskTime;
        uint16_t taskSec;
        msecu16_t taskMSec;
#   else
        msecu32_t taskTime;
        uint32_t taskSec;
        msecu32_t taskMSec;
#   endif
        taskTime = (sendDeadline - time);
        taskSec = taskTime / 1000;
        taskMSec = taskTime % 1000;
        ssw_rc_t ret = Task_start(sendTask, taskSec, taskMSec, SDDS_SSW_TaskMode_single);
        if (ret != SDDS_RT_OK) {
            Log_error("Task_start failed\n");
            return SDDS_RT_FAIL;
        }

#   ifdef UTILS_DEBUG
        Log_debug("Task startet, timer: %u\n", (sendDeadline - time));
        Log_debug("%u > %u\n", sendDeadline, time);
#   endif
        return SDDS_RT_DEFERRED;
    }
}
#else // ! SDDS_QOS_LATENCYBUDGET
rc_t
checkSending(NetBuffRef_t* buf) {
    // update header
    SNPS_updateHeader(buf);
    rc_t ret = SDDS_RT_OK;

    if (buf->locators->size_fn(buf->locators) > 0) {


        ret = Network_send(buf);
        if (ret != SDDS_RT_OK) {
            Log_error("Network_send failed\n");
            NetBuffRef_renew(buf);
            return SDDS_RT_FAIL;
        }
    }
#   if defined(TEST_SCALABILITY_LINUX) || defined(TEST_SCALABILITY_RIOT) || defined(TEST_MSG_COUNT_RIOT)
    else {
        ret = SDDS_RT_NO_SUB;
    }
#   endif
    NetBuffRef_renew(buf);

    return ret;
}
#endif //SDDS_QOS_LATENCYBUDGET
