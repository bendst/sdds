/****************************************************************************
 * Copyright (C) 2017 RheinMain University of Applied Sciences              *
 *                                                                          *
 * This file is part of:                                                    *
 *      _____  _____   _____                                                *
 *     |  __ \|  __ \ / ____|                                               *
 *  ___| |  | | |  | | (___                                                 *
 * / __| |  | | |  | |\___ \                                                *
 * \__ \ |__| | |__| |____) |                                               *
 * |___/_____/|_____/|_____/                                                *
 *                                                                          *
 * This Source Code Form is subject to the terms of the Mozilla Public      *
 * License, v. 2.0. If a copy of the MPL was not distributed with this      *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.                 *
 ****************************************************************************/

/**
 * @file      sDDS.h
 * @author    Kai Beckmann
 * @copyright MPL 2 
 * @see       https://github.com/sdds/sdds
 * dunno yet
 */




#ifndef  SDDS_H_INC
#define  SDDS_H_INC

#include "sdds_features_config.h"
#include "sdds_features.h"
#include "sdds_network.h"
#include "sdds_types.h"

rc_t
sDDS_init(void);

//  Forward declaration of classes
//#ifndef SDDS_DATA_READER_MAX_OBJS
//#define SDDS_DATA_READER_MAX_OBJS 64
//#endif
typedef struct _DataReader_t DataReader_t;
typedef struct _History_t History_t;
typedef struct _NetBuffRef_t NetBuffRef_t;
typedef struct NetFrameBuff_t* NetFrameBuff;
typedef struct _Locator_t Locator_t;
typedef struct SourceQos_t SourceQos_t;
typedef struct _Sample_t Sample_t;
typedef struct _ReliableSample_t ReliableSample_t;
typedef struct _Topic_t Topic_t;
typedef struct TimeStampSimple_struct TimeStampSimple_t;
#ifdef FEATURE_SDDS_LOCATION_FILTER_ENABLED
typedef struct LocationFilteredTopic LocationFilteredTopic_t;
#endif

//  Abstraction
#include "os-ssal/Random.h"
#include "os-ssal/Task.h"
#include "os-ssal/Trace.h"
#include "os-ssal/Thread.h"
#include "os-ssal/TimeMng.h"
#include "os-ssal/Memory.h"
#include "os-ssal/Mutex.h"
#include "os-ssal/NodeConfig.h"
#include "os-ssal/SSW.h"
#ifdef FEATURE_SDDS_LOCATION_ENABLED
#include "os-ssal/LocationService.h"
#endif
#include "dds/DDS_DCPS.h"

//  Class headers
#include "Debug.h"
#include "BitArray.h"
#include "Qos.h"
#include "Marshalling.h"
#include "SNPS.h"
#include "NetFrameBuff.h"
#include "NetBuffRef.h"
#include "Network.h"
#include "History.h"
#include "DataReader.h"
#include "DataWriter.h"
#include "DataSink.h"
#include "DataSource.h"
#include "Locator.h"
#include "LocatorDB.h"
#include "Log.h"
#include "Sample.h"
#include "TopicDB.h"
#include "Topic.h"
#ifdef FEATURE_SDDS_GEOMETRY_ENABLED
#include "Geometry.h"
#include "GeometryStore.h"
#endif
#ifdef FEATURE_SDDS_LOCATION_TRACKING_ENABLED
#include "LocationTrackingService.h"
#endif
#ifdef FEATURE_SDDS_LOCATION_FILTER_ENABLED
#include "ContentFilteredTopic.h"
#include "LocationFilteredTopic.h"
#include "FilteredDataReader.h"
#endif
#ifdef FEATURE_SDDS_MANAGEMENT_TOPIC_ENABLED
#include "ManagementTopic.h"
#include "ManagementTopicPublicationService.h"
#include "ManagementTopicSubscriptionService.h"
#endif
#ifdef FEATURE_SDDS_SUBSCRIPTION_MANAGER_ENABLED
#include "SubscriptionManagementService.h"
#endif
#ifdef FEATURE_SDDS_BUILTIN_TOPICS_ENABLED
#include "BuiltinTopic.h"
#include "BuiltInTopicPublicationService.h"
#   ifdef FEATURE_SDDS_LOCATION_ENABLED 
#include "BuiltInLocationUpdateService.h"
#   endif
#endif
#ifdef FEATURE_SDDS_DISCOVERY_ENABLED
#include "DiscoveryService.h"
#endif

#ifdef FEATURE_SDDS_SECURITY_ENABLED
#include "Security.h"
#endif
#include "Management.h"

#endif   /* ----- #ifndef SDDS_H_INC  ----- */
