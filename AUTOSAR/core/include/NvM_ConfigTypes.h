/*-------------------------------- Arctic Core ------------------------------
 * Copyright (C) 2013, ArcCore AB, Sweden, www.arccore.com.
 * Contact: <contact@arccore.com>
 * 
 * You may ONLY use this file:
 * 1)if you have a valid commercial ArcCore license and then in accordance with  
 * the terms contained in the written license agreement between you and ArcCore, 
 * or alternatively
 * 2)if you follow the terms found in GNU General Public License version 2 as 
 * published by the Free Software Foundation and appearing in the file 
 * LICENSE.GPL included in the packaging of this file or here 
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>
 *-------------------------------- Arctic Core -----------------------------*/

/** @reqSettings DEFAULT_SPECIFICATION_REVISION=3.1.5 */

#ifndef NVM_CONFIG_TYPES_H_
#define NVM_CONFIG_TYPES_H_

#include "NvM_Types.h"

#if defined(USE_DEM)
#include "Dem.h"
#endif

/* NvM_ApiConfigClassType */	/** @req NVM491 */
#define NVM_API_CONFIG_CLASS_1	0
#define NVM_API_CONFIG_CLASS_2	1
#define NVM_API_CONFIG_CLASS_3	2

/* IMPROVEMENT: The CRC module has now support for CRC8 so it may be added */
typedef enum {
	NVM_CRC16,
	NVM_CRC32
} Nvm_BlockCRCTypeType;

typedef enum {
	NVM_BLOCK_NATIVE,
	NVM_BLOCK_REDUNDANT,
	NVM_BLOCK_DATASET
} NvM_BlockManagementTypeType;

/*
 * Callback function prototypes
 */

typedef Std_ReturnType (*NvM_SingleBlockCallbackFunctionType)(uint8 ServiceId, NvM_RequestResultType JobResult);	/** @req NVM467 */
typedef void (*NvM_MultiBlockCallbackFunctionType)(uint8 ServiceId, NvM_RequestResultType JobResult);	/** @req NVM468 */
typedef Std_ReturnType (*NvM_InitBlockCallbackFunctionType)(void);	/** @req NVM469 */

/*
 * Containers and configuration parameters
 */

typedef struct {
	NvM_MultiBlockCallbackFunctionType		MultiBlockCallback;		/** @req NVM500 */
	// The rest of the parameters is realized in NvM_Cfg.h
} NvM_CommonType;


typedef struct {
	// NVRAM block global settings
NvM_BlockManagementTypeType			BlockManagementType;	/** @req 3.1.5|NVM062 */
	uint8								BlockJobPriority;		/** @req 3.1.5|NVM477 */
	boolean								BlockWriteProt;			/** @req 3.1.5|NVM033 */
	boolean								WriteBlockOnce;			/** @req 3.1.5|NVM072 */
	boolean								SelectBlockForReadall;	/** @req 3.1.5|NVM117 *//** @req 3.1.5|NVM245 */
	boolean								ResistantToChangesSw;	/** @req 3.1.5|NVM483 */
	NvM_SingleBlockCallbackFunctionType	SingleBlockCallback;
	uint16								NvBlockLength;			/** @req 3.1.5|NVM479 */

	// CRC usage of RAM and NV blocks
	boolean								BlockUseCrc;			/** @req 3.1.5|NVM036 */
	Nvm_BlockCRCTypeType				BlockCRCType;			/** @req 3.1.5|NVM476 */

	// RAM block, RamBlockDataAddress == NULL means temporary block otherwise permanent block
	uint8								*RamBlockDataAddress;	/** @req 3.1.5|NVM482 */
	boolean								CalcRamBlockCrc;		/** @req 3.1.5|NVM119 */

	// NV block, FEE/EA references
	uint8								NvBlockNum;				/** @req 3.1.5|NVM480 */
	uint32								NvramDeviceId;			/** @req 3.1.5|NVM035 */
	uint16								NvBlockBaseNumber;		/** @req 3.1.5|NVM478 */

	// ROM block, reference, if RomBlockDataAdress == NULL no ROM data is available
	uint16								RomBlockNum;			/** @req 3.1.5|NVM485 */
	uint8								*RomBlockDataAdress;	/** @req 3.1.5|NVM484 */
	NvM_InitBlockCallbackFunctionType	InitBlockCallback;		/** @req 3.1.5|NVM116 */

	// Containers
#if 0	// Currently not used
	NvM_TargetBlockReferenceType		TargetBlockReference;	/** @req NVM486 */
#endif
} NvM_BlockDescriptorType;	/** @req NVM061 */

#if defined(USE_DEM)
typedef struct {
    Dem_EventIdType                 NvMIntegrityFailedDemEventId;
    Dem_EventIdType                 NvMReqFailedDemEventId;
} NvM_DemEventReferencesType;
#endif

typedef struct {
	// Containers
	NvM_CommonType					Common;				// 1
#if defined(USE_DEM)
	const NvM_DemEventReferencesType      DemEvents;          // 1
#endif
	const NvM_BlockDescriptorType	*BlockDescriptor;	// 1..65536
} NvM_ConfigType;

/*
 * Make the NvM_Config visible for others.
 */
extern const NvM_ConfigType NvM_Config;




#endif /*NVM_CONFIG_TYPES_H_*/
