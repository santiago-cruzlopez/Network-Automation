/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#include <QFont>
#include "DeckLinkStatusDataTableModel.h"

using namespace std::placeholders;

using GetStatusDataFunc = std::function<QString(com_ptr<IDeckLinkStatus>&)>;

// Map of status IDs against getter function and display name
static const std::map<BMDDeckLinkStatusID, std::pair<GetStatusDataFunc, QString>> kStatusItems =
{
	{ bmdDeckLinkStatusDetectedVideoInputMode,				std::make_pair(std::bind(getStatusVideoInputMode, _1, bmdDeckLinkStatusDetectedVideoInputMode),				"Detected video input display mode") },
	{ bmdDeckLinkStatusDetectedVideoInputFormatFlags,		std::make_pair(getStatusDetectedVideoInputFormatFlags,														"Detected video input format flags") },
	{ bmdDeckLinkStatusDetectedVideoInputFieldDominance,	std::make_pair(std::bind(getStatusFieldDominance, _1, bmdDeckLinkStatusDetectedVideoInputFieldDominance),	"Detected video input field dominance") },
	{ bmdDeckLinkStatusDetectedVideoInputColorspace,		std::make_pair(std::bind(getStatusColorspace, _1, bmdDeckLinkStatusDetectedVideoInputColorspace),			"Detected video input colorspace") },
	{ bmdDeckLinkStatusDetectedVideoInputDynamicRange,		std::make_pair(std::bind(getStatusDynamicRange, _1, bmdDeckLinkStatusDetectedVideoInputDynamicRange),		"Detected video input dynamic range") },
	{ bmdDeckLinkStatusDetectedSDILinkConfiguration,		std::make_pair(std::bind(getStatusLinkConfiguration, _1, bmdDeckLinkStatusDetectedSDILinkConfiguration),	"Detected SDI video input link width") },
	{ bmdDeckLinkStatusCurrentVideoInputMode,				std::make_pair(std::bind(getStatusVideoInputMode, _1, bmdDeckLinkStatusCurrentVideoInputMode),				"Video input display mode") },
	{ bmdDeckLinkStatusCurrentVideoInputPixelFormat,		std::make_pair(std::bind(getStatusPixelFormat, _1, bmdDeckLinkStatusCurrentVideoInputPixelFormat),			"Video input pixel format") },
	{ bmdDeckLinkStatusCurrentVideoInputFlags,				std::make_pair(std::bind(getStatusVideoFlags, _1, bmdDeckLinkStatusCurrentVideoInputFlags),					"Video input flags") },
	{ bmdDeckLinkStatusCurrentVideoOutputMode,				std::make_pair(std::bind(getStatusVideoOutputMode, _1, bmdDeckLinkStatusCurrentVideoOutputMode),			"Video output display mode") },
	{ bmdDeckLinkStatusCurrentVideoOutputFlags,				std::make_pair(std::bind(getStatusVideoFlags, _1, bmdDeckLinkStatusCurrentVideoOutputFlags),				"Video output flags") },
	{ bmdDeckLinkStatusPCIExpressLinkWidth,					std::make_pair(std::bind(getStatusInt, _1, bmdDeckLinkStatusPCIExpressLinkWidth),							"PCIe link width") },
	{ bmdDeckLinkStatusPCIExpressLinkSpeed,					std::make_pair(std::bind(getStatusInt, _1, bmdDeckLinkStatusPCIExpressLinkSpeed),							"PCIe link speed") },
	{ bmdDeckLinkStatusLastVideoOutputPixelFormat,			std::make_pair(std::bind(getStatusPixelFormat, _1, bmdDeckLinkStatusLastVideoOutputPixelFormat),			"Video output pixel format") },
	{ bmdDeckLinkStatusReferenceSignalMode,					std::make_pair(std::bind(getStatusVideoOutputMode, _1, bmdDeckLinkStatusReferenceSignalMode),				"Detected reference video mode") },
	{ bmdDeckLinkStatusBusy,								std::make_pair(getStatusBusy,																				"Busy state") },
	{ bmdDeckLinkStatusVideoInputSignalLocked,				std::make_pair(std::bind(getStatusFlag, _1, bmdDeckLinkStatusVideoInputSignalLocked),						"Video input locked") },
	{ bmdDeckLinkStatusReferenceSignalLocked,				std::make_pair(std::bind(getStatusFlag, _1, bmdDeckLinkStatusReferenceSignalLocked),						"Reference input locked") },
	{ bmdDeckLinkStatusReferenceSignalFlags,				std::make_pair(std::bind(getStatusVideoFlags, _1, bmdDeckLinkStatusReferenceSignalFlags),					"Reference input video flags") },
	{ bmdDeckLinkStatusInterchangeablePanelType,			std::make_pair(getStatusPanelType,																			"Panel Installed") },
	{ bmdDeckLinkStatusReceivedEDID,						std::make_pair(std::bind(getStatusBytes, _1, bmdDeckLinkStatusReceivedEDID),								"Received EDID of connected HDMI sink") },
	{ bmdDeckLinkStatusDeviceTemperature,					std::make_pair(std::bind(getStatusInt, _1, bmdDeckLinkStatusDeviceTemperature),								"On-board temperature (°C)") },
};

