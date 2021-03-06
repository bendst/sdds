/*
 * =====================================================================================
 *
 *       Filename:  DataSink.c
 *
 *    Description:  Impl file for the DataSink class
 *
 *        Version:  1.0
 *        Created:  03.03.2010 08:57:23
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Kai Beckmann (kai), kai-oliver.beckmann@hs-rm.de
 *        Company:  Hochschule RheinMain - DOPSY Labor für verteilte Systeme
 *
 * =====================================================================================
 */

#include "sDDS.h"
#include <os-ssal/Trace.h>
#include "FilteredDataReader.h"

#ifdef FEATURE_SDDS_BUILTIN_TOPICS_ENABLED
// participant ID is defined in src/BuiltinTopic.c
extern SSW_NodeID_t BuiltinTopic_participantID;
#endif



struct _DataSink_t {
    DataReader_t readers[SDDS_DATA_READER_MAX_OBJS];
    uint64_t allocated_readers;

#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
    FilteredDataReader_t filteredReaders[SDDS_DATA_FILTER_READER_MAX_OBJS];
	uint64_t allocated_filteredReaders;
#endif
    SNPS_Address_t addr;
};
static DataSink_t _dataSink;
static DataSink_t* self = &_dataSink;

//  Forward declarations of internal helper functions

rc_t
checkDomain(NetBuffRef_t* buff);
rc_t
checkTopic(NetBuffRef_t* buff, topicid_t topic);
rc_t
BuiltinTopicDataReader_encode(NetBuffRef_t* buff, Data data, size_t* size);


//  ---------------------------------------------------------------------------
//  Initialize this class

rc_t
DataSink_init(void) {
    return SDDS_RT_OK;
}

FilteredDataReader_t*
DataSink_getFilteredDataReaders() {
#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
    return self->filteredReaders;
#endif
    return NULL;
}

#if defined(SDDS_TOPIC_HAS_PUB) || \
    defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED) || \
    defined(SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK) || \
    defined(SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK)
rc_t
DataSink_getTopic(DDS_DCPSSubscription* st, topicid_t id, Topic_t** topic) {
    uint8_t index;
    for (index = 0; index < SDDS_DATA_READER_MAX_OBJS; index++) {
        if (  BitArray_check(&self->allocated_readers, index)
           &&  DataReader_topic(&self->readers[index])->id == id) {
            if (st != NULL) {
                st->key = DataReader_id(&self->readers[index]);
#   ifdef FEATURE_SDDS_BUILTIN_TOPICS_ENABLED
                st->participant_key = BuiltinTopic_participantID;
#   endif
                st->topic_id = DataReader_topic(&self->readers[index])->id;
            }
            if (topic != NULL) {
                *topic = DataReader_topic(&self->readers[index]);
            }
            return SDDS_RT_OK;
        }
    }
    Log_debug("No DataReader found that listens for topics with id %u\n", id);
    return SDDS_RT_FAIL;
}
#endif


DataReader_t*
DataSink_DataReader_by_topic(topicid_t id) {
    uint8_t index;
    for (index = 0; index < SDDS_DATA_READER_MAX_OBJS; index++) {
        if (  BitArray_check(&self->allocated_readers, index)
           &&  DataReader_topic(&self->readers[index])->id == id) {
            return &self->readers[index];
        }
    }
    return NULL;
}

rc_t
DataSink_getAddr(SNPS_Address_t* address) {
    address->addrType = self->addr.addrType;
    address->addrCast = self->addr.addrCast;
    address->addr = self->addr.addr;
    return SDDS_RT_OK;
}

