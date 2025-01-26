//SMPTE 2022-7 with DTAPI

DtIpPars IpPars; // IP parameters used for Rx
// Set up destination IP address and port for the primary link
IpPars.m_Ip[0] = 192;
IpPars.m_Ip[1] = 168;
IpPars.m_Ip[2] = 1;
IpPars.m_Ip[3] = 100;
IpPars.m_Port = 12345;
// Set up destination IP address and port for the redundant link
IpPars.m_Ip2[0] = 192;
IpPars.m_Ip2[1] = 168;
IpPars.m_Ip2[2] = 2;
IpPars.m_Ip2[3] = 100;
IpPars.m_Port2 = 4321;
// Set the receive mode to SMPTE 2022-7
IpPars.m_Mode = DTAPI_IP_RX_2022_7;
 // Set the max. bitrate and max. skew
IpPars.m_IpProfile.m_Profile = DTAPI_IP_USER_DEFINED;
IpPars.m_IpProfile.m_MaxBitrate = 270000000; // bps
IpPars.m_IpProfile.m_MaxSkew = 40; // ms
// Set up other parameters
IpPars.m_NumTpPerIp = 7;
IpPars.m_Protocol = DTAPI_PROTO_AUTO;
IpPars.m_Flags = DTAPI_IP_V4;
// Set the parameters in the input channel.
// This assumes TsInp is a DtInpChannel object that has been
// attached to the hardware (e.g. DTA-2162)
TsInp.SetIpPars(IpPars);