DeckLinkNotificationCallback::DeckLinkNotificationCallback() :
	m_refCount(1)
{
}

// IUnknown methods

HRESULT DeckLinkNotificationCallback::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT result = S_OK;

	if (ppv == nullptr)
		return E_POINTER;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
	}
	else if (iid == IID_IDeckLinkNotificationCallback)
	{
		*ppv = static_cast<IDeckLinkNotificationCallback*>(this);
		AddRef();
	}
	else
	{
		*ppv = nullptr;
		result = E_NOINTERFACE;
	}

	return result;
}

ULONG DeckLinkNotificationCallback::AddRef()
{
	return ++m_refCount;
}

ULONG DeckLinkNotificationCallback::Release()
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

// IDeckLinkNotificationCallback

HRESULT DeckLinkNotificationCallback::Notify(BMDNotifications topic, uint64_t param1, uint64_t /*param2*/)
{
	if (topic == bmdStatusChanged)
		emit statusChanged(static_cast<BMDDeckLinkStatusID>(param1));

	else if (topic == bmdPreferencesChanged)
		emit preferencesChanged();

	return S_OK;
}

DeckLinkStatusDataTableModel::DeckLinkStatusDataTableModel(QObject* parent) :
	QAbstractTableModel(parent)
{
	m_delegate = make_com_ptr<DeckLinkNotificationCallback>();

	connect(m_delegate.get(), &DeckLinkNotificationCallback::statusChanged, this, &DeckLinkStatusDataTableModel::statusChanged, Qt::QueuedConnection);
}

IDeckLinkNotificationCallback* DeckLinkStatusDataTableModel::delegate()
{
	return m_delegate.get();
}

void DeckLinkStatusDataTableModel::reset(com_ptr<IDeckLink>& deckLink)
{
	// Called whenever new device is selected
	beginResetModel();
	m_statusData.clear();

	m_deckLinkStatus = com_ptr<IDeckLinkStatus>(IID_IDeckLinkStatus, deckLink);
	if (!m_deckLinkStatus)
	{
		endResetModel();
		return;
	}

	// Scan all status IDs and fill any non-empty data values
	for (auto& item : kStatusItems)
	{
		GetStatusDataFunc	getStatusDataFunc;
		QString				statusData;
		std::tie(getStatusDataFunc, std::ignore) = item.second;

		statusData = getStatusDataFunc(m_deckLinkStatus);
		if (!statusData.isEmpty())
			m_statusData.push_back(std::make_pair(item.first, statusData));
	}

	endResetModel();
}


int DeckLinkStatusDataTableModel::rowCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent)
	return static_cast<int>(m_statusData.size());
}

int DeckLinkStatusDataTableModel::columnCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	return static_cast<int>(StatusDataTableHeader::Count);
}

QVariant DeckLinkStatusDataTableModel::data(const QModelIndex& index, int role) const
{
	if (role == Qt::DisplayRole)
	{
		int rowIndex = 0;
		for (auto iter = m_statusData.begin(); iter != m_statusData.end(); ++iter)
		{
			if (rowIndex++ == index.row())
			{
				if (index.column() == static_cast<int>(StatusDataTableHeader::Item))
				{
					auto search = kStatusItems.find(iter->first);
					if (search != kStatusItems.end())
					{
						QString statusIDName;
						std::tie(std::ignore, statusIDName) = search->second;
						return statusIDName;
					}
				}
				else if (index.column() == static_cast<int>(StatusDataTableHeader::Value))
				{
					return iter->second;
				}
			}
		}
	}

	return QVariant();
}

QVariant DeckLinkStatusDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
		return QVariant();

	if (role == Qt::DisplayRole)
	{
		if (section == static_cast<int>(StatusDataTableHeader::Item))
			return "Item";
		else if (section == static_cast<int>(StatusDataTableHeader::Value))
			return "Value";
	}
	else if (role == Qt::FontRole)
	{
		QFont headerFont;
		headerFont.setBold(true);
		return headerFont;
	}

	return QVariant();
}