//  ---------------------------------------------------------------------------
//  Processes a SNPS message by reading through all submessages. Will enqueue a
//  Sample into a DataReader's History. Returns OK if successful, otherwise
//  FAIL.
rc_t
DataSink_processFrame(NetBuffRef_t* buff) {
    assert(buff);


#ifdef FEATURE_SDDS_TRACING_ENABLED
#   if defined (FEATURE_SDDS_TRACING_RECV_NORMAL) || defined (FEATURE_SDDS_TRACING_RECV_ISOLATED)
#       ifdef FEATURE_SDDS_TRACING_PROCESS_FRAME
    Trace_point(SDDS_TRACE_EVENT_PROCESS_FRAME);
#       endif
#   endif
#endif
    //  Parse the header
    rc_t ret;
    ret = SNPS_readHeader(buff);
    if (ret == SDDS_RT_FAIL) {
        Log_error("Invalid SNPS header\n");
        return ret;
    }

    topicid_t topic_id = 0;

    while (buff->subMsgCount > 0) {


        subMsg_t type;
        SNPS_evalSubMsg(buff, &type);

        switch (type) {


        case (SDDS_SNPS_T_DOMAIN):
            ret = checkDomain(buff);
            if (ret == SDDS_RT_FAIL) {
                Log_error("Invalid domain!\n");
                return ret;
            }
            break;


        case (SDDS_SNPS_T_TOPIC):
            ret = SNPS_readTopic(buff, &topic_id);
            if (ret == SDDS_RT_FAIL) {
                Log_error("Can't read Topic\n");
                return ret;
            }
            Log_debug("Read topic %u\n", topic_id);
            checkTopic(buff, topic_id);
            break;
        case (SDDS_SNPS_T_ADDRESS):
            //  Write address into global variable
            if (SNPS_readAddress(buff, &self->addr.addrCast, &self->addr.addrType, &self->addr.addr) != SDDS_RT_OK) {
                Log_warn("Read address failed, discard subMessage.\n", type);
                SNPS_discardSubMsg(buff);
            }
            break;
            
        case (SDDS_SNPS_T_SECURE):
        case (SDDS_SNPS_T_DATA):
        {
#if defined(SDDS_TOPIC_HAS_PUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED) || defined(FEATURE_SDDS_MANAGEMENT_TOPIC_ENABLED)
                        DataSink_ReaderIterator_t it;
            rc_t it_ret = DataSink_readerIterator_reset(&it, topic_id);
            if (it_ret != SDDS_RT_OK) {
                Log_debug("Couĺdn't get Data Reader for topic id %u: "
                          "Discard submessage\n", topic_id);
                SNPS_discardSubMsg(buff);
                return SDDS_RT_FAIL;
            }

            Topic_t* topic = buff->curTopic;
            Locator_t* loc = (Locator_t*) buff->locators->first_fn(buff->locators);

            ret = SNPS_readData(buff, topic->Data_decode, (Data) topic->incomingSample.data);
            if (ret == SDDS_RT_FAIL) {
                SNPS_discardSubMsg(buff);
                return ret;
            }
            topic->incomingSample.instance = loc;

            while (DataSink_readerIterator_hasNext(&it) == SDDS_RT_OK) {
                DataReader_t* data_reader = DataSink_readerIterator_next(&it);
                ret = DataReader_pushData(data_reader, buff);
                if (ret != SDDS_RT_OK) {
                    Log_error("DataReader_pushData failed for readerID %d\n", data_reader->id);
                }
            }
#endif // end of defined(SDDS_TOPIC_HAS_PUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED)
        } // end of case SDDS_SNPS_T_DATA
        break;

#ifdef SDDS_HAS_QOS_RELIABILITY
        case (SDDS_SNPS_T_SEQNR):

            SNPS_readSeqNr(buff, (uint8_t*) &buff->curTopic->incomingSample.seqNr);
            break;


#   if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
        case (SDDS_SNPS_T_SEQNRSMALL):
            SNPS_readSeqNrSmall(buff, (uint8_t*) &buff->curTopic->incomingSample.seqNr);
            break;
#   endif


#   if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
        case (SDDS_SNPS_T_SEQNRBIG):
            SNPS_readSeqNrBig(buff, (uint16_t*) &buff->curTopic->incomingSample.seqNr);
            break;
#   endif


#   if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
        case (SDDS_SNPS_T_SEQNRHUGE):
            SNPS_readSeqNrHUGE(buff, (uint32_t*) &buff->curTopic->incomingSample.seqNr);
            break;
#   endif


#   if defined SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK
        case (SDDS_SNPS_T_ACKSEQ): {
            SNPS_readAckSeq(buff, (uint8_t*)&buff->curTopic->incomingSample.seqNr );
            Reliable_DataWriter_t* dw_reliable_p = DataSource_DataWriter_by_topic(topic_id);

            for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++){
                if (dw_reliable_p->samplesToKeep[index].seqNr == buff->curTopic->incomingSample.seqNr
                && dw_reliable_p->samplesToKeep[index].isUsed != 0) {
                    dw_reliable_p->samplesToKeep[index].isUsed = 0;
                    dw_reliable_p->samplesToKeep[index].timeStamp = 0;
                    dw_reliable_p->samplesToKeep[index].seqNr = 0;
                    break;
                }
            }

            break;
        }


#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
        case (SDDS_SNPS_T_ACK): {
            Topic_t* topic = TopicDB_getTopic(topic_id);
            SNPS_readAck(buff);
            if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL){
                SNPS_readSeqNrSmall(buff, (uint8_t*)&buff->curTopic->incomingSample.seqNr);
#           if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG){
                SNPS_readSeqNrBig(buff, (uint16_t*)&buff->curTopic->incomingSample.seqNr);
#           endif
#           if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE){
                SNPS_readSeqNrHUGE(buff, (uint32_t*)&buff->curTopic->incomingSample.seqNr);
#           endif
            }


            Reliable_DataWriter_t* dw_reliable_p = DataSource_DataWriter_by_topic(topic_id);
            for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++){
                if(dw_reliable_p->samplesToKeep[index].seqNr == buff->curTopic->incomingSample.seqNr
                && dw_reliable_p->samplesToKeep[index].isUsed != 0) {
                    dw_reliable_p->samplesToKeep[index].isUsed = 0;
                    dw_reliable_p->samplesToKeep[index].timeStamp = 0;
                    dw_reliable_p->samplesToKeep[index].seqNr = 0;
                    break;
                }
            }

            break;
        }
