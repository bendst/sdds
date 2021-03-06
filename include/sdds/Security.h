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
 * @file      Security.h
 * @author    TODO
 * @copyright MPL 2 
 * @see       https://github.com/sdds/sdds
 * TODO
 */

#ifndef __SECURITY_H__
#define __SECURITY_H__

#include "Sha224.h"
#include "Ecc.h"
#include "os-ssal/aes-128.h"

#define DDS_S_Result_t int
#define VALIDATION_OK	0
#define VALIDATION_FAILED	1
#define VALIDATION_PENDING_RETRY	2
#define VALIDATION_PENDING_HANDSHAKE_MESSAGE	3
#define VALIDATION_OK_FINAL_MESSAGE	4

#define SDDS_MAX_AUTH_ITERATIONS 10

#define GMCLASSID_SECURITY_AUTH_HANDSHAKE "dds.sec.auth"
#define GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS "dds.sec.ct"

#define SDDS_SECURITY_CLASS_AUTH_REQ "DDS:Auth:Req"
#define SDDS_SECURITY_CLASS_AUTH_REP "DDS:Auth:Rep"
#define SDDS_SECURITY_PROP_ID "dds.sec.id"
#define SDDS_SECURITY_PROP_PUB_KEY_X "dds.sec.x"
#define SDDS_SECURITY_PROP_PUB_KEY_Y "dds.sec.y"
#define SDDS_SECURITY_PROP_SIG_R "dds.sec.r"
#define SDDS_SECURITY_PROP_SIG_S "dds.sec.s"
#define SDDS_SECURITY_PROP_NONCE "dds.sec.nonce"
#define SDDS_SECURITY_PROP_MACTAG "dds.sec.mactag"
#define SDDS_SECURITY_PROP_KEY_MATERIAL "dds.sec.k"
#define SDDS_SECURITY_PROP_TID "dds.sec.tid"
#define SDDS_SECURITY_PROP_IV "dds.sec.iv"

#define SDDS_SECURITY_RECEIVE_SLEEP_SEC 2

#define SDDS_SECURITY_KDC_USER_ID "user0001"

#define SDDS_SECURITY_USER_ID_STRLEN 8
#define SDDS_SECURITY_VERSION_STRLEN 2
#define SDDS_SECURITY_TIMESTAMP_STRLEN 8
#define SDDS_SECURITY_CIPHER_SUITE_STRLEN 4

#define SDDS_SECURITY_CERT_INFO_STRLEN \
        SDDS_SECURITY_USER_ID_STRLEN + 1 \
      + SDDS_SECURITY_VERSION_STRLEN + 1 \
      + SDDS_SECURITY_TIMESTAMP_STRLEN + 1 \
      + SDDS_SECURITY_CIPHER_SUITE_STRLEN + 1 \

#define SDDS_SECURITY_CERT_STRLEN \
      + SDDS_SECURITY_CERT_INFO_STRLEN \
      + NUM_ECC_DIGITS*2 + 1 \
      + NUM_ECC_DIGITS*2 + 1

#define SDDS_SECURITY_HANDSHAKE_STATE_0 0
#define SDDS_SECURITY_HANDSHAKE_STATE_1 1
#define SDDS_SECURITY_HANDSHAKE_STATE_2 2

#define SDDS_SECURITY_HANDSHAKE_MAX 4

#define SDDS_SECURITY_KDF_KEY_BYTES 32
#define SDDS_SECURITY_IV_SIZE 12
#define XCBC_MAC_SIZE 12
#define XCBC_K1 0x01
#define XCBC_K2 0x02
#define XCBC_K3 0x03

typedef DataHolder Token;
typedef Token HandshakeMessageToken;
typedef SSW_NodeID_t IdentityHandle;
typedef uint16_t MessageIdentity;
typedef uint8_t* SharedSecretHandle;

typedef struct {
	char* message;
	long code;
	long minor_code;
} SecurityException;

typedef struct {
	uint16_t tid;
  List_t *applist;
  uint8_t key_material[SDDS_SECURITY_KDF_KEY_BYTES];
} Ac_topic;

typedef Ac_topic DatawriterCryptoHandle;
typedef Ac_topic DatareaderCryptoHandle;

typedef struct {
  uint8_t *data;
  uint8_t len;
} OctetSeq;