void DeckLinkStatusDataTableModel::statusChanged(BMDDeckLinkStatusID statusID)
{
	GetStatusDataFunc	getStatusDataFunc;
	QString				statusData;

	// Get updated status value
	auto search = kStatusItems.find(statusID);
	if (search == kStatusItems.end())
		return;

	std::tie(getStatusDataFunc, std::ignore) = search->second;
	statusData = getStatusDataFunc(m_deckLinkStatus);

	// Check if status ID was already valid.
	auto iter = std::find_if(m_statusData.begin(), m_statusData.end(),
		[&statusID](const std::pair<BMDDeckLinkStatusID, QString> statusItem) { return statusItem.first == statusID; });

	if (iter != m_statusData.end())
	{
		// Status ID is existing, get index
		int statusDataIndex = std::distance(m_statusData.begin(), iter);

		if (statusData.isEmpty())
		{
			// Status data has become invalid, remove from vector
			beginRemoveRows(QModelIndex(), statusDataIndex, statusDataIndex);
			m_statusData.erase(iter);
			endRemoveRows();
		}
		else
		{
			// Update stored status data in vector
			iter->second = statusData;
			emit dataChanged(index(statusDataIndex, static_cast<int>(StatusDataTableHeader::Value)), index(statusDataIndex, static_cast<int>(StatusDataTableHeader::Value)));
		}
	}
	else if (!statusData.isEmpty())
	{
		// New status ID data, add new row to vector
		beginInsertRows(QModelIndex(), m_statusData.size(), m_statusData.size());
		m_statusData.push_back(std::make_pair(statusID, statusData));
		endInsertRows();
	}
}

// Status data getter functions

QString getStatusFlag(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	dlbool_t statusFlag;

	if (deckLinkStatus->GetFlag(id, &statusFlag) != S_OK)
		return QString();

	return statusFlag ? "Yes" : "No";
}

QString getStatusBytes(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	uint32_t				bytesSize = 0;
	std::vector<int8_t>		bytes;
	QStringList				bytesStringList;

	// Get required size of buffer
	if (deckLinkStatus->GetBytes(id, nullptr, &bytesSize) != S_OK)
		return QString();

	bytes.resize(bytesSize);

	if (deckLinkStatus->GetBytes(id, bytes.data(), &bytesSize) != S_OK)
		return QString();

	// Convert each byte to QStringList to display as hex
	for (auto byte : bytes)
		bytesStringList << QString("%1").arg(byte, 2, 16, QChar('0'));

	return bytesStringList.join(" ");
}

QString getStatusInt(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	return QString::number(statusInt);
}

QString getStatusPanelType(com_ptr<IDeckLinkStatus>& deckLinkStatus)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(bmdDeckLinkStatusInterchangeablePanelType, &statusInt) != S_OK)
		return QString();

	switch ((BMDPanelType)statusInt)
	{
		case bmdPanelNotDetected:
			return "No panel detected";

		case bmdPanelTeranexMiniSmartPanel:
			return "Teranex Mini panel detected";

		default:
			return QString();
	}
}

QString getStatusBusy(com_ptr<IDeckLinkStatus>& deckLinkStatus)
{
	QStringList busyStringList;
	int64_t statusInt;

	if (deckLinkStatus->GetInt(bmdDeckLinkStatusBusy, &statusInt) != S_OK)
		return QString();

	if ((BMDDeviceBusyState)statusInt & bmdDeviceCaptureBusy)
		busyStringList << "Capture active";

	if ((BMDDeviceBusyState)statusInt & bmdDevicePlaybackBusy)
		busyStringList << "Playback active";

	if ((BMDDeviceBusyState)statusInt & bmdDeviceSerialPortBusy)
		busyStringList << "Serial port active";

	return busyStringList.size() ? busyStringList.join("\n") : "Inactive";
}

QString getStatusVideoOutputMode(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t							statusInt;
	com_ptr<IDeckLinkOutput>		deckLinkOutput(IID_IDeckLinkOutput, deckLinkStatus);
	com_ptr<IDeckLinkDisplayMode>	displayMode;
	dlstring_t						displayModeName;
	QString							displayModeString;

	if (!deckLinkOutput)
		return QString();

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	if ((BMDDisplayMode)statusInt == bmdModeUnknown)
		return QString();

	if (deckLinkOutput->GetDisplayMode((BMDDisplayMode)statusInt, displayMode.releaseAndGetAddressOf()) != S_OK)
		return QString();

	if (displayMode->GetName(&displayModeName) != S_OK)
		return QString();

	displayModeString = DlToQString(displayModeName);
	DeleteString(displayModeName);
	return displayModeString;
}