#       endif // end of extended qos reliability submsg
#   endif // end of SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_ACK

#   if defined SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK
        case (SDDS_SNPS_T_NACKSEQ): {

            SNPS_readNackSeq(buff, (uint8_t*)&buff->curTopic->incomingSample.seqNr );
            Topic_t* topic = TopicDB_getTopic(topic_id);
            domainid_t domain = topic->domain;
            Locator_t* loc = (Locator_t*) buff->locators->first_fn(buff->locators);
            List_t* subscribers = topic->dsinks.list;
            Reliable_DataWriter_t* dw_reliable_p = DataSource_DataWriter_by_topic(topic_id);

            DataWriter_mutex_lock();
            Topic_addRemoteDataSink(topic, loc, 0, ACTIVE);
            Locator_upRef(loc);

            for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++){
                if (dw_reliable_p->samplesToKeep[index].seqNr == buff->curTopic->incomingSample.seqNr
                && dw_reliable_p->samplesToKeep[index].isUsed) {

                    NetBuffRef_t* out_buffer = find_free_buffer(subscribers);
                    if (out_buffer->curDomain != domain) {
                        SNPS_writeDomain(out_buffer, domain);
                        out_buffer->curDomain = domain;
                    }

                    if (out_buffer->curTopic != topic) {
                        SNPS_writeTopic(out_buffer, topic->id);
                        out_buffer->curTopic = topic;
                    }

                    if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC) {
                        SNPS_writeSeqNr(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);

#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL) {
                        SNPS_writeSeqNrSmall(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG) {
                        SNPS_writeSeqNrBig(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE) {
                        SNPS_writeSeqNrHUGE(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
                    }

                    if (SNPS_writeData(out_buffer, topic->Data_encode, dw_reliable_p->samplesToKeep[index].data) != SDDS_RT_OK) {
                        Log_error("(%d) SNPS_writeData failed\n", __LINE__);
#       ifdef SDDS_QOS_LATENCYBUDGET
                        out_buffer->bufferOverflow = true;
#       endif
                    }

#       ifdef SDDS_QOS_LATENCYBUDGET
#           if SDDS_QOS_DW_LATBUD < 65536
                    ret = Time_getTime16(&out_buffer->sendDeadline);
#           else
                    ret = Time_getTime32(&out_buffer->sendDeadline);
#           endif
                    out_buffer->latBudDuration = self->qos.latBudDuration;
                    Log_debug("sendDeadline: %u\n", out_buffer->sendDeadline);
#       endif

                    checkSending(out_buffer);
                    DataWriter_mutex_unlock();
                    break;
                }
            }

            break;
        }


#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
        case (SDDS_SNPS_T_NACK): {

            SNPS_readNack(buff);
            Topic_t* topic = TopicDB_getTopic(topic_id);
            domainid_t domain = topic->domain;
            Locator_t* loc = (Locator_t*) buff->locators->first_fn(buff->locators);
            List_t* subscribers = topic->dsinks.list;
            Reliable_DataWriter_t* dw_reliable_p = DataSource_DataWriter_by_topic(topic_id);

            DataWriter_mutex_lock();
            Topic_addRemoteDataSink(topic, loc, 0, ACTIVE);
            Locator_upRef(loc);

            if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL){
                SNPS_readSeqNrSmall(buff, (uint8_t*)&buff->curTopic->incomingSample.seqNr);
#           if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG){
                SNPS_readSeqNrBig(buff, (uint16_t*)&buff->curTopic->incomingSample.seqNr);
#           endif
#           if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
            } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE){
                SNPS_readSeqNrHUGE(buff, (uint32_t*)&buff->curTopic->incomingSample.seqNr);
#           endif
            }


            for (int index = 0; index < SDDS_QOS_RELIABILITY_RELIABLE_SAMPLES_SIZE; index++){
                if (dw_reliable_p->samplesToKeep[index].seqNr == buff->curTopic->incomingSample.seqNr
                && dw_reliable_p->samplesToKeep[index].isUsed) {

                    NetBuffRef_t* out_buffer = find_free_buffer(subscribers);
                    if (out_buffer->curDomain != domain) {
                        SNPS_writeDomain(out_buffer, domain);
                        out_buffer->curDomain = domain;
                    }

                    if (out_buffer->curTopic != topic) {
                        SNPS_writeTopic(out_buffer, topic->id);
                        out_buffer->curTopic = topic;
                    }

                    if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BASIC) {
                        SNPS_writeSeqNr(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);

#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_SMALL
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_SMALL) {
                        SNPS_writeSeqNrSmall(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE >= SDDS_QOS_RELIABILITY_SEQSIZE_BIG
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_BIG) {
                        SNPS_writeSeqNrBig(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
#       if SDDS_SEQNR_BIGGEST_TYPE_BITSIZE == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE
                    } else if (topic->seqNrBitSize == SDDS_QOS_RELIABILITY_SEQSIZE_HUGE) {
                        SNPS_writeSeqNrHUGE(out_buffer, dw_reliable_p->samplesToKeep[index].seqNr);
#       endif
                    }

                    if (SNPS_writeData(out_buffer, topic->Data_encode, dw_reliable_p->samplesToKeep[index].data) != SDDS_RT_OK) {
                        Log_error("(%d) SNPS_writeData failed\n", __LINE__);
#       ifdef SDDS_QOS_LATENCYBUDGET
                        out_buffer->bufferOverflow = true;
#       endif
                    }

#       ifdef SDDS_QOS_LATENCYBUDGET
#           if SDDS_QOS_DW_LATBUD < 65536
                    ret = Time_getTime16(&out_buffer->sendDeadline);
#           else
                    ret = Time_getTime32(&out_buffer->sendDeadline);
#           endif
                    out_buffer->latBudDuration = self->qos.latBudDuration;
                    Log_debug("sendDeadline: %u\n", out_buffer->sendDeadline);
#       endif

                    checkSending(out_buffer);
                    DataWriter_mutex_unlock();
                    break;
                }
            }


            break;
        }
