/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowHostServerCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"
#include <Utility.h>

/**
  Command syntax definition
**/
struct Command ShowHostServerCommand = {
  SHOW_VERB,                                                                                    //!< verb
  { {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty },
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired }},   //!< options
  {{SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
   {HOST_TARGET, L"", L"", TRUE, ValueEmpty }},                                                //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                                      //!< properties
  L"Show basic information about the host server.",                                             //!< help
  ShowHostServer
};

/**
  Execute the show host server command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOGOL function failure
**/
EFI_STATUS
ShowHostServer(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  HOST_SERVER_INFO HostServerinfo;
  BOOLEAN IsMixedSku;
  BOOLEAN IsSkuViolation;
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  BOOLEAN ShowAll = FALSE;
  CHAR16 *pDisplayValues = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** if the all option was specified **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }
  /** if the display option was specified **/
  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    DisplayOptionSet = TRUE;
  }
  else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      DisplayOptionSet = TRUE;
    }
  }

  /** make sure they didn't specify both the all and display options **/
  if (AllOptionSet && DisplayOptionSet) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Options used together");
    Print(FORMAT_STR, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    goto Finish;
  }

  ShowAll = (!AllOptionSet && !DisplayOptionSet) || AllOptionSet;

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = GetHostServerInfo(&HostServerinfo);
  if (EFI_ERROR(ReturnCode)) {
     Print(L"Error: GetHostServerInfo Failed\n");
     goto Finish;
  }


  ReturnCode = IsDimmsMixedSkuCfg(pNvmDimmConfigProtocol, &IsMixedSku, &IsSkuViolation);
  if (EFI_ERROR(ReturnCode)) {
     goto Finish;
  }

  SetDisplayInfo(L"HostServer", ListView);

  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_NAME_STR))) {
    Print(FORMAT_STR L": " FORMAT_STR_NL, DISPLAYED_NAME_STR, HostServerinfo.Name);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_OS_NAME_STR))) {
    Print(FORMAT_STR L": " FORMAT_STR_NL, DISPLAYED_OS_NAME_STR, HostServerinfo.OsName);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_OS_VERSION_STR))) {
    Print(FORMAT_STR L": " FORMAT_STR_NL, DISPLAYED_OS_VERSION_STR, HostServerinfo.OsVersion);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_MIXED_SKU_STR))) {
    Print(FORMAT_STR L": " FORMAT_STR_NL, DISPLAYED_MIXED_SKU_STR, (IsMixedSku == TRUE) ? L"1" : L"0");
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_SKU_VIOLATION_STR))) {
    Print(FORMAT_STR L": " FORMAT_STR_NL, DISPLAYED_SKU_VIOLATION_STR, (IsSkuViolation == TRUE) ? L"1" : L"0");
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pDisplayValues);
  return  ReturnCode;
}

/**
  Register the show memory resources command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowHostServerCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowHostServerCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS IsDimmsMixedSkuCfg(EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
      BOOLEAN *pIsMixedSku,
      BOOLEAN *pIsSkuViolation)
{
   EFI_STATUS ReturnCode = EFI_SUCCESS;
   UINT32 DimmCount = 0;
   DIMM_INFO *pDimms = NULL;
   UINT32 i;

   ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
   if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
      return ReturnCode;
   }

   pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);

   if (pDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      return ReturnCode;
   }
   /** retrieve the DIMM list **/
   ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
      DIMM_INFO_CATEGORY_PACKAGE_SPARING, pDimms);
   if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_ABORTED;
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
      goto Finish;
   }

   *pIsMixedSku = FALSE;
   *pIsSkuViolation = FALSE;

   for (i = 0; i < DimmCount; ++i)
   {
      if (pDimms[i].SKUViolation)
      {
         *pIsSkuViolation = TRUE;
      }

      if (NVM_SUCCESS != SkuComparison(pDimms[0].PackageSparingCapable,
                                       pDimms[i].PackageSparingCapable,
                                       pDimms[0].SkuInformation,
                                       pDimms[i].SkuInformation))
      {
         *pIsMixedSku = TRUE;
      }
   }

Finish:
   FreePool(pDimms);
   return ReturnCode;
}