QString getStatusPixelFormat(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	switch ((BMDPixelFormat)statusInt)
	{
		case bmdFormat8BitYUV:
			return "8-bit YUV (UYVY)";

		case bmdFormat10BitYUV:
			return "10-bit YUV (v210)";

		case bmdFormat8BitARGB:
			return "8-bit ARGB";

		case bmdFormat8BitBGRA:
			return "8-bit BGRA";

		case bmdFormat10BitRGB:
			return "10-bit RGB (r210)";

		case bmdFormat12BitRGB:
			return "12-bit RGB Big-Endian (R12B)";

		case bmdFormat12BitRGBLE:
			return "12-bit RGB Little-Endian (R12L)";

		case bmdFormat10BitRGBX:
			return "10-bit RGB Big-Endian (R10b)";

		case bmdFormat10BitRGBXLE:
			return "10-bit RGB Little-Endian (R10l)";

		case bmdFormatH265:
			return "H.265 Encoded Video Data";

		case bmdFormatDNxHR:
			return "DNxHR Encoded Video Data";

		default:
			return QString();
	}
}

QString getStatusVideoFlags(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	QStringList busyStringList;
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	if ((BMDDeckLinkVideoStatusFlags)statusInt & bmdDeckLinkVideoStatusPsF)
		busyStringList << "Progressive frames are PsF";

	if ((BMDDeckLinkVideoStatusFlags)statusInt & bmdDeckLinkVideoStatusDualStream3D)
		busyStringList << "Dual-stream 3D video";

	return busyStringList.join("\n");
}

QString getStatusDetectedVideoInputFormatFlags(com_ptr<IDeckLinkStatus>& deckLinkStatus)
{
	QStringList busyStringList;
	int64_t statusInt;

	if (deckLinkStatus->GetInt(bmdDeckLinkStatusDetectedVideoInputFormatFlags, &statusInt) != S_OK)
		return QString();

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInputYCbCr422)
		busyStringList << "YCbCr 4:2:2";

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInputRGB444)
		busyStringList << "RGB 4:4:4";

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInputDualStream3D)
		busyStringList << "Dual-stream 3D";

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInput12BitDepth)
		busyStringList << "12-bit depth";

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInput10BitDepth)
		busyStringList << "10-bit depth";

	if ((BMDDetectedVideoInputFormatFlags)statusInt & bmdDetectedVideoInput8BitDepth)
		busyStringList << "8-bit depth";

	return busyStringList.join("\n");
}

QString getStatusVideoInputMode(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t							statusInt;
	com_ptr<IDeckLinkInput>			deckLinkInput(IID_IDeckLinkInput, deckLinkStatus);
	com_ptr<IDeckLinkDisplayMode>	displayMode;
	dlstring_t						displayModeName;
	QString							displayModeString;

	if (!deckLinkInput)
		return QString();

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	if ((BMDDisplayMode)statusInt == bmdModeUnknown)
		return QString();

	if (deckLinkInput->GetDisplayMode((BMDDisplayMode)statusInt, displayMode.releaseAndGetAddressOf()) != S_OK)
		return QString();

	if (displayMode->GetName(&displayModeName) != S_OK)
		return QString();

	displayModeString = DlToQString(displayModeName);
	DeleteString(displayModeName);
	return displayModeString;
}

QString getStatusDynamicRange(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	switch ((BMDDynamicRange)statusInt)
	{
		case bmdDynamicRangeSDR:
			return "SDR";

		case bmdDynamicRangeHDRStaticPQ:
			return "PQ (ST 2084)";

		case bmdDynamicRangeHDRStaticHLG:
			return "HLG";

		default:
			return QString();
	}
}

QString getStatusFieldDominance(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	switch ((BMDFieldDominance)statusInt)
	{
		case bmdUpperFieldFirst:
			return "Upper field first";

		case bmdLowerFieldFirst:
			return "Lower field first";

		case bmdProgressiveFrame:
			return "Progressive frame";

		case bmdProgressiveSegmentedFrame:
			return "Progressive segmented frame";

		default:
			return QString();
	}
}

QString getStatusColorspace(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	switch ((BMDColorspace)statusInt)
	{
		case bmdColorspaceRec601:
			return "Rec.601";

		case bmdColorspaceRec709:
			return "Rec.709";

		case bmdColorspaceRec2020:
			return "Rec.2020";

		default:
			return QString();
	}
}

QString getStatusLinkConfiguration(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id)
{
	int64_t statusInt;

	if (deckLinkStatus->GetInt(id, &statusInt) != S_OK)
		return QString();

	switch ((BMDLinkConfiguration)statusInt)
	{
		case bmdLinkConfigurationSingleLink:
			return "Single-link";

		case bmdLinkConfigurationDualLink:
			return "Dual-link";

		case bmdLinkConfigurationQuadLink:
			return "Quad-link";

		default:
			return QString();
	}
}