#       endif // end of extended qos reliability submsg
#   endif // end of SDDS_HAS_QOS_RELIABILITY_KIND_RELIABLE_NACK

#endif // end of SDDS_HAS_QOS_RELIABILITY
        default:
            //  Go to next submessage
            Log_warn("Invalid submessage type %u: Discard submessage\n", type);
            SNPS_discardSubMsg(buff);
            break;
        }
    }
    // Reset the buffer
//    NetBuffRef_renew(buff);

#if defined(SDDS_TOPIC_HAS_PUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED)
    // Send event notifications
    uint8_t index;
    for (index = 0; index < SDDS_DATA_READER_MAX_OBJS; index++) {
        DataReader_t* data_reader = &self->readers[index];
        if (!data_reader || !data_reader->topic) {
            continue;
        }
        int tpc = DataReader_topic(data_reader)->id;
        if ((topic_id == tpc) && DataReader_on_data_avail_listener(data_reader)) {
            On_Data_Avail_Listener on_data_avail_listener =
                DataReader_on_data_avail_listener(data_reader);
            //  Notify listener
            on_data_avail_listener(data_reader);
        }
    }

#   ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
    for (index = 0; index < SDDS_DATA_FILTER_READER_MAX_OBJS; index++) {
        DataReader_t* data_reader = (DataReader_t*)&self->filteredReaders[index];
        if (!data_reader || !data_reader->topic) {
            continue;
        }
        int tpc = DataReader_topic(data_reader)->id;
        if ((topic_id == tpc) && DataReader_on_data_avail_listener(data_reader)) {
            On_Data_Avail_Listener on_data_avail_listener =
                DataReader_on_data_avail_listener(data_reader);
            //  Notify listener
            on_data_avail_listener(data_reader);
        }
    }