typedef struct Remote_info {
  char uid[SDDS_SECURITY_CERT_INFO_STRLEN];
  EccPoint public_key;
  uint8_t signature_r[NUM_ECC_DIGITS];
  uint8_t signature_s[NUM_ECC_DIGITS];
  uint8_t mactag[XCBC_MAC_SIZE];
  uint8_t nonce[NUM_ECC_DIGITS / 2];
  uint8_t remote_nonce[NUM_ECC_DIGITS / 2];
  uint8_t shared_secret[NUM_ECC_DIGITS];
  uint8_t key_material[SDDS_SECURITY_KDF_KEY_BYTES];
} Remote_info;

typedef struct Local_info {
  uint8_t *ca_public_key_x;
  uint8_t *ca_public_key_y;
  uint8_t *public_key_x;
  uint8_t *public_key_y;
  uint8_t *signature_r;
  uint8_t *signature_s;
  uint8_t *private_key;
} Local_info;

typedef struct HandshakeHandle {
  IdentityHandle node;
  char state;
  Remote_info info;
} HandshakeHandle;

HandshakeHandle*
Security_get_handshake_handle(IdentityHandle *node);

HandshakeHandle*
Security_new_handshake_handle(IdentityHandle *node);

rc_t 
Security_init();

Ac_topic*
Security_lookup_key(uint16_t tid);

rc_t 
Security_receive_key();

rc_t 
Security_auth();

rc_t
Security_kdc_add_rule(uint16_t tid, char *aid);

rc_t 
Security_kdc_init();

rc_t 
Security_kdc();

rc_t
Security_send_crypto_tokens(HandshakeHandle *h);

rc_t
Security_verify_certificate(HandshakeHandle *h);

rc_t
Security_verify_mactag(HandshakeHandle *h);

rc_t
Security_set_key_material(HandshakeHandle *h);

void
Security_cleanup_handshake_handle(HandshakeHandle *h);

void 
Security_get_bytes(uint8_t res[NUM_ECC_DIGITS], char* str, int nbytes);

void 
Security_get_string(char *str, uint8_t num[NUM_ECC_DIGITS]);

void
Security_print_key(uint8_t *key, int n);

void 
Security_kdf(HandshakeHandle *h);

void 
Security_aes_xcbc_mac(uint8_t aes_key[AES_128_KEY_LENGTH], uint8_t *data, uint8_t size, uint8_t mac[XCBC_MAC_SIZE]);

void 
Security_aes_ctr(uint8_t iv[SDDS_SECURITY_IV_SIZE], uint8_t aes_key[AES_128_KEY_LENGTH], uint8_t *data, uint8_t size);

DDS_S_Result_t 
DDS_Security_Authentication_begin_handshake_request(
	HandshakeHandle *handshake_handle, /* inout (fixed length) */
	HandshakeMessageToken *handshake_message_out, /* in (variable length) */
	HandshakeMessageToken *handshake_message_in, /* in (variable length) */
	IdentityHandle *initiator_identity_handle, /* in (fixed length) */
	IdentityHandle *replier_identity_handle, /* in (fixed length) */
	SecurityException *ex /* inout (variable length) */
);

DDS_S_Result_t 
DDS_Security_Authentication_begin_handshake_reply(
	HandshakeHandle *handshake_handle, /* inout (fixed length) */
	HandshakeMessageToken *handshake_message_out, /* inout (variable length) */
	HandshakeMessageToken *handshake_message_in, /* in (variable length) */
	IdentityHandle *initiator_identity_handle, /* in (fixed length) */
	IdentityHandle *replier_identity_handle, /* in (fixed length) */
	SecurityException *ex /* inout (variable length) */
);

DDS_S_Result_t 
DDS_Security_Authentication_process_handshake(
	HandshakeMessageToken *handshake_message_out,
	HandshakeMessageToken *handshake_message_in,
	HandshakeHandle *handshake_handle,
	SecurityException *ex
);

SharedSecretHandle 
DDS_Security_Authentication_get_shared_secret(
	HandshakeHandle *handshake_handle,
	SecurityException *ex
);

bool 
DDS_Security_CryptoTransform_encode_serialized_data(
	OctetSeq *encoded_buffer, /* inout (variable length) */
	OctetSeq *plain_buffer, /* in (variable length) */
	DatawriterCryptoHandle *sending_datawriter_crypto, /* in (fixed length) */
	SecurityException *ex /* inout (variable length) */
);

bool 
DDS_Security_CryptoTransform_decode_serialized_data(
	OctetSeq *plain_buffer, /* inout (variable length) */
	OctetSeq *encoded_buffer, /* in (variable length) */
	DatareaderCryptoHandle *receiving_datareader_crypto, /* in (fixed length) */
	DatawriterCryptoHandle *sending_datawriter_crypto, /* in (fixed length) */
	SecurityException *ex
);

#endif  /* __SECURITY_H__ */
