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

#pragma once

#include <QObject>
#include <atomic>

#include "DeckLinkAPI.h"
#include "com_ptr.h"
#include "platform.h"

class DeckLinkProfileCallback : public QObject, public IDeckLinkProfileCallback
{
	Q_OBJECT

public:
	DeckLinkProfileCallback();
	virtual ~DeckLinkProfileCallback() = default;

	// IUnknown interface
	HRESULT	QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG	AddRef() override;
	ULONG	Release() override;

	// IDeckLinkProfileCallback interface
	HRESULT	ProfileChanging(IDeckLinkProfile *profileToBeActivated, dlbool_t streamsWillBeForcedToStop) override;
	HRESULT	ProfileActivated(IDeckLinkProfile *activatedProfile) override;

signals:
	void	profileChanging(com_ptr<IDeckLinkProfile> profileToBeActivated);
	void	profileActivated(com_ptr<IDeckLinkProfile> activatedProfile);

private:
	std::atomic<ULONG>			m_refCount;
};