#   endif
#endif
    return SDDS_RT_OK;
}


#if defined(SDDS_TOPIC_HAS_PUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED)
//  ---------------------------------------------------------------------------
//  Creates a new DataReader object

DataReader_t*
DataSink_create_datareader(Topic_t* topic, Qos qos, Listener listener, StatusMask sm) {
    (void) qos;
    (void) sm;

    uint8_t index;
    for (index = 0; index < SDDS_DATA_READER_MAX_OBJS; index++) {
        //  Check if object at index has been allocated
        if (!BitArray_check(&self->allocated_readers, index)) {
            //  Allocate object at index
            BitArray_set(&self->allocated_readers, index);
            DataReader_t* reader = &self->readers[index];
            // Initialize object properties
            DataReader_init(reader, index, topic, listener);
            Log_debug("Create data reader with id %u\n", DataReader_id(reader));
            return reader;
        }
    }
    return NULL;
}

FilteredDataReader_t*
DataSink_create_filteredDatareader(LocationFilteredTopic_t* topic, Qos qos, Listener listener, StatusMask sm) {
    (void) qos;
    (void) sm;

#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
    uint8_t index;
    for (index = 0; index < SDDS_DATA_FILTER_READER_MAX_OBJS; index++) {
        //  Check if object at index has been allocated
        if (!BitArray_check(&self->allocated_filteredReaders, index)) {
            //  Allocate object at index
            BitArray_set(&self->allocated_filteredReaders, index);
            FilteredDataReader_t* reader = &self->filteredReaders[index];
            // Initialize object properties
            FilteredDataReader_init(reader, index, topic, listener);
            Log_debug("Create filtered data reader with id %u\n", DataReader_id((DataReader_t*)reader));
            return reader;
        }
    }
#endif
    return NULL;
}

#endif


rc_t
checkDomain(NetBuffRef_t* buff) {
    domainid_t domain;
    SNPS_readDomain(buff, &domain);
    if (TopicDB_checkDomain(domain) == false) {
        SNPS_gotoNextSubMsg(buff, SDDS_SNPS_T_DOMAIN);
    }
    else {
        buff->curDomain = domain;
    }
    return SDDS_RT_OK;
}

rc_t
checkTopic(NetBuffRef_t* buff, topicid_t topic) {
    assert(buff);
    Topic_t* t_ptr = TopicDB_getTopic(topic);
    if (t_ptr == NULL) {
        SNPS_gotoNextSubMsg(buff, SDDS_SNPS_T_TOPIC);
    }
    else {
        buff->curTopic = t_ptr;
    }

    return SDDS_RT_OK;
}

