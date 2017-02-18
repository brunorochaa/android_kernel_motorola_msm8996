/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either
* 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
*  terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
*  provisions apply instead of the ones
* mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
********************************************************************************
*
*/

#ifndef _VL53L1_API_H_
#define _VL53L1_API_H_

#include "vl53l1_api_strings.h"
#include "vl53l1_api_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @defgroup VL53L1_cut11_group VL53L1 cut1.1 Function Definition
 *  @brief    VL53L1 cut1.1 Function Definition
 *  @{
 */

/** @defgroup VL53L1_general_group VL53L1 General Functions
 *  @brief    General functions and definitions
 *  @{
 */

/**
 * @brief Return the VL53L1 PAL Implementation Version
 *
 * @note This function doesn't access to the device
 *
 * @param   pVersion              Pointer to current PAL Implementation Version
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetVersion(VL53L1_Version_t *pVersion);

/**
 * @brief Return the PAL Specification Version used for the current
 * implementation.
 *
 * @note This function doesn't access to the device
 *
 * @param   pPalSpecVersion       Pointer to current PAL Specification Version
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetPalSpecVersion(
	VL53L1_Version_t *pPalSpecVersion);

/**
 * @brief Reads the Product Revision for a for given Device
 * This function can be used to distinguish cut1.0 from cut1.1.
 *
 * @param   Dev                 Device Handle
 * @param   pProductRevisionMajor  Pointer to Product Revision Major
 * for a given Device
 * @param   pProductRevisionMinor  Pointer to Product Revision Minor
 * for a given Device
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetProductRevision(VL53L1_DEV Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor);

/**
 * @brief Reads the Device information for given Device
 *
 * @note This function Access to the device
 *
 * @param   Dev                 Device Handle
 * @param   pVL53L1_DeviceInfo  Pointer to current device info for a given
 *  Device
 * @return  VL53L1_ERROR_NONE   Success
 * @return  "Other error code"  See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetDeviceInfo(VL53L1_DEV Dev,
	VL53L1_DeviceInfo_t *pVL53L1_DeviceInfo);

/**
 * @brief Human readable Range Status string for a given RangeStatus
 *
 * @note This function doesn't access to the device
 *
 * @param   RangeStatus         The RangeStatus code as stored on
 * @a VL53L1_RangingMeasurementData_t
 * @param   pRangeStatusString  The returned RangeStatus string. Shall be
 * defined as char buf[VL53L1_MAX_STRING_LENGTH]
 * @return  VL53L1_ERROR_NONE   Success
 * @return  "Other error code"  See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString);

/**
 * @brief Human readable error string for current PAL error status
 *
 * @note This function doesn't access to the device
 *
 * @param   PalErrorCode       The error code as stored on @a VL53L1_Error
 * @param   pPalErrorString    The error string corresponding to the
 * PalErrorCode. Shall be defined as char buf[VL53L1_MAX_STRING_LENGTH]
 * @return  VL53L1_ERROR_NONE  Success
 * @return  "Other error code" See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetPalErrorString(VL53L1_Error PalErrorCode,
	char *pPalErrorString);

/**
 * @brief Human readable PAL State string
 *
 * @note This function doesn't access to the device
 *
 * @param   PalStateCode          The State code as stored on @a VL53L1_State
 * @param   pPalStateString       The State string corresponding to the
 * PalStateCode. Shall be defined as char buf[VL53L1_MAX_STRING_LENGTH]
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetPalStateString(VL53L1_State PalStateCode,
	char *pPalStateString);

/**
 * @brief Reads the internal state of the PAL for a given Device
 *
 * @note This function doesn't access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPalState             Pointer to current state of the PAL for a
 * given Device
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetPalState(VL53L1_DEV Dev,
	VL53L1_State *pPalState);



/** @} VL53L1_general_group */

/** @defgroup VL53L1_init_group VL53L1 Init Functions
 *  @brief    VL53L1 Init Functions
 *  @{
 */

/**
 * @brief Set new device address
 *
 * After completion the device will answer to the new address programmed.
 * This function should be called when several devices are used in parallel
 * before start programming the sensor.
 * When a single device us used, there is no need to call this function.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   DeviceAddress         The new Device address
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetDeviceAddress(VL53L1_DEV Dev,
	uint8_t DeviceAddress);

/**
 *
 * @brief One time device initialization
 *
 * To be called once and only once after device is brought out of reset
 * (Chip enable) and booted see @a VL53L1_WaitDeviceBooted()
 *
 * @par Function Description
 * When not used after a fresh device "power up" or reset, it may return
 * @a #VL53L1_ERROR_CALIBRATION_WARNING meaning wrong calibration data
 * may have been fetched from device that can result in ranging offset error\n
 * If application cannot execute device reset or need to run VL53L1_DataInit
 * multiple time then it  must ensure proper offset calibration saving and
 * restore on its own by using @a VL53L1_GetOffsetCalibrationData() on first
 * power up and then @a VL53L1_SetOffsetCalibrationData() in all subsequent init
 * This function will change the VL53L1_State from VL53L1_STATE_POWERDOWN to
 * VL53L1_STATE_WAIT_STATICINIT.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_DataInit(VL53L1_DEV Dev);


/**
 * @brief Do basic device init (and eventually patch loading)
 * This function will change the VL53L1_State from
 * VL53L1_STATE_WAIT_STATICINIT to VL53L1_STATE_IDLE.
 * In this stage all default setting will be applied.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_StaticInit(VL53L1_DEV Dev);

/**
 * @brief Wait for device booted after chip enable (hardware standby)
 * This function can be run only when VL53L1_State is VL53L1_STATE_POWERDOWN.
 *
 * @param   Dev      Device Handle
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 *
 */
VL53L1_Error VL53L1_WaitDeviceBooted(VL53L1_DEV Dev);


/** @} VL53L1_init_group */

/** @defgroup VL53L1_parameters_group VL53L1 Parameters Functions
 *  @brief    Functions used to prepare and setup the device
 *  @{
 */

/**
 * @brief  Set a new Preset Mode
 * @par Function Description
 * Set device to a new Operating Mode (High speed ranging, Multi objects ...)
 *
 * @note This function doesn't Access to the device
 *
 * @warning This function change the timing budget to 16 ms and the inter-
 * measurement period to 1000 ms. Also the VL53L1_DISTANCEMODE_LONG is used.
 *
 * @param   Dev                   Device Handle
 * @param   PresetMode            New Preset mode to apply
 *                                Valid values are:
 *                                VL53L1_PRESETMODE_MULTIZONES_SCANNING
 *                                VL53L1_PRESETMODE_RANGING
 *                                VL53L1_PRESETMODE_AUTONOMOUS
 *                                VL53L1_PRESETMODE_LITE_RANGING
 *                                VL53L1_PRESETMODE_OLT
 *
 * @return  VL53L1_ERROR_NONE               Success
 * @return  VL53L1_ERROR_MODE_NOT_SUPPORTED This error occurs when PresetMode is
 *                                          not in the supported list
 */
VL53L1_Error VL53L1_SetPresetMode(VL53L1_DEV Dev,
		VL53L1_PresetModes PresetMode);

/**
 * @brief  Get current Preset Mode
 * @par Function Description
 * Get actual mode of the device(ranging, histogram ...)
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPresetMode           Pointer to current apply mode value
 *
 * @return  VL53L1_ERROR_NONE                   Success
 * @return  VL53L1_ERROR_MODE_NOT_SUPPORTED     This error occurs when
 * DeviceMode is not in the supported list
 */
VL53L1_Error VL53L1_GetPresetMode(VL53L1_DEV Dev,
		VL53L1_PresetModes *pPresetMode);


/**
 * @brief  Set the distance mode
 * @par Function Description
 * Set the distance mode to be used for the next ranging.
 * The modes Short, Medium and Long are used to optimize the ranging accuracy
 * in a specific range of distance. The user select one of these modes to fix
 * the distance range, for this reason these are also called Manual modes.
 * Two additional modes are supported: AUTO and AUTO_LITE the difference between
 * these modes is the following.
 * The mode AUTO take into account both the ranging distance (RangeMilliMeter)
 * and the dmax distance (DmaxMilliMeter). The algorithm uses the ranging
 * distance when the range status is ok i.e. when range
 * status = VL53L1_RANGESTATUS_RANGE_VALID or
 * status = VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK
 * and it uses the dmax distance when the range status is not ok.
 * The AUTO_LITE take into account only the ranging distance, so nothing is done
 * in case of range error i.e. the distance mode will not be changed.
 * @note This function doesn't Access to the device
 *
 * @warning This function should be called after @a VL53L1_SetPresetMode().

 * @param   Dev                   Device Handle
 * @param   DistanceMode          Distance mode to apply
 *                                Valid values are:
 *                                for all Preset Mode (MANUAL MODE)
 *                                VL53L1_DISTANCEMODE_SHORT
 *                                VL53L1_DISTANCEMODE_MEDIUM
 *                                VL53L1_DISTANCEMODE_LONG
 *
 *                                for LIGHT_RANGING and RANGING (AUTO MODE)
 *                                VL53L1_DISTANCEMODE_AUTO_LITE
 *                                VL53L1_DISTANCEMODE_AUTO
 * @return  VL53L1_ERROR_NONE               Success
 * @return  VL53L1_ERROR_MODE_NOT_SUPPORTED This error occurs when DistanceMode
 *                                          is not in the supported list
 * @return  "Other error code"              See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetDistanceMode(VL53L1_DEV Dev,
		VL53L1_DistanceModes DistanceMode);

/**
 * @brief  Get the distance mode
 * @par Function Description
 * Get the distance mode used for the next ranging.
 *
 * @param   Dev                   Device Handle
 * @param   *pDistanceMode        Pointer to Distance mode
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetDistanceMode(VL53L1_DEV Dev,
		VL53L1_DistanceModes *pDistanceMode);


/**
 * @brief  Set the output mode
 * @par Function Description
 * Set the output mode to be used for the next ranging. The output mode is used
 * to select, in case of multiple objects, which one will be used in
 * function @a VL53L1_GetRangingMeasurementData().
  * VL53L1_SetOutputMode also sets the object used by automatic
  * distance mode algorithm when @a VL53L1_SetDistanceMode() is
  * set to automatic mode.
 *
 * @note This function doesn't Access to the device
 *
 * @warning This function should be called after @a VL53L1_SetPresetMode().

 * @param   Dev                   Device Handle
 * @param   OutputMode            Output mode to apply
 *                                Valid values are:
 *                                VL53L1_OUTPUTMODE_NEAR
 *                                VL53L1_OUTPUTMODE_STRONGEST
 *
 * @return  VL53L1_ERROR_NONE               Success
 * @return  VL53L1_ERROR_MODE_NOT_SUPPORTED This error occurs when OutputMode
 *                                          is not in the supported list
 * @return  "Other error code"              See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetOutputMode(VL53L1_DEV Dev,
		VL53L1_OutputModes OutputMode);

/**
 * @brief  Get the output mode
 * @par Function Description
 * Get the output mode used for the next ranging.
 *
 * @param   Dev                   Device Handle
 * @param   *pOutputMode          Pointer to Output mode
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetOutputMode(VL53L1_DEV Dev,
		VL53L1_OutputModes *pOutputMode);


/**
 * @brief Set Ranging Timing Budget in microseconds
 *
 * @par Function Description
 * Defines the maximum time allowed by the user to the device to run a
 * full ranging sequence for the current mode (ranging, histogram, ASL ...)
 *
 * @param   Dev                                Device Handle
 * @param MeasurementTimingBudgetMicroSeconds  Max measurement time in
 * microseconds.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS  Error timing parameter not
 *                                       supported.
 *                                       The maximum accepted value for the
 *                                       computed timing budget is 10 seconds
 *                                       the minimum value depends on the preset
 *                                       mode selected.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetMeasurementTimingBudgetMicroSeconds(
	VL53L1_DEV Dev, uint32_t MeasurementTimingBudgetMicroSeconds);

/**
 * @brief Get Ranging Timing Budget in microseconds
 *
 * @par Function Description
 * Returns the programmed the maximum time allowed by the user to the
 * device to run a full ranging sequence for the current mode
 * (ranging, histogram, ASL ...)
 *
 * @param   Dev                                    Device Handle
 * @param   pMeasurementTimingBudgetMicroSeconds   Max measurement time in
 * microseconds.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetMeasurementTimingBudgetMicroSeconds(
	VL53L1_DEV Dev, uint32_t *pMeasurementTimingBudgetMicroSeconds);


/**
 * Program continuous mode Inter-Measurement period in milliseconds
 *
 * @par Function Description
 * When trying to set too short time return  INVALID_PARAMS minimal value
 *
 * @param   Dev                                  Device Handle
 * @param   InterMeasurementPeriodMilliSeconds   Inter-Measurement Period in ms.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetInterMeasurementPeriodMilliSeconds(
	VL53L1_DEV Dev, uint32_t InterMeasurementPeriodMilliSeconds);

/**
 * Get continuous mode Inter-Measurement period in milliseconds
 *
 * @par Function Description
 *
 * @param   Dev                                  Device Handle
 * @param   pInterMeasurementPeriodMilliSeconds  Pointer to programmed
 *  Inter-Measurement Period in milliseconds.
 * @return  VL53L1_ERROR_NONE
 */
VL53L1_Error VL53L1_GetInterMeasurementPeriodMilliSeconds(
	VL53L1_DEV Dev, uint32_t *pInterMeasurementPeriodMilliSeconds);


/** @} VL53L1_parameters_group */


/** @defgroup VL53L1_limitcheck_group VL53L1 Limit Check Functions
 *  @brief    Functions used for the Limit checks
 *  @{
 */



/**
 * @brief  Get the number of the check limit managed by a given Device
 *
 * @par Function Description
 * This function give the number of the check limit managed by the Device
 *
 * @param   pNumberOfLimitCheck           Pointer to the number of check limit.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetNumberOfLimitCheck(
	uint16_t *pNumberOfLimitCheck);

/**
 * @brief  Return a description string for a given limit check number
 *
 * @par Function Description
 * This function returns a description string for a given limit check number.
 * The limit check is identified with the LimitCheckId.
 *
 * @param   LimitCheckId                  Limit Check ID
 (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckString             Pointer to the description string of
 * the given check limit. Shall be defined as char buf[VL53L1_MAX_STRING_LENGTH]
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetLimitCheckInfo(uint16_t LimitCheckId,
	char *pLimitCheckString);

/**
 * @brief  Return a the Status of the specified check limit
 *
 * @par Function Description
 * This function returns the Status of the specified check limit.
 * The value indicate if the check is fail or not.
 * The limit check is identified with the LimitCheckId.
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckStatus             Pointer to the
 Limit Check Status of the given check limit.
 * LimitCheckStatus :
 * 0 the check is not fail or not enabled
 * 1 the check if fail
 *
 * <p><ul>
 *    <li>VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE: the sigma indicate the quality
 *    of the measure. The more it is little the better it is.
 *    The status is 1 when current sigma is greater then the limit.</li>
 *    <li>VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE: the signal rate indicate
 *    the strength of the returned signal. The more it is big the better it is.
 *    The status is 1 when current signal is lower then the limit.</li>
 * </ul></p>
 *
 *
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetLimitCheckStatus(VL53L1_DEV Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckStatus);

/**
 * @brief  Enable/Disable a specific limit check
 *
 * @par Function Description
 * This function Enable/Disable a specific limit check.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function doesn't Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   LimitCheckEnable              if 1 the check limit
 *  corresponding to LimitCheckId is Enabled
 *                                        if 0 the check limit
 *  corresponding to LimitCheckId is disabled
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS   This error is returned
 *  when LimitCheckId value is out of range.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetLimitCheckEnable(VL53L1_DEV Dev,
	uint16_t LimitCheckId, uint8_t LimitCheckEnable);

/**
 * @brief  Get specific limit check enable state
 *
 * @par Function Description
 * This function get the enable state of a specific limit check.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function Access to the device
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckEnable             Pointer to the check limit enable
 * value.
 *  if 1 the check limit
 *        corresponding to LimitCheckId is Enabled
 *  if 0 the check limit
 *        corresponding to LimitCheckId is disabled
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS   This error is returned
 *  when LimitCheckId value is out of range.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetLimitCheckEnable(VL53L1_DEV Dev,
	uint16_t LimitCheckId, uint8_t *pLimitCheckEnable);

/**
 * @brief  Set a specific limit check value
 *
 * @par Function Description
 * This function set a specific limit check value.
 * The limit check is identified with the LimitCheckId.
 *
 * @note Note that the value written with that function will not be applied if
 * the limit is not enabled. In other words this function will not enable the
 * limit but change only the value. In case the limit is not enabled the value
 * is saved internally and applied with VL53L1_SetLimitCheckEnable.
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   LimitCheckValue               Limit check Value for a given
 * LimitCheckId
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetLimitCheckValue(VL53L1_DEV Dev,
	uint16_t LimitCheckId, FixPoint1616_t LimitCheckValue);

/**
 * @brief  Get a specific limit check value
 *
 * @par Function Description
 * This function get a specific limit check value from device then it updates
 * internal values and check enables.
 * The limit check is identified with the LimitCheckId.
 *
 * @note This function get the current value from device if zero then the value
 * returned is the one stored by the user, but in that case the check is store
 * as disabled. If the value from device is not zero, this is returned and set
 * into the memory at the same way that user call VL53L1_SetLimitCheckValue()
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckValue              Pointer to Limit
 *  check Value for a given LimitCheckId.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetLimitCheckValue(VL53L1_DEV Dev,
	uint16_t LimitCheckId, FixPoint1616_t *pLimitCheckValue);

/**
 * @brief  Get the current value of the signal used for the limit check
 *
 * @par Function Description
 * This function get a the current value of the signal used for the limit check.
 * To obtain the latest value you should run a valid ranging before.
 * The value reported is linked to the limit check identified with the
 * LimitCheckId.
 *
 * @param   Dev                           Device Handle
 * @param   LimitCheckId                  Limit Check ID
 *  (0<= LimitCheckId < VL53L1_GetNumberOfLimitCheck() ).
 * @param   pLimitCheckCurrent            Pointer to current Value for a
 * given LimitCheckId.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetLimitCheckCurrent(VL53L1_DEV Dev,
	uint16_t LimitCheckId, FixPoint1616_t *pLimitCheckCurrent);

/** @} VL53L1_limitcheck_group */



/** @defgroup VL53L1_ROI_group VL53L1 ROI Functions
 *  @brief    Functions used to select ROIs
 *  @{
 */

/**
 * @brief Get the Maximum number of ROI Zones managed by the Device
 *
 * @par Function Description
 * Get Maximum number of ROI Zones managed by the Device.
 *
 * @note The number of Zone depends on the preset mode used so to have the
 * right number this function should be call after @a VL53L1_SetPresetMode()
 * @note This function doesn't Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pMaxNumberOfROI   Pointer to the Maximum Number
 *  of ROI Zones value.
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetMaxNumberOfROI(VL53L1_DEV Dev,
	uint8_t *pMaxNumberOfROI);

/**
 * @brief Set the ROI  to be used for a specific Device
 *
 * @par Function Description
 * Set the ROI to be used for a specific Device.
 * Rectangle in user's coordinate system:
 *	15	TL(x,y) o-----*
 *   ^			|     |
 *   |			*-----o BR(x,y)
 *   0------------------------- >15
 *   Minimal ROI size is 4x4
 *
 * @param   Dev                      Device Handle
 * @param   pRoiConfig               Pointer to the Structure containing all the
 * ROI to be used.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetROI(VL53L1_DEV Dev,
		VL53L1_RoiConfig_t *pRoiConfig);

/**
 * @brief Get the ROI managed by the Device
 *
 * @par Function Description
 * Get the ROI managed by the Device
 *
 * @param   Dev                   Device Handle
 * @param   pRoiConfig            Pointer to the Structure containing all the
 * ROI to be used.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetROI(VL53L1_DEV Dev,
		VL53L1_RoiConfig_t *pRoiConfig);


/** @} VL53L1_ROI_group */


/** @defgroup VL53L1_sequencestep_group VL53L1 Sequence Step Functions
 *  @brief    Functions used to select Steps done on each ranging
 *  @{
 */

/**
 * @brief Gets number of sequence steps managed by the API.
 *
 * @par Function Description
 * This function retrieves the number of sequence steps currently managed
 * by the API
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   pNumberOfSequenceSteps       Out parameter reporting the number of
 *                                       sequence steps.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetNumberOfSequenceSteps(VL53L1_DEV Dev,
	uint8_t *pNumberOfSequenceSteps);

/**
 * @brief Gets the name of a given sequence step.
 *
 * @par Function Description
 * This function retrieves the name of sequence steps corresponding to
 * SequenceStepId.
 *
 * @note This function doesn't Accesses the device
 *
 * @param   SequenceStepId               Sequence step identifier.
 * @param   pSequenceStepsString         Pointer to Info string. Shall be
 * defined as char buf[VL53L1_MAX_STRING_LENGTH]
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetSequenceStepsInfo(
	VL53L1_SequenceStepId SequenceStepId, char *pSequenceStepsString);



/**
 * @brief Sets the (on/off) state of a requested sequence step.
 *
 * @par Function Description
 * This function enables/disables a requested sequence step.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   SequenceStepId	         Sequence step identifier.
 * @param   SequenceStepEnabled          Demanded state {0=Off,1=On}
 *                                       is enabled.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetSequenceStepEnable(VL53L1_DEV Dev,
	VL53L1_SequenceStepId SequenceStepId, uint8_t SequenceStepEnabled);

/**
 * @brief Gets the (on/off) state of a requested sequence step.
 *
 * @par Function Description
 * This function retrieves the state of a requested sequence step, i.e. on/off.
 *
 * @note This function Accesses the device
 *
 * @param   Dev                    Device Handle
 * @param   SequenceStepId         Sequence step identifier.
 * @param   pSequenceStepEnabled   Out parameter reporting if the sequence step
 *                                 is enabled {0=Off,1=On}.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS  Error SequenceStepId parameter not
 *                                       supported.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetSequenceStepEnable(VL53L1_DEV Dev,
	VL53L1_SequenceStepId SequenceStepId, uint8_t *pSequenceStepEnabled);


/** @} VL53L1_sequencestep_group */



/** @defgroup VL53L1_measurement_group VL53L1 Measurement Functions
 *  @brief    Functions used for the measurements
 *  @{
 */

/**
 * @brief Start device measurement
 *
 * @details Started measurement will depend on preset parameters set through
 * @a VL53L1_SetPreseMode()
 * This function will change the VL53L1_State from VL53L1_STATE_IDLE to
 * VL53L1_STATE_RUNNING.
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @return  VL53L1_ERROR_NONE                  Success
 * @return  VL53L1_ERROR_MODE_NOT_SUPPORTED    This error occurs when
 * PresetMode programmed with @a VL53L1_SetPresetMode
 *
 * @return  VL53L1_ERROR_TIME_OUT    Time out on start measurement
 * @return  "Other error code"   See ::VL53L1_Error
 */
VL53L1_Error VL53L1_StartMeasurement(VL53L1_DEV Dev);

/**
 * @brief Stop device measurement
 *
 * @details Will set the device in standby mode at end of current measurement\n
 *          Not necessary in single mode as device shall return automatically
 *          in standby mode at end of measurement.
 *          This function will change the VL53L1_State from VL53L1_STATE_RUNNING
 *          to VL53L1_STATE_IDLE.
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @return  VL53L1_ERROR_NONE    Success
 * @return  "Other error code"   See ::VL53L1_Error
 */
VL53L1_Error VL53L1_StopMeasurement(VL53L1_DEV Dev);

/**
 * @brief Clear the Interrupt flag and start new measurement
 * *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @return  VL53L1_ERROR_NONE    Success
 * @return  "Other error code"   See ::VL53L1_Error
 */
VL53L1_Error VL53L1_ClearInterruptAndStartMeasurement(VL53L1_DEV Dev);

/**
 * @brief Return Measurement Data Ready
 *
 * @par Function Description
 * This function indicate that a measurement data is ready.
 * This function is used for non-blocking capture.
 *
 * @note This function Access to the device
 *
 * @param   Dev                    Device Handle
 * @param   pMeasurementDataReady  Pointer to Measurement Data Ready.
 * 0 = data not ready, 1 = data ready
 * @return  VL53L1_ERROR_NONE      Success
 * @return  "Other error code"     See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetMeasurementDataReady(VL53L1_DEV Dev,
	uint8_t *pMeasurementDataReady);

/**
 * @brief Wait for measurement data ready.
 * Blocking function.
 * Note that the timeout is given by:
 * VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS defined in def.h
 *
 *
 * @note This function Access to the device
 *
 * @param   Dev      Device Handle
 * @return  VL53L1_ERROR_NONE        Success
 * @return  VL53L1_ERROR_TIME_OUT In case of timeout
 */
VL53L1_Error VL53L1_WaitMeasurementDataReady(VL53L1_DEV Dev);


/**
 * @brief Retrieve the measurements from device for a given setup
 *
 * @par Function Description
 * Get data from last successful Ranging measurement
 * @warning this function will return only the first ROI data and only the
 * first object. For multi objects or multi ROI use:
 * @a Vl53L1_GetMultiRangingData.
 * In case of RANGING or MULTIZONES_SCANNING, only one output is given, this can
 * be selected with the help of @a VL53L1_SetOutputMode()
 *
 * @note This function Access to the device
 *
 * @note The first valid value returned by this function will have a range
 * status equal to VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK which means that
 * the data is valid but no wrap around check have been done. User should take
 * care about that.
 *
 * @param   Dev                      Device Handle
 * @param   pRangingMeasurementData  Pointer to the data structure to fill up.
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetRangingMeasurementData(VL53L1_DEV Dev,
	VL53L1_RangingMeasurementData_t *pRangingMeasurementData);

/**
 * @brief Retrieve all ROI's measurements from device for a given setup
 *
 * @par Function Description
 * Get data from last successful Ranging measurement
 * @warning USER should take care about  @a VL53L1_GetNumberOfROI()
 * before get data.
 * PAL will fill a NumberOfROI times the corresponding data
 * structure used in the measurement function.
 *
 * @note This function Access to the device
 *
 * @note The first valid value returned by this function will have a range
 * status equal to VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK which means that
 * the data is valid but no wrap around check have been done. User should take
 * care about that.
 *
 * @param   Dev                      Device Handle
 * @param   pMultiRangingData        Pointer to the data structure to fill up.
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetMultiRangingData(VL53L1_DEV Dev,
		VL53L1_MultiRangingData_t *pMultiRangingData);

/** @} VL53L1_measurement_group */

/** @defgroup VL53L1_Calibration_group VL53L1 Calibration Functions
 *  @brief    Functions used for Calibration
 *  @{
 */


/**
 * @brief Performs Reference Spad Management
 *
 * @par Function Description
 * The reference SPAD initialization procedure determines the minimum amount
 * of reference spads to be enables to achieve a target reference signal rate
 * and should be performed once during initialization.
 *
 * @note This function Access to the device
 *
 * @param   Dev                          Device Handle
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
 */
VL53L1_Error VL53L1_PerformRefSpadManagement(VL53L1_DEV Dev);

/**
* @brief Enable/Disable Cross talk compensation feature
*
* Enable/Disable Cross Talk correction.
*
* @param   Dev                       Device Handle
* @param   XTalkCompensationEnable   Cross talk compensation
*  to be set 0 = disabled; other values than zero = enabled.
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
*/
VL53L1_Error VL53L1_SetXTalkCompensationEnable(VL53L1_DEV Dev,
uint8_t XTalkCompensationEnable);

/**
 * @brief Get Cross talk compensation rate enable
 *
 * Get if the Cross Talk is Enabled or Disabled.
 *
 * @note This function doesn't access to the device
 *
 * @param   Dev                        Device Handle
 * @param   pXTalkCompensationEnable   Pointer to the Cross talk compensation
 *  state 0=disabled or 1 = enabled
 * @return  VL53L1_ERROR_NONE        Success
 * @return  "Other error code"       See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetXTalkCompensationEnable(VL53L1_DEV Dev,
	uint8_t *pXTalkCompensationEnable);

/**
 * @brief Perform XTalk Calibration
 *
 * @details Perform a XTalk calibration of the Device.
 * This function will launch a ranging measurement, if interrupts
 * are enabled an interrupt will be done.
 * This function will clear the interrupt generated automatically.
 * This function will program a new value for the XTalk compensation
 * and it will enable the cross talk before exit.
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @note This function change the preset mode so a new preset mode should
 * be done after the call to this function
 *
 * @param   Dev                  Device Handle
 * @param   CalibrationOption    Select the Calibration to be run
 * @return  VL53L1_ERROR_NONE    Success
 * @return  "Other error code"   See ::VL53L1_Error
 */
VL53L1_Error VL53L1_PerformXTalkCalibration(VL53L1_DEV Dev,
		uint8_t CalibrationOption);

/**
* @brief Define the mode to be used for the offset calibration
*
* Define the mode to be used for the offset calibration. This function should
* be called before run the @a VL53L1_PerformOffsetCalibration()
*
* @param   Dev                       Device Handle
* @param   OffsetCalibrationMode     Offset Calibration Mode
*
* @return  VL53L1_ERROR_NONE         Success
* @return  "Other error code"        See ::VL53L1_Error
*/
VL53L1_Error VL53L1_SetOffsetCalibrationMode(VL53L1_DEV Dev,
		VL53L1_OffsetCalibrationModes OffsetCalibrationMode);


/**
 * @brief Perform Offset Calibration
 *
 * @details Perform a Offset calibration of the Device.
 * This function will launch a ranging measurement, if interrupts are
 * enabled interrupts will be done.
 * This function will program a new value for the Offset calibration value
 *
 * @warning This function is a blocking function
 *
 * @note This function Access to the device
 *
 * @param   Dev                  Device Handle
 * @param   CalDistanceMilliMeter     Calibration distance value used for the
 * offset compensation.
 * @param   CalReflectancePercent     Calibration Target reflectance @ 940nm
 * in percentage.
 *
 * @return  VL53L1_ERROR_NONE
 * @return  "Other error code"   See ::VL53L1_Error
 */
VL53L1_Error VL53L1_PerformOffsetCalibration(VL53L1_DEV Dev,
	int32_t CalDistanceMilliMeter,
	FixPoint1616_t CalReflectancePercent);

/**
 * @brief Sets the Calibration Data.
 *
 * @par Function Description
 * This function set all the Calibration Data issued from the functions
 * @a VL53L1_PerformRefSpadManagement(), @a VL53L1_PerformXTalkCalibration,
 * @a VL53L1_PerformOffsetCalibration()
 *
 * @note This function doesn't Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   *pCalibrationData            Pointer to Calibration data to be set.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  VL53L1_ERROR_INVALID_PARAMS  pCalibrationData points to an older
 * version of the inner structure. Need for support to convert its content.
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_SetCalibrationData(VL53L1_DEV Dev,
		VL53L1_CalibrationData_t *pCalibrationData);

/**
 * @brief Gets the Calibration Data.
 *
 * @par Function Description
 * This function get all the Calibration Data issued from the functions
 * @a VL53L1_PerformRefSpadManagement(), @a VL53L1_PerformXTalkCalibration,
 * @a VL53L1_PerformOffsetCalibration()
 *
 * @note This function doesn't Accesses the device
 *
 * @param   Dev                          Device Handle
 * @param   *pCalibrationData            pointer where to store Calibration
 *  data.
 * @return  VL53L1_ERROR_NONE            Success
 * @return  "Other error code"           See ::VL53L1_Error
 */
VL53L1_Error VL53L1_GetCalibrationData(VL53L1_DEV Dev,
		VL53L1_CalibrationData_t  *pCalibrationData);


/** @} VL53L1_Calibration_group */

/** @defgroup VL53L1_Thresholds_group VL53L1 IRQ Triggered events Functions
 *  @brief    Functions used to configure interrupt to be triggered only when
 *  a measurement satisfies some thresholds parameters
 *  @{
 */

/**
* @brief Configure the interrupt config, from the given structure
*
* @param[in]    Dev     : Device Handle
* @param[in]    pConfig : pointer to configuration structure
*/

VL53L1_Error VL53L1_SetThresholdConfig(VL53L1_DEV Dev,
		VL53L1_DetectionConfig_t *pConfig);

/**
* @brief Retrieves the interrupt config structure currently programmed
*                             into the API
*
* @param[in]    Dev     : Device Handle
* @param[out]   pConfig : pointer to configuration structure
*/

VL53L1_Error VL53L1_GetThresholdConfig(VL53L1_DEV Dev,
		VL53L1_DetectionConfig_t *pConfig);


/** @} VL53L1_Thresholds_group */


/** @} VL53L1_cut11_group */

#ifdef __cplusplus
}
#endif

#endif /* _VL53L1_API_H_ */
