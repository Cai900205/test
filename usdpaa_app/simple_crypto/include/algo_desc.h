/**
\file algo_desc.h
\brief Shared descriptor for different SEC algorithm
*/
/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__ALG_DESC_H
#define	__ALG_DESC_H

#define JDESC_LEN_MASK   0x7F
#define PREHDR_LEN      2

/** Returns the maximum value from 2 arguments  */
#define MAX(a, b)	((a) > (b) ? (a) : (b))

/** Extracts th Job Descriptor length from the encrypt SNOW F8 + F9 job
 *  descriptor. The extracted value is the number of 32bit words and the
 *   returned value is the number of bytes.
  */
#define SNOW_JDESC_ENC_F8_F9_LEN	(sizeof(uint32_t) * \
					(snow_jdesc_enc_f8_f9[0] &\
					 JDESC_LEN_MASK))

/** Extracts th Job Descriptor length from the decrypt SNOW F8 + F9 job
 *  descriptor. The extracted value is the number of 32bit words and the
 *   returned value is the number of bytes.

  */
#define SNOW_JDESC_DEC_F8_F9_LEN	(sizeof(uint32_t) * \
					(snow_jdesc_dec_f8_f9[0] &\
					 JDESC_LEN_MASK))

#ifdef AES_GCM_RAW_DESCRIPTOR
/** TODO: It has to be built by using DCL(not available currently) */
static uint32_t aes_gcm_desc[] = {
	0x00000011,	/* preheader hi - init descriptor is  17 words long */
	0x00000000,	/* preheader lo - output frame provided to SEC40, so
				 no BMan info */
	0xb8800311,	/* Init Descriptor Header */
	0x02800010,	/* KEY command - class 1, immediate, unencrypted,
				 16 bytes */
	0x2b7e1516,	/* key data  */
	0x28aed2a6,	/* key data  */
	0xabf71588,	/* key data  */
	0x09cf4f3c,	/* key data  */
	0x22a1000c,	/* FIFO LOAD 12 bytes IV in class 1 context reg,
				 flush set */
	0xae2356a1,	/* IV data  */
	0xbc83915f,	/* IV data  */
	0xbc83915f,	/* IV data  */
	0x82100909,	/* OPERATION command - class 1 algorithm, AES, GCM,
				 AS(finalize), encrypt  */
	0x16880004,	/* LOAD immediate value (0) in MATH0 reg */
	0x00000000,	/* immediate data (0) */
	0xa8080a04,	/* MATH command - (input_len + MATH0) ->
				 var_input_len  */
	0xa8080b04,	/* MATH command - (input_len + MATH0) ->
				 var_output_len  */
	0x2b120000,	/* SEQ FIFO LOAD - class 1, message data, L1 set,
				 variable length */
	0x69300000	/* SEQ FIFO STORE - variable length, message data */
};
#endif

/** Job descriptor for SNOW F8 + F9 encryption */
static uint32_t snow_jdesc_enc_f8_f9[] = {
	0xB0850010,	/* Init Job Descriptor Header:start executing from
				 startindex=5; desclen=0x10 words of 32 bytes*/
	/* Protocol Data Block (PDB) */
	0x00000002,	/* reserved */
	0x9FB31245,	/* HFN(Hyper Frame Number)=0x4FD9892[27 bits],
				 seq_num=0x5[5 bits] */
	0xA4000000,	/* bearer=0x14[5 bits], direction=1 (encap) [1bit] */
	0xFBFFB1C0,	/* Threshold=0x07DFFD8E [27 bits] */
	0x04800010,	/* KEY command - class 2, immediate,
				 unencrypted, 16 bytes */
	0xFE6592D4,	/* key data  */
	0x8BFCEA9C,	/* key data  */
	0x9D8E3244,	/* key data  */
	0xD7D7E9F1,	/* key data  */
	0x02800010,	/* KEY command - class 1, immediate,
				 unencrypted, 16 bytes */
	0x8C48FF89,	/* key data  */
	0xCB854FC0,	/* key data  */
	0x9081CC47,	/* key data  */
	0xEDFC8619,	/* key data  */
	0x87430001	/* OPERATION encapsulation, protocol: LTE PDCP
				 PDU Control Plane */
};

/** Job descriptor for SNOW F8 + F9 decryption */
static uint32_t snow_jdesc_dec_f8_f9[] = {
	0xB0850010,		 /* Init Job Descriptor Header:start executing
					 from startindex=5; desclen=0x10 words
					 of 32 bytes */
	/* Protocol Data Block (PDB) */
	0x00000002,	/* reserved[bits 0:29], SNS (Serial Number Size)=
				1[bit 30], optShift=0 [bit 31] */
	0x9FB31245,	/* HFN(Hyper Frame Number)=0x4FD9892[27 bits],
				 seq_num=0x5[5 bits] */
	0xA0000000,	/* bearer=0x14[5 bits], direction=0 (decap)
				 [1 bit], reserved */
	0xFBFFB1C0,	/* Threshold=0x07DFFD8E [27 bits] */
	0x04800010,	/* KEY command - class 2, immediate,
				 unencrypted, 16 bytes */
	0xFE6592D4,	/* key data  */
	0x8BFCEA9C,	/* key data  */
	0x9D8E3244,	/* key data  */
	0xD7D7E9F1,	/* key data  */
	0x02800010,	/* KEY command - class 1, immediate,
				 unencrypted, 16 bytes */
	0x8C48FF89,	/* key data  */
	0xCB854FC0,	/* key data  */
	0x9081CC47,	/* key data  */
	0xEDFC8619,	/* key data  */
	0x86430001	/* OPERATION decapsulation, protocol: LTE PDCP
					 PDU Control Plane */
};

#endif /* __ALG_DESC_H */