// FIXME not used?
rc_t
BuiltinTopic_writeDataReaders2Buf(NetBuffRef_t* buf) {
    SNPS_writeTopic(buf, DDS_DCPS_SUBSCRIPTION_TOPIC);
    uint8_t index;
    for (index = 0; index < SDDS_DATA_READER_MAX_OBJS; index++) {
        SNPS_writeData(buf, BuiltinTopicDataReader_encode,
                       (Data) &self->readers[index]);
    }

    return SDDS_RT_OK;
}
rc_t
BuiltinTopicDataReader_encode(NetBuffRef_t* buff, Data data, size_t* size) {
#if defined(SDDS_TOPIC_HAS_PUB) || defined(FEATURE_SDDS_BUILTIN_TOPICS_ENABLED)
    DataReader_t* dr = (DataReader_t*) data;
    byte_t* start = buff->buff_start + buff->curPos;
    *size = 0;
    Marshalling_enc_uint8(start + (*size), &(DataReader_topic(dr)->domain));
    *size += sizeof(domainid_t);
    // tmp variable for the (possible) cast from 8bit to 16bit
    // necessary, because without extended_topic_support the topic_id is only 8 bit
    uint16_t tmp_topic_id = (uint16_t) DataReader_topic(dr)->id;
    Marshalling_enc_uint16(start + (*size), &(tmp_topic_id));
    *size += sizeof(topicid_t);
#endif

    return SDDS_RT_OK;
}

rc_t
DataSink_readerIterator_reset(DataSink_ReaderIterator_t* it, topicid_t topic) {
    it->iteratorTopicID = topic;
    it->iteratorPos = -1;
    it->iteratorNext = -1;
    it->iteratorFiltered = 0;

	for (int8_t i = 0; i < SDDS_DATA_READER_MAX_OBJS; i++) {
		if (self->readers[i].topic->id == it->iteratorTopicID) {
		    it->iteratorNext = i;
			return SDDS_RT_OK;
		}
	}
#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
	it->iteratorFiltered = 1;
	for (int8_t i = 0; i < SDDS_DATA_FILTER_READER_MAX_OBJS; i++) {
		if (self->filteredReaders[i].dataReader.topic->id == it->iteratorTopicID) {
		    it->iteratorNext = i;
			return SDDS_RT_OK;
		}
	}
#endif
    return SDDS_RT_FAIL;
}

DataReader_t*
DataSink_readerIterator_next(DataSink_ReaderIterator_t* it) {
    it->iteratorPos = it->iteratorNext;
    it->iteratorNext = -1;

    DataReader_t* curReader;
    if (it->iteratorFiltered == 0) {
        curReader = &(self->readers[it->iteratorPos]);
    }
    else {
#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
        curReader = (DataReader_t*) &(self->filteredReaders[it->iteratorPos]);
#endif
    }

    if (it->iteratorFiltered == 0) {
		for (int8_t i = it->iteratorPos+1; i < SDDS_DATA_READER_MAX_OBJS; i++) {
			if (self->readers[i].topic->id == it->iteratorTopicID) {
			    it->iteratorNext = i;
				break;
			}
		}
#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
	    if (it->iteratorNext == -1) {
	        it->iteratorFiltered = 1;
	        it->iteratorPos = -1;
	    }
#endif
    }
#ifdef SDDS_DATA_FILTER_READER_MAX_OBJS
    if (it->iteratorFiltered != 0) {
		for (int8_t i = it->iteratorPos+1; i < SDDS_DATA_FILTER_READER_MAX_OBJS; i++) {
			if (self->filteredReaders[i].dataReader.topic->id == it->iteratorTopicID) {
			    it->iteratorNext = i;
				break;
			}
		}
    }
#endif
    return curReader;
}

rc_t
DataSink_readerIterator_hasNext(DataSink_ReaderIterator_t* it) {
    if (it->iteratorNext != -1) {
        return SDDS_RT_OK;
    }
    return SDDS_RT_FAIL;
}
