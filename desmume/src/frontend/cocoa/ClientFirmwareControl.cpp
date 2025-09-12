/*
	Copyright (C) 2025 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ClientFirmwareControl.h"

#include "../../firmware.h"
#undef BOOL


ClientFirmwareControl::ClientFirmwareControl()
{
	_macAddressSelection = FirmwareCfgMACAddrSetID_Firmware;
	
	// First, get the default firmware config.
	_internalData = (FirmwareConfig *)malloc(sizeof(FirmwareConfig));
	NDS_GetDefaultFirmwareConfig(*_internalData);
	
	srand((uint32_t)time(NULL));
	
	// Generate a random firmware MAC address and its associated string.
	const uint32_t defaultFirmwareMACAddressValue = (uint32_t)_internalData->MACAddress[2] | ((uint32_t)_internalData->MACAddress[3] << 8) | ((uint32_t)_internalData->MACAddress[4] << 16) | ((uint32_t)_internalData->MACAddress[5] << 24);
	
	do
	{
		_firmwareMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		_firmwareMACAddressValue = (_firmwareMACAddressValue << 8) | 0x000000BF;
	} while ( (_firmwareMACAddressValue == 0x000000BF) || (_firmwareMACAddressValue == 0xFFFFFFBF) || (_firmwareMACAddressValue == defaultFirmwareMACAddressValue) );
	
	_internalData->MACAddress[2] = (uint8_t)( _firmwareMACAddressValue        & 0x000000FF);
	_internalData->MACAddress[3] = (uint8_t)((_firmwareMACAddressValue >>  8) & 0x000000FF);
	_internalData->MACAddress[4] = (uint8_t)((_firmwareMACAddressValue >> 16) & 0x000000FF);
	_internalData->MACAddress[5] = (uint8_t)((_firmwareMACAddressValue >> 24) & 0x000000FF);
	
	memset(_macAddressString, '\0', sizeof(_macAddressString));
	ClientFirmwareControl::WriteMACAddressStringToBuffer(_internalData->MACAddress[3], _internalData->MACAddress[4], _internalData->MACAddress[5], &_macAddressString[0]);
	
	// Generate a random custom MAC address set and their associated strings.
	do
	{
		_customMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		_customMACAddressValue = (_customMACAddressValue << 8) | 0x000000BF;
	} while ( (_customMACAddressValue == 0x000000BF) || (_customMACAddressValue == 0xFFFFFFBF) || ((_customMACAddressValue & 0xF0FFFFFF) == (_firmwareMACAddressValue & 0xF0FFFFFF)) );
	
	const uint8_t customMAC4 = (_customMACAddressValue >>  8) & 0x000000FF;
	const uint8_t customMAC5 = (_customMACAddressValue >> 16) & 0x000000FF;
	const uint8_t customMAC6 = (_customMACAddressValue >> 24) & 0x000000F0;
	
	for (size_t i = 1; i <= 8; i++)
	{
		ClientFirmwareControl::WriteMACAddressStringToBuffer(customMAC4, customMAC5, customMAC6 + (uint8_t)i, &_macAddressString[18*i]);
	}
	
	// Generate the WFC User ID string.
	memset(_wfcUserIDString, '\0', sizeof(_wfcUserIDString));
	const uint64_t wfcUserIDValue = (uint64_t)_internalData->WFCUserID[0]        |
	                               ((uint64_t)_internalData->WFCUserID[1] <<  8) |
	                               ((uint64_t)_internalData->WFCUserID[2] << 16) |
	                               ((uint64_t)_internalData->WFCUserID[3] << 24) |
	                               ((uint64_t)_internalData->WFCUserID[4] << 32) |
	                               ((uint64_t)_internalData->WFCUserID[5] << 40);
	ClientFirmwareControl::WriteWFCUserIDStringToBuffer(wfcUserIDValue, _wfcUserIDString);
	
	// Generate the subnet mask strings.
	memset(_subnetMaskString, '\0', sizeof(_subnetMaskString));
	
	uint32_t subnetMaskValue = (_internalData->subnetMask_AP1 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP1));
	snprintf(&_subnetMaskString[0],  16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	
	subnetMaskValue = (_internalData->subnetMask_AP2 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP2));
	snprintf(&_subnetMaskString[16], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	
	subnetMaskValue = (_internalData->subnetMask_AP3 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP3));
	snprintf(&_subnetMaskString[32], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
}

ClientFirmwareControl::~ClientFirmwareControl()
{
	free(this->_internalData);
	this->_internalData = NULL;
}

void ClientFirmwareControl::WriteMACAddressStringToBuffer(const uint8_t mac4, const uint8_t mac5, const uint8_t mac6, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	snprintf(stringBuffer, 18, "00:09:BF:%02X:%02X:%02X", mac4, mac5, mac6);
}

void ClientFirmwareControl::WriteMACAddressStringToBuffer(const uint32_t macAddressValue, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000FF;
	
	ClientFirmwareControl::WriteMACAddressStringToBuffer(mac4, mac5, mac6, stringBuffer);
}

void ClientFirmwareControl::WriteWFCUserIDStringToBuffer(const uint64_t wfcUserIDValue, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	const unsigned long long wfcUserIDValuePrint = (wfcUserIDValue & 0x000007FFFFFFFFFFULL) * 1000ULL;
	const unsigned long long wfcUserIDValuePrint1 =  wfcUserIDValuePrint / 1000000000000ULL;
	const unsigned long long wfcUserIDValuePrint2 = (wfcUserIDValuePrint / 100000000ULL) - (wfcUserIDValuePrint1 * 10000ULL);
	const unsigned long long wfcUserIDValuePrint3 = (wfcUserIDValuePrint / 10000ULL) - (wfcUserIDValuePrint1 * 100000000ULL) - (wfcUserIDValuePrint2 * 10000ULL);
	const unsigned long long wfcUserIDValuePrint4 =  wfcUserIDValuePrint - (wfcUserIDValuePrint1 * 1000000000000ULL) - (wfcUserIDValuePrint2 * 100000000ULL) - (wfcUserIDValuePrint3 * 10000ULL);
	snprintf(stringBuffer, 20, "%04llu-%04llu-%04llu-%04llu", wfcUserIDValuePrint1, wfcUserIDValuePrint2, wfcUserIDValuePrint3, wfcUserIDValuePrint4);
}

const FirmwareConfig& ClientFirmwareControl::GetFirmwareConfig()
{
	return *this->_internalData;
}

uint32_t ClientFirmwareControl::GenerateRandomMACValue()
{
	uint32_t randomMACAddressValue = 0;
	
	do
	{
		randomMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		randomMACAddressValue = (randomMACAddressValue << 8) | 0x000000BF;
	} while ( (randomMACAddressValue == 0x000000BF) || (randomMACAddressValue == 0xFFFFFFBF) || ((randomMACAddressValue & 0xF0FFFFFF) == (this->_firmwareMACAddressValue & 0xF0FFFFFF)) || ((randomMACAddressValue & 0xF0FFFFFF) == (this->_customMACAddressValue & 0xF0FFFFFF)) );
	
	return randomMACAddressValue;
}

FirmwareCfgMACAddrSetID ClientFirmwareControl::GetMACAddressSelection()
{
	return this->_macAddressSelection;
}

void ClientFirmwareControl::SetMACAddressSelection(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware:
			this->_macAddressSelection = FirmwareCfgMACAddrSetID_Firmware;
			this->_internalData->MACAddress[0] = 0x00;
			this->_internalData->MACAddress[1] = 0x09;
			this->_internalData->MACAddress[2] = (uint8_t)( this->_firmwareMACAddressValue        & 0x000000FF);
			this->_internalData->MACAddress[3] = (uint8_t)((this->_firmwareMACAddressValue >>  8) & 0x000000FF);
			this->_internalData->MACAddress[4] = (uint8_t)((this->_firmwareMACAddressValue >> 16) & 0x000000FF);
			this->_internalData->MACAddress[5] = (uint8_t)((this->_firmwareMACAddressValue >> 24) & 0x000000FF);
			break;
			
		case FirmwareCfgMACAddrSetID_Custom1:
		case FirmwareCfgMACAddrSetID_Custom2:
		case FirmwareCfgMACAddrSetID_Custom3:
		case FirmwareCfgMACAddrSetID_Custom4:
		case FirmwareCfgMACAddrSetID_Custom5:
		case FirmwareCfgMACAddrSetID_Custom6:
		case FirmwareCfgMACAddrSetID_Custom7:
		case FirmwareCfgMACAddrSetID_Custom8:
		{
			this->_macAddressSelection = addressSetID;
			this->_internalData->MACAddress[0] = 0x00;
			this->_internalData->MACAddress[1] = 0x09;
			this->_internalData->MACAddress[2] = (uint8_t)( this->_customMACAddressValue        & 0x000000FF);
			this->_internalData->MACAddress[3] = (uint8_t)((this->_customMACAddressValue >>  8) & 0x000000FF);
			this->_internalData->MACAddress[4] = (uint8_t)((this->_customMACAddressValue >> 16) & 0x000000FF);
			this->_internalData->MACAddress[5] = (uint8_t)((this->_customMACAddressValue >> 24) & 0x000000F0);
			
			switch (addressSetID)
			{
				case FirmwareCfgMACAddrSetID_Custom1: this->_internalData->MACAddress[5] += 1; break;
				case FirmwareCfgMACAddrSetID_Custom2: this->_internalData->MACAddress[5] += 2; break;
				case FirmwareCfgMACAddrSetID_Custom3: this->_internalData->MACAddress[5] += 3; break;
				case FirmwareCfgMACAddrSetID_Custom4: this->_internalData->MACAddress[5] += 4; break;
				case FirmwareCfgMACAddrSetID_Custom5: this->_internalData->MACAddress[5] += 5; break;
				case FirmwareCfgMACAddrSetID_Custom6: this->_internalData->MACAddress[5] += 6; break;
				case FirmwareCfgMACAddrSetID_Custom7: this->_internalData->MACAddress[5] += 7; break;
				case FirmwareCfgMACAddrSetID_Custom8: this->_internalData->MACAddress[5] += 8; break;
				default: break;
			}
			
			break;
		}
			
		default:
			break;
	}
}

uint32_t ClientFirmwareControl::GetMACAddressValue(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware: return this->_firmwareMACAddressValue;
		case FirmwareCfgMACAddrSetID_Custom1:  return this->_customMACAddressValue + 0x01000000;
		case FirmwareCfgMACAddrSetID_Custom2:  return this->_customMACAddressValue + 0x02000000;
		case FirmwareCfgMACAddrSetID_Custom3:  return this->_customMACAddressValue + 0x03000000;
		case FirmwareCfgMACAddrSetID_Custom4:  return this->_customMACAddressValue + 0x04000000;
		case FirmwareCfgMACAddrSetID_Custom5:  return this->_customMACAddressValue + 0x05000000;
		case FirmwareCfgMACAddrSetID_Custom6:  return this->_customMACAddressValue + 0x06000000;
		case FirmwareCfgMACAddrSetID_Custom7:  return this->_customMACAddressValue + 0x07000000;
		case FirmwareCfgMACAddrSetID_Custom8:  return this->_customMACAddressValue + 0x08000000;
		default: break;
	}
	
	return 0;
}

const char* ClientFirmwareControl::GetMACAddressString(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware: return &this->_macAddressString[18*0];
		case FirmwareCfgMACAddrSetID_Custom1:  return &this->_macAddressString[18*1];
		case FirmwareCfgMACAddrSetID_Custom2:  return &this->_macAddressString[18*2];
		case FirmwareCfgMACAddrSetID_Custom3:  return &this->_macAddressString[18*3];
		case FirmwareCfgMACAddrSetID_Custom4:  return &this->_macAddressString[18*4];
		case FirmwareCfgMACAddrSetID_Custom5:  return &this->_macAddressString[18*5];
		case FirmwareCfgMACAddrSetID_Custom6:  return &this->_macAddressString[18*6];
		case FirmwareCfgMACAddrSetID_Custom7:  return &this->_macAddressString[18*7];
		case FirmwareCfgMACAddrSetID_Custom8:  return &this->_macAddressString[18*8];
		default: break;
	}
	
	return "";
}

uint32_t ClientFirmwareControl::GetSelectedMACAddressValue()
{
	const uint32_t selectedMACAddressValue = (uint32_t)this->_internalData->MACAddress[2] | ((uint32_t)this->_internalData->MACAddress[3] << 8) | ((uint32_t)this->_internalData->MACAddress[4] << 16) | ((uint32_t)this->_internalData->MACAddress[5] << 24);
	return selectedMACAddressValue;
}

uint64_t ClientFirmwareControl::GetSelectedWFCUserID64()
{
	const uint64_t selectedUserID64 = (uint64_t)this->_internalData->WFCUserID[0] |
	                                 ((uint64_t)this->_internalData->WFCUserID[1] <<  8) |
	                                 ((uint64_t)this->_internalData->WFCUserID[2] << 16) |
	                                 ((uint64_t)this->_internalData->WFCUserID[3] << 24) |
	                                 ((uint64_t)this->_internalData->WFCUserID[4] << 32) |
	                                 ((uint64_t)this->_internalData->WFCUserID[5] << 40);
	return selectedUserID64;
}

const char* ClientFirmwareControl::GetSelectedMACAddressString()
{
	return this->GetMACAddressString(this->_macAddressSelection);
}

void ClientFirmwareControl::SetFirmwareMACAddressValue(uint32_t macAddressValue)
{
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000FF;
	
	this->SetFirmwareMACAddressValue(mac4, mac5, mac6);
}

void ClientFirmwareControl::SetFirmwareMACAddressValue(uint8_t mac4, uint8_t mac5, uint8_t mac6)
{
	this->_firmwareMACAddressValue = 0x000000BF | ((uint32_t)mac4 << 8) | ((uint32_t)mac5 << 16) | ((uint32_t)mac6 << 24);
	ClientFirmwareControl::WriteMACAddressStringToBuffer(mac4, mac5, mac6, &this->_macAddressString[0]);
	
	if (this->_macAddressSelection == FirmwareCfgMACAddrSetID_Firmware)
	{
		this->SetMACAddressSelection(FirmwareCfgMACAddrSetID_Firmware);
	}
}

void ClientFirmwareControl::SetCustomMACAddressValue(uint32_t macAddressValue)
{
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000F0;
	
	this->SetCustomMACAddressValue(mac4, mac5, mac6);
}

void ClientFirmwareControl::SetCustomMACAddressValue(uint8_t mac4, uint8_t mac5, uint8_t mac6)
{
	mac6 &= 0xF0;
	this->_customMACAddressValue = 0x000000BF | ((uint32_t)mac4 << 8) | ((uint32_t)mac5 << 16) | ((uint32_t)mac6 << 24);
	
	for (size_t i = 1; i <= 8; i++)
	{
		ClientFirmwareControl::WriteMACAddressStringToBuffer(mac4, mac5, mac6 + (uint8_t)i, &this->_macAddressString[18*i]);
	}
	
	if (this->_macAddressSelection != FirmwareCfgMACAddrSetID_Firmware)
	{
		this->SetMACAddressSelection(this->_macAddressSelection);
	}
}

uint8_t* ClientFirmwareControl::GetWFCUserID()
{
	return this->_internalData->WFCUserID;
}

void ClientFirmwareControl::SetWFCUserID(uint8_t *wfcUserID)
{
	const uint64_t wfcUserIDValue = (uint64_t)wfcUserID[0] |
	                               ((uint64_t)wfcUserID[1] <<  8) |
	                               ((uint64_t)wfcUserID[2] << 16) |
	                               ((uint64_t)wfcUserID[3] << 24) |
	                               ((uint64_t)wfcUserID[4] << 32) |
	                               ((uint64_t)wfcUserID[5] << 40);
	
	this->SetWFCUserID64(wfcUserIDValue);
}

uint64_t ClientFirmwareControl::GetWFCUserID64()
{
	const uint64_t userID = (uint64_t)this->_internalData->WFCUserID[0] |
	                       ((uint64_t)this->_internalData->WFCUserID[1] <<  8) |
	                       ((uint64_t)this->_internalData->WFCUserID[2] << 16) |
	                       ((uint64_t)this->_internalData->WFCUserID[3] << 24) |
	                       ((uint64_t)this->_internalData->WFCUserID[4] << 32) |
	                       ((uint64_t)this->_internalData->WFCUserID[5] << 40);
	return userID;
}

void ClientFirmwareControl::SetWFCUserID64(uint64_t wfcUserIDValue)
{
	if ( (wfcUserIDValue & 0x000007FFFFFFFFFFULL) != 0)
	{
		this->_internalData->WFCUserID[0] = (uint8_t)( wfcUserIDValue        & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[1] = (uint8_t)((wfcUserIDValue >>  8) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[2] = (uint8_t)((wfcUserIDValue >> 16) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[3] = (uint8_t)((wfcUserIDValue >> 24) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[4] = (uint8_t)((wfcUserIDValue >> 32) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[5] = (uint8_t)((wfcUserIDValue >> 40) & 0x00000000000000FFULL);
	}
	else
	{
		this->_internalData->WFCUserID[0] = 0;
		this->_internalData->WFCUserID[1] = 0;
		this->_internalData->WFCUserID[2] = 0;
		this->_internalData->WFCUserID[3] = 0;
		this->_internalData->WFCUserID[4] = 0;
		this->_internalData->WFCUserID[5] = 0;
	}
	
	ClientFirmwareControl::WriteWFCUserIDStringToBuffer(wfcUserIDValue, this->_wfcUserIDString);
}

const char* ClientFirmwareControl::GetWFCUserIDString()
{
	return this->_wfcUserIDString;
}

uint8_t ClientFirmwareControl::GetIPv4AddressPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4Address_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4Address_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4Address_AP3[addrPart];
	}
	
	return 0;
}

void ClientFirmwareControl::SetIPv4AddressPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4Address_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4Address_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4Address_AP3[addrPart] = addrValue; break;
	}
}

uint8_t ClientFirmwareControl::GetIPv4GatewayPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4Gateway_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4Gateway_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4Gateway_AP3[addrPart];
	}
	
	return 0;
}

void ClientFirmwareControl::SetIPv4GatewayPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4Gateway_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4Gateway_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4Gateway_AP3[addrPart] = addrValue; break;
	}
}

uint8_t ClientFirmwareControl::GetIPv4PrimaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4PrimaryDNS_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4PrimaryDNS_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4PrimaryDNS_AP3[addrPart];
	}
	
	return 0;
}

void ClientFirmwareControl::SetIPv4PrimaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4PrimaryDNS_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4PrimaryDNS_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4PrimaryDNS_AP3[addrPart] = addrValue; break;
	}
}

uint8_t ClientFirmwareControl::GetIPv4SecondaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4SecondaryDNS_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4SecondaryDNS_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4SecondaryDNS_AP3[addrPart];
	}
	
	return 0;
}

void ClientFirmwareControl::SetIPv4SecondaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4SecondaryDNS_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4SecondaryDNS_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4SecondaryDNS_AP3[addrPart] = addrValue; break;
	}
}

uint8_t ClientFirmwareControl::GetSubnetMask(FirmwareCfgAPID apid)
{
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->subnetMask_AP1;
		case FirmwareCfgAPID_AP2: return this->_internalData->subnetMask_AP2;
		case FirmwareCfgAPID_AP3: return this->_internalData->subnetMask_AP3;
	}
	
	return 0;
}

const char* ClientFirmwareControl::GetSubnetMaskString(FirmwareCfgAPID apid)
{
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return &this->_subnetMaskString[0];
		case FirmwareCfgAPID_AP2: return &this->_subnetMaskString[16];
		case FirmwareCfgAPID_AP3: return &this->_subnetMaskString[32];
	}
	
	return "";
}

void ClientFirmwareControl::SetSubnetMask(FirmwareCfgAPID apid, uint8_t subnetMaskShift)
{
	if (subnetMaskShift > 28)
	{
		subnetMaskShift = 28;
	}
	
	int subnetMaskCharIndex = -1;
	uint32_t subnetMaskValue = 0;
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1:
			this->_internalData->subnetMask_AP1 = subnetMaskShift;
			subnetMaskCharIndex = 0;
			break;
			
		case FirmwareCfgAPID_AP2:
			this->_internalData->subnetMask_AP2 = subnetMaskShift;
			subnetMaskCharIndex = 16;
			break;
			
		case FirmwareCfgAPID_AP3:
			this->_internalData->subnetMask_AP3 = subnetMaskShift;
			subnetMaskCharIndex = 32;
			break;
	}
	
	if ( (subnetMaskCharIndex >= 0) && (subnetMaskCharIndex <= 32) )
	{
		subnetMaskValue = (subnetMaskShift == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMaskShift));
		memset(&this->_subnetMaskString[subnetMaskCharIndex], '\0', 16 * sizeof(char));
		snprintf(&this->_subnetMaskString[subnetMaskCharIndex], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	}
}

int ClientFirmwareControl::GetConsoleType()
{
	return (int)this->_internalData->consoleType;
}

void ClientFirmwareControl::SetConsoleType(int type)
{
	this->_internalData->consoleType = (uint8_t)type;
}

uint16_t* ClientFirmwareControl::GetNicknameStringBuffer()
{
	return this->_internalData->nickname;
}

void ClientFirmwareControl::SetNicknameWithStringBuffer(uint16_t *buffer, size_t charLength)
{
	size_t i = 0;
	
	if (buffer != NULL)
	{
		if (charLength > MAX_FW_NICKNAME_LENGTH)
		{
			charLength = MAX_FW_NICKNAME_LENGTH;
		}
		
		for (; i < charLength; i++)
		{
			this->_internalData->nickname[i] = buffer[i];
		}
	}
	else
	{
		charLength = 0;
	}
	
	for (; i < MAX_FW_NICKNAME_LENGTH+1; i++)
	{
		this->_internalData->nickname[i] = 0;
	}
	
	this->_internalData->nicknameLength = charLength;
}

size_t ClientFirmwareControl::GetNicknameStringLength()
{
	return (size_t)this->_internalData->nicknameLength;
}

void ClientFirmwareControl::SetNicknameStringLength(size_t charLength)
{
	if (charLength > MAX_FW_NICKNAME_LENGTH)
	{
		charLength = MAX_FW_NICKNAME_LENGTH;
	}
	
	this->_internalData->nicknameLength = charLength;
}

uint16_t* ClientFirmwareControl::GetMessageStringBuffer()
{
	return this->_internalData->message;
}

void ClientFirmwareControl::SetMessageWithStringBuffer(uint16_t *buffer, size_t charLength)
{
	size_t i = 0;
	
	if (buffer != NULL)
	{
		if (charLength > MAX_FW_MESSAGE_LENGTH)
		{
			charLength = MAX_FW_MESSAGE_LENGTH;
		}
		
		for (; i < charLength; i++)
		{
			this->_internalData->message[i] = buffer[i];
		}
	}
	else
	{
		charLength = 0;
	}
	
	for (; i < MAX_FW_MESSAGE_LENGTH+1; i++)
	{
		this->_internalData->message[i] = 0;
	}
	
	this->_internalData->messageLength = charLength;
}

size_t ClientFirmwareControl::GetMessageStringLength()
{
	return (size_t)this->_internalData->messageLength;
}

void ClientFirmwareControl::SetMessageStringLength(size_t charLength)
{
	if (charLength > MAX_FW_MESSAGE_LENGTH)
	{
		charLength = MAX_FW_MESSAGE_LENGTH;
	}
	
	this->_internalData->messageLength = charLength;
}

int ClientFirmwareControl::GetFavoriteColorByID()
{
	return (int)this->_internalData->favoriteColor;
}

void ClientFirmwareControl::SetFavoriteColorByID(int colorID)
{
	this->_internalData->favoriteColor = (uint8_t)colorID;
}

int ClientFirmwareControl::GetBirthdayDay()
{
	return (int)this->_internalData->birthdayDay;
}

void ClientFirmwareControl::SetBirthdayDay(int day)
{
	if (day < 1)
	{
		day = 1;
	}
	
	switch (this->_internalData->birthdayMonth)
	{
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
		{
			if (day > 31) day = 31;
			break;
		}
			
		case 4:
		case 6:
		case 9:
		case 11:
		{
			if (day > 30) day = 30;
			break;
		}
			
		case 2:
		{
			if (day > 29) day = 29;
			break;
		}
			
		default:
			break;
	}
	
	this->_internalData->birthdayDay = (uint8_t)day;
}

int ClientFirmwareControl::GetBirthdayMonth()
{
	return (int)this->_internalData->birthdayMonth;
}

void ClientFirmwareControl::SetBirthdayMonth(int month)
{
	if (month < 1)
	{
		month = 1;
	}
	else if (month > 12)
	{
		month = 12;
	}
	
	this->_internalData->birthdayMonth = (uint8_t)month;
}

int ClientFirmwareControl::GetLanguageByID()
{
	return (int)this->_internalData->language;
}

void ClientFirmwareControl::SetLanguageByID(int languageID)
{
	this->_internalData->language = (uint8_t)languageID;
}

int ClientFirmwareControl::GetBacklightLevel()
{
	return (int)this->_internalData->backlightLevel;
}

void ClientFirmwareControl::SetBacklightLevel(int level)
{
	this->_internalData->backlightLevel = (uint8_t)level;
}
